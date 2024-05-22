#include<stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <stdbool.h>

#define MEMORY_SIZE 65535

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

int main (int argc, char** argv) {
    unsigned char* buffer = loadROM();
    for (int i = 0; i < 300; i++) {
        printf("%x", buffer[i]);
    }
    return 0;
}