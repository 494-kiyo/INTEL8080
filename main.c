#include<stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <stdbool.h>

#define MEMORY_SIZE 65535
#define HIGH_BYTE(reg) ((uint8_t)((reg >> 8) & 0xFF))
#define LOW_BYTE(reg) ((uint8_t)(reg & 0xFF))
#define SET_HIGH_BYTE(reg, value) ((reg) = ((reg) & 0x00FF) | ((value) << 8))

#define CARRY_MASK ((1 << 1) - 1) << 0
#define PARITY_MASK ((1 << 1)- 1) << 2
#define AC_MASK ((1 << 1)- 1) << 4
#define ZERO_MASK ((1 << 1)- 1) << 6
#define SIGN_MASK ((1 << 1)- 1) << 7

uint16_t AF = 0;
uint16_t BC = 0;
uint16_t DE = 0;
uint16_t HL = 0;
uint16_t SP = 0;
uint16_t stack = {0};

size_t fileSize;

unsigned char* loadROM () {
    FILE* file = fopen("space-invaders.rom", "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open file\n");
        return NULL;
    }

    // file size
    fseek(file, 0, SEEK_END); // file pointer to the end
    size_t file_size = ftell(file); // get the position
    fseek(file, 0, SEEK_SET); // return to the starting position

    unsigned char* buffer = malloc(file_size);
    if (!buffer) {
        fprintf(stderr, "Error: Could not allocate buffer\n");
        fclose(file);
        return NULL;
    }

    if (fread(buffer, 1, file_size, file) != file_size) {
        fprintf(stderr, "Error: Could not read in data\n");
        free(buffer);
        fclose(file);
        return NULL;
    }

    fileSize = file_size;
    fclose(file);
    return buffer;
}

void opcodeExtract (unsigned char* codebuffer, int PC) {
    unsigned char* opcode = &codebuffer[PC];
    printf("%02x", codebuffer[PC]);
    int opbytes = 1;
    uint16_t temp16 = 0;
    uint8_t temp8 = 0;
    switch (*opcode) {
        case (0x00): // NOP
            opbytes = 1;
            break;
        case (0x01): // LXI B, d16
            opbytes = 3;
            BC = ((uint16_t) codebuffer[PC + 1] << 8) | codebuffer[PC + 2];
        case (0x02): // STAX B
            SET_HIGH_BYTE(AF, codebuffer[PC + 1]);
            break;
        case (0x03): // INX B
            BC += 1;
            break;
        case (0x04): // INR B (S, Z, A, P)
            SET_HIGH_BYTE(temp16, (BC & 0xFF00) + 1 | ((BC & 0x00FF)));

    }
}

int main (int argc, char** argv) {
    unsigned char* buffer = loadROM();
    int PC = 3;
    opcodeExtract(buffer, PC);
    return 0;
}