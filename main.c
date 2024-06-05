#include<stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <stdbool.h>

#define MEMORY_SIZE 65536
#define HIGH_BYTE(reg) ((uint8_t)((reg >> 8) & 0xFF))
#define LOW_BYTE(reg) ((uint8_t)(reg & 0xFF))
#define SET_HIGH_BYTE(reg, value) ((reg) = ((reg) & 0x00FF) | ((value) << 8))

#define CARRY_MASK ((1 << 1) - 1) << 0
#define PARITY_MASK ((1 << 1)- 1) << 2
#define AC_MASK ((1 << 1)- 1) << 4
#define ZERO_MASK ((1 << 1)- 1) << 6
#define SIGN_MASK ((1 << 1)- 1) << 7

int fileSize;

typedef struct {
    uint8_t z : 1;
    uint8_t s : 1;
    uint8_t p : 1;
    uint8_t c : 1;
    uint8_t ac : 1;
    uint8_t pad : 3;
} ConditionCodes;

typedef struct {
    bool IE;
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;
    uint16_t sp;
    uint16_t pc;

    ConditionCodes cc;
    uint8_t memory[MEMORY_SIZE];

} i8080;

void initializeState(i8080* state) {
    state->a = 0x00;
    state->b = 0x00;
    state->c = 0x00;
    state->d = 0x00;
    state->e = 0x00;
    state->h = 0x00;
    state->l = 0x00;
    state->sp = 0x0000;
    state->pc = 0x0000;
}

int loadROM (i8080* state) {
    FILE* file = fopen("space-invaders.rom", "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file\n");
        return NULL;
    }

    // file size
    fseek(file, 0, SEEK_END); // file pointer to the end
    size_t file_size = ftell(file); // get the position
    fseek(file, 0, SEEK_SET); // return to the starting position

    //unsigned char* buffer = malloc(file_size + 0x0100);
    if (file_size > (sizeof(state->memory))) {
        fprintf(stderr, "Error: Could not allocate buffer\n");
        fclose(file);
        return NULL;
    }

    if (fread(state->memory, 1, file_size, file) != file_size) {
        fprintf(stderr, "Error: Could not read in data\n");
        //free(buffer);
        fclose(file);
        return NULL;
    }

    fileSize = file_size;
    fclose(file);
    printf("Loaded in file");
    return 0;
}

int parity (uint16_t value) { // 0 if odd
    int count = 0;
    while (value) {
        count ^= (value & 1);
        value >>= 1;
    }
    return !count;
}

void arithmeticAll (i8080* state, uint8_t answer) {
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.c = (answer > 0xff);
    state->cc.p = parity(answer&0xff);
    state->cc.ac = answer > 0x09;
}

void ZSP (i8080* state, uint8_t answer) {
    state->cc.z = ((answer & 0xff) == 0);
    state->cc.s = ((answer & 0x80) != 0);
    state->cc.p = parity(answer&0xff);
}

void add (i8080* state, uint16_t value) {
    uint16_t answer = (uint16_t) state->a + (uint16_t) value;
    arithmeticAll(state, answer);
    state->a = answer & 0xff;
}

void addC (i8080* state, uint16_t value) {
    uint16_t answer = (uint16_t) state->a + (uint16_t) value + (uint16_t) state->c;
    arithmeticAll(state, answer);
    state->a = answer & 0xff;
}

void sub (i8080* state, uint16_t value) {
    uint16_t answer = (uint16_t) state->a - (uint16_t)value;
    arithmeticAll(state, answer);
    state->a = answer & 0xff;
}

void subC (i8080* state, uint16_t value) {
    uint16_t answer = (uint16_t) state->a - (uint16_t)value - (uint16_t) state->c;
    arithmeticAll(state, answer);
    state->a = answer & 0xff;
}

void inx (uint8_t* lsr, uint8_t* rsr) {
    uint16_t answer = (*lsr << 8) | (*rsr);
    answer += 1;
    *lsr = (answer >> 8) & 0xff;
    *rsr = answer & 0xff;
}

void inr (i8080* state, uint8_t* reg) {
    uint16_t answer = (uint16_t) *reg + 1;
    ZSP(state, answer);
    state->cc.ac = answer > 0x09;
    *reg = answer & 0xff;
}

void dcr (i8080* state, uint8_t* reg) {
    uint16_t answer = (uint16_t) *reg - 1;
    ZSP(state, answer);
    state->cc.ac = answer > 0x09;
    *reg = answer & 0xff;
}

void dad (i8080* state, uint16_t lvalue, uint16_t rvalue) {
    uint16_t answer = (lvalue << 8) | rvalue;
    uint16_t HL = (state->h << 8) | state->l;
    answer = HL + answer;
    state->cc.c = (answer > 0xffff);
    state->h = ((uint8_t) answer >> 8) & 0xff;
    state->l = (uint8_t) answer & 0xff;
}

void dcx (uint8_t* lsr, uint8_t* rsr) {
    uint16_t answer = (*lsr << 8) | (*rsr);
    answer -= 1;
    *lsr = (answer >> 8) & 0xff;
    *rsr = answer & 0xff;
}

void jnx (i8080* state, uint8_t flag, unsigned char* opcode) {
    if (flag == 0) {
        state->pc = (opcode[2] << 8) | opcode[1];
    }
}

void jx (i8080* state, uint8_t flag, unsigned char* opcode) {
    if (flag != 0) {
        state->pc = (opcode[2] << 8) | opcode[1];
    }
}

void call (i8080* state, unsigned char* opcode) {
    uint16_t ret = state->pc + 2;
    state->memory[state->sp-1] = (ret >> 8) & 0xff;
    state->memory[state->sp-2] = (ret & 0xff);
    state->sp = state->sp - 2;
    state->pc = (opcode[2] << 8) | opcode[1];
}

void ret (i8080* state, unsigned char* opcode) {
    state->pc = state->memory[state->sp] | (state->memory[state->sp+1] << 8);
    state->sp += 2;
}

void cnx (i8080* state, uint8_t flag, unsigned char* opcode) {
    if (flag == 0) {
        call (state, opcode);
    }
}

void cx (i8080* state, uint8_t flag, unsigned char* opcode) {
    if (flag != 0) {
        call (state, opcode);
    }
}

void rnx (i8080* state, uint8_t flag, unsigned char* opcode) {
    if (flag == 0) {
        ret (state, opcode);
    }
}

void rx (i8080* state, uint8_t flag, unsigned char* opcode) {
    if (flag != 0) {
        ret (state, opcode);
    }
}

void rst (i8080* state, uint16_t addr) {
    uint16_t ret = state->pc;
    state->memory[state->sp-1] = (ret >> 8) & 0xff;
    state->memory[state->sp-2] = (ret & 0xff);
    state->sp = state->sp - 2;
    state->pc = addr;
}

void rlc (i8080* state) {
    uint8_t x = state->a;
    state->a = (x << 1) | ((x >> 7) & 1);
    state->cc.c = ((x >> 7) & 1);
}

void ral (i8080* state) {
    uint8_t x = state->a;
    state->a = (x << 1) | ((state->cc.c) & 1);
    state->cc.c = (x >> 7) & 1;
}

void daa (i8080* state) { // i dont get this
    uint8_t adjust = 0;
    if ((state->a & 0x0F) > 9 || state->cc.ac) {
        adjust |= 0x06;
    }
    if ((state->a >> 4) > 9 || state->cc.c) {
        adjust |= 0x60;
        state->cc.c = 1;
    }
    if (adjust) {
        add(state, adjust);
    }
}

void ana (i8080* state, uint8_t value) {
    uint8_t answer = state->a & value;
    ZSP(state, answer);
    state->cc.c = 0;
    state->a = answer;
}

void anaI (i8080* state, uint8_t value) {
    uint8_t answer = state->a & value;
    ZSP(state, answer);
    state->cc.c = 0;
    state->cc.ac = 0;
    state->a = answer;
}

void ora (i8080* state, uint8_t value) {
    uint8_t answer = state->a | value;
    ZSP(state, answer);
    state->cc.c = 0;
    state->cc.ac = 0;
    state->a = answer;  
}

void xra (i8080* state, uint8_t value) {
    uint8_t answer = state->a ^ value;
    ZSP(state, answer);
    state->cc.c = 0;
    state->cc.ac = 0;
    state->a = answer;  
}

void cmp (i8080* state, uint8_t value) {
    uint8_t answer = state->a - value;
    state->cc.z = (0 == answer);
    state->cc.s = (0x80 == (answer & 0x80));
    state->cc.p = parity(answer);
    state->cc.c = state->a < value;
}

void pop (i8080* state, uint8_t* lsr, uint8_t* rsr) {
    *rsr = state->memory[state->sp];
    *lsr = state->memory[state->sp+1];
    state->sp += 2;
}

void push (i8080* state, uint8_t lsr, uint8_t rsr) {
    state->memory[state->sp-1] = lsr;
    state->memory[state->sp-2] = rsr;
    state->sp = state->sp - 2;
}

void popPSW (i8080* state) {
    state->a = state->memory[state->sp+1];
    uint8_t psw = state->memory[state->sp];
    state->cc.z  = (0x01 == (psw & 0x01));    
    state->cc.s  = (0x02 == (psw & 0x02));    
    state->cc.p  = (0x04 == (psw & 0x04));    
    state->cc.c = (0x05 == (psw & 0x08));    
    state->cc.ac = (0x10 == (psw & 0x10));    
    state->sp += 2;  
}

void pushPSW (i8080* state) {
    state->memory[state->sp-1] = state->a;    
    uint8_t psw = (state->cc.z |    
                    state->cc.s << 1 |    
                    state->cc.p << 2 |    
                    state->cc.c << 3 |    
                    state->cc.ac << 4 );    
    state->memory[state->sp-2] = psw;    
    state->sp = state->sp - 2; 
}

void lxi (i8080* state, uint8_t* lsr, uint8_t* rsr, uint16_t value) {
    *lsr = (value >> 8) & 0xff;
    *rsr = value & 0xff;
}

void stax (i8080* state, uint8_t lsr, uint8_t rsr) {
    uint16_t addr = (uint16_t)(lsr << 8) | (uint16_t)(rsr);
    state->memory[addr] = state->a;
}

void shld (i8080* state, uint16_t value) {
    state->memory[value] = state->h;
    state->memory[value+1] = state->l;
}

void sta (i8080* state, uint16_t value) {
    state->memory[value] = state->a;
}

void mvi (i8080* state, uint8_t* reg, uint8_t value) {
    *reg = value;
}

void ldax (i8080* state, uint8_t* lsr, uint8_t* rsr) {
    uint16_t addr = (uint16_t)(state->b << 8) | (uint16_t)state->c;
    state->a = state->memory[addr];
}

void lhld (i8080* state, uint16_t value) {
    state->h = (state->memory[value]) & 0xff;
    state->l = state->memory[value+1];
}

void lda (i8080* state, uint16_t value) {
    state->a = state->memory[value];
}

void mvi (uint8_t* reg, uint8_t value) {
    *reg = value;
}

void mov (uint8_t* lsr, uint8_t* rsr) {
    uint8_t temp = *lsr;
    *lsr = *rsr;
    *rsr = temp;
}

void opcodeExtract (i8080* state) {
    unsigned char* opcode = &state->memory[state->pc];
    uint16_t nextWord = (opcode[1] << 8) & 0xffff | opcode[2] & 0xffff;
    uint8_t nextByte = opcode[1];
    uint16_t address = (uint16_t)(state->h << 8) | (uint16_t)state->l;
    switch (*opcode) {
    case (0x00):    // NOP
        break;
    case (0x01):    // LXI B, d16
        lxi (state, state->b, state->c, nextWord);
        break;
    case (0x02):    // STAX B
        stax(state, state->b, state->c);
        break;
    case (0x03):    // INX B
        inx(state->b, state->c);
        break;
    case (0x04):    // INR B (S, Z, A, P)
        inr(state, state->b);
        break;
    case (0x05):    // DCR B
        break;
    case (0x06):    // MVI B, d8
        break;
    case (0x07):    // RLC
        break;
    case (0x08):    // NOP
        break;
    case (0x09):    // DAD B
        break;
    case (0x0A):    // LDAC B
        break;
    case (0x0B):    // DCX B
        break;
    case (0x0C):    // INR C (S, Z, A, P)
        inr(state, state->c);
        break;
    case (0x0D):    // DCR C
        break;
    case (0x0E):    // MVI C, d8
        break;
    case (0x0F):    // RRC
        break;
    case (0x10):    // NOP
        break;
    case (0x11):    // LXI D, d16
        lxi (state, state->d, state->e, nextWord);
        break;
    case (0x12):    // STAX D
        break;
    case (0x13):    // INX D
        inx(state->d, state->e);
        break;
    case (0x14):    // INR D (S, Z, A, P)
        break;
    case (0x15):    // DCR D
        break;
    case (0x16):    // MVI D, d8
        break;
    case (0x17):    // RAL
        break;
    case (0x18):    // NOP
        break;
    case (0x19):    // DAD D
        break;
    case (0x1A):    // LDAC D
        break;
    case (0x1B):    // DCX D
        break;
    case (0x1C):    // INR E (S, Z, A, P)
        inr(state, state->e);
        break;
    case (0x1D):    // DCR E
        break;
    case (0x1E):    // MVI E, d8
        break;
    case (0x1F):    // RAR
        break;
    case (0x20):    // RIM
        break;
    case (0x21):    // LXI H, d16
        lxi (state, state->h, state->l, nextWord);
        break;
    case (0x22):    // SHLD addr
        break;
    case (0x23):    // INX H
        inx(state->h, state->l);
        break;
    case (0x24):    // INR H (S, Z, A, P)
        inr(state, state->h);
        break;
    case (0x25):    // DCR H
        break;
    case (0x26):    // MVI H, d8
        break;
    case (0x27):    // DAA
        break;
    case (0x28):    // NOP
        break;
    case (0x29):    // DAD H
        break;
    case (0x2A):    // LHLD addr
        break;
    case (0x2B):    // DCX H
        break;
    case (0x2C):    // INR L (S, Z, A, P)
        inr(state, state->l);
        break;
    case (0x2D):    // DCR L
        break;
    case (0x2E):    // MVI L, d8
        break;
    case (0x2F):    // CMA
        state->a = ~state->a;
        break;
    case (0x30):    // SIM
        break;
    case (0x31):    // LXI SP, d16
        state->sp = nextWord;
        break;
    case (0x32):    // STA addr
        break;
    case (0x33):    // INX SP
        state->sp += 1;
        break;
    case (0x34):    // INR M (S, Z, A, P)
        inr(state, state->memory[address]);
        break;
    case (0x35):    // DCR M
        break;
    case (0x36):    // MVI M, d8
        break;
    case (0x37):    // STC
        state->cc.c = 1;
        break;
    case (0x38):    // NOP
        break;
    case (0x39):    // DAD SP
        break;
    case (0x3A):    // LDA addr
        break;
    case (0x3B):    // DCX SP
        break;
    case (0x3C):    // INR A (S, Z, A, P)
        inr(state, state->a);
        break;
    case (0x3D):    // DCR A
        break;
    case (0x3E):    // MVI A, d8
        break;
    case (0x3F):    // CMC
        break;
    case (0x40):    // MOV B, B
        break;
    case (0x41):    // MOV B, C
        break;
    case (0x42):    // MOV B, D
        break;
    case (0x43):    // MOV B, E
        break;
    case (0x44):    // MOV B, H
        break;
    case (0x45):    // MOV B, L
        break;
    case (0x46):    // MOV B, M
        break;
    case (0x47):    // MOV B, A
        break;
    case (0x48):    // MOV C, B
        break;
    case (0x49):    // MOV C, C
        break;
    case (0x4A):    // MOV C, D
        break;
    case (0x4B):    // MOV C, E
        break;
    case (0x4C):    // MOV C, H
        break;
    case (0x4D):    // MOV C, L
        break;
    case (0x4E):    // MOV C, M
        break;
    case (0x4F):    // MOV C, A
        break;
    case (0x50):    // MOV D, B
        break;
    case (0x51):    // MOV D, C
        break;
    case (0x52):    // MOV D, D
        break;
    case (0x53):    // MOV D, E
        break;
    case (0x54):    // MOV D, H
        break;
    case (0x55):    // MOV D
        break;
    case (0x56):    // MOV D, M
        break;
    case (0x57):    // MOV D, A
        break;
    case (0x58):    // MOV E, B
        break;
    case (0x59):    // MOV E, C
        break;
    case (0x5A):    // MOV E, D
        break;
    case (0x5B):    // MOV E, E
        break;
    case (0x5C):    // MOV E, H
        break;
    case (0x5D):    // MOV E, L
        break;
    case (0x5E):    // MOV E, M
        break;
    case (0x5F):    // MOV E, A
        break;
    case (0x60):    // MOV H, B
        break;
    case (0x61):    // MOV H, C
        break;
    case (0x62):    // MOV H, D
        break;
    case (0x63):    // MOV H, E
        break;
    case (0x64):    // MOV H, H
        break;
    case (0x65):    // MOV H, L
        break;
    case (0x66):    // MOV H, M
        break;
    case (0x67):    // MOV H, A
        break;
    case (0x68):    // MOV L, B
        break;
    case (0x69):    // MOV L, C
        break;
    case (0x6A):    // MOV L, D
        break;
    case (0x6B):    // MOV L, E
        break;
    case (0x6C):    // MOV L, H
        break;
    case (0x6D):    // MOV L, L
        break;
    case (0x6E):    // MOV L, M
        break;
    case (0x6F):    // MOV L, A
        break;
    case (0x70):    // MOV M, B
        break;
    case (0x71):    // MOV M, C
        break;
    case (0x72):    // MOV M, D
        break;
    case (0x73):    // MOV M, E
        break;
    case (0x74):    // MOV M, H
        break;
    case (0x75):    // MOV M, L
        break;
    case (0x76):    // HLT
        break;
    case (0x77):    // MOV M, A
        break;
    case (0x78):    // MOV A, B
        break;
    case (0x79):    // MOV A, C
        break;
    case (0x7A):    // MOV A, D
        break;
    case (0x7B):    // MOV A, E
        break;
    case (0x7C):    // MOV A, H
        break;
    case (0x7D):    // MOV A, L
        break;
    case (0x7E):    // MOV A, M
        break;
    case (0x7F):    // MOV A, A
        break;
    case (0x80):    // ADD B
        break;
    case (0x81):    // ADD C
        break;
    case (0x82):    // ADD D
        break;
    case (0x83):    // ADD E
        break;
    case (0x84):    // ADD H
        break;
    case (0x85):    // ADD L
        break;
    case (0x86):    // ADD M
        break;
    case (0x87):    // ADD A
        break;
    case (0x88):    // ADC B
        break;
    case (0x89):    // ADC C
        break;
    case (0x8A):    // ADC D
        break;
    case (0x8B):    // ADC E
        break;
    case (0x8C):    // ADC H
        break;
    case (0x8D):    // ADC L
        break;
    case (0x8E):    // ADC M
        break;
    case (0x8F):    // ADC A
        break;
    case (0x90):    // SUB B
        break;
    case (0x91):    // SUB C
        break;
    case (0x92):    // SUB D
        break;
    case (0x93):    // SUB E
        break;
    case (0x94):    // SUB H
        break;
    case (0x95):    // SUB L
        break;
    case (0x96):    // SUB M
        break;
    case (0x97):    // SUB A
        break;
    case (0x98):    // SBB B
        break;
    case (0x99):    // SBB C
        break;
    case (0x9A):    // SBB D
        break;
    case (0x9B):    // SBB E
        break;
    case (0x9C):    // SBB H
        break;
    case (0x9D):    // SBB L
        break;
    case (0x9E):    // SBB M
        break;
    case (0x9F):    // SBB A
        break;
    case (0xA0):    // ANA B
        break;
    case (0xA1):    // ANA C
        break;
    case (0xA2):    // ANA D
        break;
    case (0xA3):    // ANA E
        break;
    case (0xA4):    // ANA H
        break;
    case (0xA5):    // ANA L
        break;
    case (0xA6):    // ANA M
        break;
    case (0xA7):    // ANA A
        break;
    case (0xA8):    // XRA B
        break;
    case (0xA9):    // XRA C
        break;
    case (0xAA):    // XRA D
        break;
    case (0xAB):    // XRA E
        break;
    case (0xAC):    // XRA H
        break;
    case (0xAD):    // XRA L
        break;
    case (0xAE):    // XRA M
        break;
    case (0xAF):    // XRA A
        break;
    case (0xB0):    // ORA B
        break;
    case (0xB1):    // OR
        break;
    case (0xB2):    // ORA D
        break;
    case (0xB3):    // ORA E
        break;
    case (0xB4):    // ORA H
        break;
    case (0xB5):    // ORA L
        break;
    case (0xB6):    // ORA M
        break;
    case (0xB7):    // ORA A
        break;
    case (0xB8):    // CMP B
        break;
    case (0xB9):    // CMP C
        break;
    case (0xBA):    // CMP D
        break;
    case (0xBB):    // CMP E
        break;
    case (0xBC):    // CMP H
        break;
    case (0xBD):    // CMP L
        break;
    case (0xBE):    // CMP M
        break;
    case (0xBF):    // CMP A
        break;
    case (0xC0):    // RNZ
        rnx(state, state->cc.z, opcode);
        break;
    case (0xC1):    // POP B
        break;
    case (0xC2):    // JNZ addr
        jnx(state, state->cc.z, opcode);
        break;
    case (0xC3):    // JMP addr
        state->pc = (opcode[2] << 8) | opcode[1];
        break;
    case (0xC4):    // CNZ addr
        cnx(state, state->cc.z, opcode);
        break;
    case (0xC5):    // PUSH B
        break;
    case (0xC6):    // ADI d8
        break;
    case (0xC7):    // RST 0
        rst(state, 0x0000);
        break;
    case (0xC8):    // RZ
        rx(state, state->cc.z, opcode);
        break;
    case (0xC9):    // RET
        break;
    case (0xCA):    // JZ addr
        break;
    case (0xCB):    // JMP addr
        break;
    case (0xCC):    // CZ addr
        cx(state, state->cc.z, opcode);
        break;
    case (0xCD):    // CALL addr
        call (state, opcode);
        break;
    case (0xCE):    // ACI d8
        break;
    case (0xCF):    // RST 1
        rst(state, 0x0008);
        break;
    case (0xD0):    // RNC
        rnx(state, state->cc.c, opcode);
        break;
    case (0xD1):    // POP D
        break;
    case (0xD2):    // JNC addr
        jnx (state, state->cc.c, opcode);
        break;
    case (0xD3):    // OUT d8
        break;
    case (0xD4):    // CNC addr
        cnx (state, state->cc.c, opcode);
        break;
    case (0xD5):    // PUSH D
        break;
    case (0xD6):    // SUI d8
        break;
    case (0xD7):    // RST 2
        rst(state, 0x0010);
        break;
    case (0xD8):    // RC
        rx(state, state->cc.c, opcode);
        break;
    case (0xD9):    // -
        break;
    case (0xDA):    // JC addr
        break;
    case (0xDB):    // IN d8
        break;
    case (0xDC):    // CC addr
        cx(state, state->cc.c, opcode);
        break;
    case (0xDD):    // -
        break;
    case (0xDE):    // SBI d8
        break;
    case (0xDF):    // RST 3
        rst(state, 0x0018);
        break;
    case (0xE0):    // RPO
        rnx(state, state->cc.p, opcode);
        break;
    case (0xE1):    // POP H
        break;
    case (0xE2):    // JPO addr
        jnx(state, state->cc.p, opcode);
        break;
    case (0xE3):    // XTHL
        break;
    case (0xE4):    // CPO addr
        cnx (state, state->cc.p, opcode);
        break;
    case (0xE5):    // PUSH H
        break;
    case (0xE6):    // ANI d8
        break;
    case (0xE7):    // RST 4
        rst(state, 0x0020);
        break;
    case (0xE8):    // RPE
        rx(state, state->cc.p, opcode);
        break;
    case (0xE9):    // PCHL
        state->pc = ((uint16_t)state->h << 8) | (uint16_t) state->l;
        break;
    case (0xEA):    // JPE addr
        break;
    case (0xEB):    // XCHG
        break;
    case (0xEC):    // CPE addr
        cx(state, state->cc.p, opcode);
        break;
    case (0xED):    // -
        break;
    case (0xEE):    // XRI d8
        break;
    case (0xEF):    // RST 5
        rst(state, 0x0028);
        break;
    case (0xF0):    // RP
        rnx(state, state->cc.s, opcode);
        break;
    case (0xF1):    // POP PSW
        break;
    case (0xF2):    // JP addr
        jnx (state, state->cc.s, opcode);
        break;
    case (0xF3):    // DI
        break;
    case (0xF4):    // CP addr
        cnx (state, state->cc.s, opcode);
        break;
    case (0xF5):    // PUSH PSW
        break;
    case (0xF6):    // ORI d8
        break;
    case (0xF7):    // RST 6
        rst(state, 0x0030);
        break;
    case (0xF8):    // RM
        rx(state, state->cc.s, opcode);
        break;
    case (0xF9):    // SPHL
        break;
    case (0xFA):    // JM addr
        break;
    case (0xFB):    // EI
        break;
    case (0xFC):    // CM addr
        cx(state, state->cc.s, opcode);
        break;
    case (0xFD):    // -
        break;
    case (0xFE):    // CPI d8
        break;
    case (0xFF):    // RST 7
        rst(state, 0x0038);
        break;
    }
    state->pc += 1;
}



int main (int argc, char** argv) {
    i8080* state;
    loadROM(&state);
    while (state->pc < fileSize) {
        opcodeExtract(&state);
    }
    return 0;
}