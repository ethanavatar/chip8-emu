#include <corecrt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#include "canvas.h"
#include "chip8.h"

const int screenWidth = 640;
const int screenHeight = 320;

// Oh my gosh, C, I just want to read a binary file
// https://codereview.stackexchange.com/questions/137818/standard-way-of-reading-file-contents-to-a-buffer
const uint8_t *const read_file(const char *const path, size_t *const out_length) {
    FILE *file;
    errno_t ret = fopen_s(&file, path, "rb");
    if (ret != 0 || !file) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        exit(1);
    }

    size_t file_size = 0, buffer_size = 1 << 12;
    uint8_t *buffer = NULL, *temp = NULL;
    do {
        buffer_size <<= 1;
        if (buffer_size < file_size) {
            free(buffer);
            fprintf(stderr, "File too large\n");
            exit(1);
        }

        temp = realloc(buffer, buffer_size);
        if (!temp) {
            free(buffer);
            fprintf(stderr, "Failed to allocate memory\n");
            exit(1);
        }

        buffer = temp;
        file_size += fread(buffer + file_size, 1, buffer_size - file_size, file);
    } while (file_size == buffer_size);

    if (buffer_size <= file_size || ferror(file)) {
        free(buffer);
        fprintf(stderr, "Failed to read file\n");
        exit(1);
    }

    temp = realloc(buffer, file_size + 1);
    if (!temp) {
        free(buffer);
        fprintf(stderr, "Failed to allocate memory\n");
        exit(1);
    }

    buffer = temp;
    buffer[file_size] = '\0';

    fclose(file);

    if (out_length) {
        *out_length = file_size;
    }

    return buffer;
}

void Draw(struct Canvas *canvas) {
    ClearBackground(BLACK);
    DrawCanvas(canvas);
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <rom>\n", argv[0]);
        exit(1);
    }

    const char *const rom = argv[1];
    size_t rom_size = 0;
    uint8_t *rom_buffer = read_file(rom, &rom_size);

    struct Chip8 *chip8 = InitChip8();
    fprintf(stderr, "Chip-8 Initialized\n");

    LoadROM(chip8, rom_buffer, rom_size);
    fprintf(stderr, "Loaded ROM: %s (%d bytes long)\n", rom, rom_size);

    free(rom_buffer);

    InitWindow(screenWidth, screenHeight, "Chip-8 Emu");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {

        float deltaTime = GetFrameTime();
        float deltaTimeMs = deltaTime * 1000.0f;
        const char *title = TextFormat("Chip-8 Emu - %.2f ms", deltaTimeMs);
        SetWindowTitle(title);

        BeginDrawing();
            Draw(chip8->canvas);
        EndDrawing();

        EmulateCycle(chip8);
    }

    CloseWindow();

    return 0;
}
