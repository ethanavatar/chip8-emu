#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "canvas.h"
#include "chip8.h"

const uint8_t chip8_fontset[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F;
};

const int clock_speed = 10;
const int screen_width = 64;
const int screen_height = 32;

static void ExecuteOpcode(struct Chip8 *chip8, uint16_t opcode);

static uint16_t ReadWord(struct Chip8 *chip8, uint16_t address) {
    return (chip8->memory[address] << 8) | chip8->memory[address + 1];
}

struct Chip8 *InitChip8(void) {
    struct Chip8 *chip8 = (struct Chip8 *) malloc(sizeof(struct Chip8));
    //memset(chip8, 0, sizeof(struct Chip8));

    chip8->canvas = (struct Canvas *) malloc(sizeof(struct Canvas));
    InitCanvas(chip8->canvas, screen_width, screen_height);

    return chip8;
}

void DeinitChip8(struct Chip8 *chip8) {
    DeinitCanvas(chip8->canvas);
    free(chip8);
}

void Reset(struct Chip8 *chip8) {
    ClearCanvas(chip8->canvas);

    //memset(chip8, 0, sizeof(struct Chip8));
    chip8->draw_flag = false;
    chip8->paused = false;

    chip8->pc = 0x200;

    chip8->I = 0;
    chip8->sp = 0;

    for (int i = 0; i < 16; i++) {
        chip8->V[i] = 0;
    }

    for (int i = 0; i < 16; i++) {
        chip8->stack[i] = 0;
    }

    for (int i = 0; i < 4096; i++) {
        chip8->memory[i] = 0;
    }

    for (int i = 0; i < 16; i++) {
        chip8->keypad[i] = 0;
    }

    chip8->delay_timer = 0;
    chip8->sound_timer = 0;

    fprintf(stderr, "Loading fontset...\n");
    for (int i = 0; i < 80; i++) {
        chip8->memory[i] = chip8_fontset[i];
    }
}

void LoadROM(struct Chip8 *chip8, uint8_t* rom, int size) {
    Reset(chip8);
    memcpy(chip8->memory + 0x200, rom, size);
}

void EmulateCycle(struct Chip8 *chip8) {

    if (chip8->waiting_for_key) {
        for (int i = 0; i < 16; i++) {
            if (chip8->keypad[i]) {
                chip8->V[chip8->waiting_for_key - 1] = i;
                chip8->waiting_for_key = false;
                chip8->paused = false;
                break;
            }
        }

        return;
    }

    if (chip8->paused) {
        return;
    }

    for (int clock = 0; clock < clock_speed; ++clock) {
        uint16_t opcode = ReadWord(chip8, chip8->pc);
        chip8->pc += 2;
        ExecuteOpcode(chip8, opcode);
    }

    if (chip8->delay_timer > 0) {
        --chip8->delay_timer;
    }

    if (chip8->sound_timer > 0) {
        if (chip8->sound_timer == 1) {
            //printf("BEEP!\n");
        }
        --chip8->sound_timer;
    }
}

static void DrawSprite(struct Chip8 *chip8, uint8_t x, uint8_t y, uint8_t *sprite, uint8_t n) {
    chip8->V[0xF] = 0;
    for (int row = 0; row < n; ++row) {
        uint8_t pixel = sprite[row];
        for (int col = 0; col < 8; ++col) {
            if ((pixel & (0x80 >> col)) == 0) {
                continue;
            }

            int index = (x + col) + (y + row) * screen_width;
            if (chip8->canvas->buffer[index] == 1) {
                chip8->V[0xF] = 1;
            }
            chip8->canvas->buffer[index] ^= 1;
        }
    }

    chip8->draw_flag = true;
}

// 00E0 - CLS
// Clear the display.
static void cls(struct Chip8 *chip8) {
    ClearCanvas(chip8->canvas);
}

// 00EE - RET
// Return from a subroutine.
static void ret(struct Chip8 *chip8) {
    chip8->pc = chip8->stack[chip8->sp--];
}

// 1nnn - JP addr
// Jump to location nnn.
static void jp(struct Chip8 *chip8, uint16_t nnn) {
    chip8->pc = nnn;
}

// 2nnn - CALL addr
// Call subroutine at nnn.
static void call(struct Chip8 *chip8, uint16_t nnn) {
    chip8->stack[++chip8->sp] = chip8->pc;
    jp(chip8, nnn);
}

// 3xkk - SE Vx, byte
// Skip next instruction if Vx = kk.
static void se(struct Chip8 *chip8, uint8_t x, uint8_t kk) {
    if (chip8->V[x] == kk) {
        chip8->pc += 2;
    }
}

// 4xkk - SNE Vx, byte
// Skip next instruction if Vx != kk.
static void sne(struct Chip8 *chip8, uint8_t x, uint8_t kk) {
    if (chip8->V[x] != kk) {
        chip8->pc += 2;
    }
}

// 5xy0 - SE Vx, Vy
// Skip next instruction if Vx = Vy.
static void se1(struct Chip8 *chip8, uint8_t x, uint8_t y) {
    if (chip8->V[x] == chip8->V[y]) {
        chip8->pc += 2;
    }
}

// 6xkk - LD Vx, byte
// Set Vx = kk.
static void ld(struct Chip8 *chip8, uint8_t x, uint8_t kk) {
    chip8->V[x] = kk;
}

// 7xkk - ADD Vx, byte
// Set Vx = Vx + kk.
static void add(struct Chip8 *chip8, uint8_t x, uint8_t kk) {
    chip8->V[x] += kk;
}

// 8xy0 - LD Vx, Vy
// Set Vx = Vy.
static void ld1(struct Chip8 *chip8, uint8_t x, uint8_t y) {
    chip8->V[x] = chip8->V[y];
}


// 8xy1 - OR Vx, Vy
// Set Vx = Vx OR Vy.
static void or(struct Chip8 *chip8, uint8_t x, uint8_t y) {
    chip8->V[x] |= chip8->V[y];
}

// 8xy2 - AND Vx, Vy
// Set Vx = Vx AND Vy.
static void and(struct Chip8 *chip8, uint8_t x, uint8_t y) {
    chip8->V[x] &= chip8->V[y];
}

// 8xy3 - XOR Vx, Vy
// Set Vx = Vx XOR Vy.
static void xor(struct Chip8 *chip8, uint8_t x, uint8_t y) {
    chip8->V[x] ^= chip8->V[y];
}

// 8xy4 - ADD Vx, Vy
// Set Vx = Vx + Vy, set VF = carry.
static void add1(struct Chip8 *chip8, uint8_t x, uint8_t y) {
    uint16_t sum = chip8->V[x] + chip8->V[y];
    chip8->V[x] = sum & 0xFF;
    chip8->V[0xF] = sum > 0xFF;
}

// 8xy5 - SUB Vx, Vy
// Set Vx = Vx - Vy, set VF = NOT borrow.
static void sub(struct Chip8 *chip8, uint8_t x, uint8_t y) {
    uint8_t underflow = chip8->V[x] < chip8->V[y];
    chip8->V[x] -= chip8->V[y];
    chip8->V[0xF] = underflow == 0; 
}

// 8xy6 - SHR Vx {, Vy}
// Set Vx = Vx SHR 1, set VF to the bit that was shifted out.
static void shr(struct Chip8 *chip8, uint8_t x, uint8_t y) {
    (void) y;

    uint8_t carry = chip8->V[x] & 0x1;
    chip8->V[x] >>= 1;
    chip8->V[0xF] = carry;
}

// 8xy7 - SUBN Vx, Vy
// Set Vx = Vy - Vx, set VF = NOT borrow.
static void subn(struct Chip8 *chip8, uint8_t x, uint8_t y) {
    uint8_t underflow = chip8->V[y] < chip8->V[x];
    chip8->V[x] = chip8->V[y] - chip8->V[x];
    chip8->V[0xF] = underflow == 0;
}

// 8xyE - SHL Vx {, Vy}
// Set Vx = Vx SHL 1. Set VF to the bit that was shifted out.
static void shl(struct Chip8 *chip8, uint8_t x, uint8_t y) {
    (void) y;

    uint8_t carry = chip8->V[x] >> 7;
    chip8->V[x] <<= 1;
    chip8->V[0xF] = carry;
}

// 9xy0 - SNE Vx, Vy
// Skip next instruction if Vx != Vy.
static void sne1(struct Chip8 *chip8, uint8_t x, uint8_t y) {
    if (chip8->V[x] != chip8->V[y]) {
        chip8->pc += 2;
    }
}

// Annn - LD I, addr
// Set I = nnn.
static void ld2(struct Chip8 *chip8, uint16_t nnn) {
    chip8->I = nnn;
}

// Bnnn - JP V0, addr
// Jump to location nnn + V0.
static void jp1(struct Chip8 *chip8, uint16_t nnn) {
    chip8->pc = nnn + chip8->V[0];
}

// Cxkk - RND Vx, byte
// Set Vx = random byte AND kk.
static void rnd(struct Chip8 *chip8, uint8_t x, uint8_t kk) {
    chip8->V[x] = (rand() % 0xFF) & kk;
}

// Dxyn - DRW Vx, Vy, nibble
// Display n-byte sprite starting at memory location I at (Vx, Vy), set VF = collision.
static void drw(struct Chip8 *chip8, uint8_t x, uint8_t y, uint8_t n) {
    uint8_t *sprite = chip8->memory + chip8->I;
    DrawSprite(chip8, chip8->V[x], chip8->V[y], sprite, n);
}

// Ex9E - SKP Vx
// Skip next instruction if key with the value of Vx is pressed.
static void skp(struct Chip8 *chip8, uint8_t x) {
    if (chip8->keypad[chip8->V[x]]) {
        chip8->pc += 2;
    }
}

// ExA1 - SKNP Vx
// Skip next instruction if key with the value of Vx is not pressed.
static void sknp(struct Chip8 *chip8, uint8_t x) {
    if (!chip8->keypad[chip8->V[x]]) {
        chip8->pc += 2;
    }
}

// Fx07 - LD Vx, DT
// Set Vx = delay timer value.
static void ld3(struct Chip8 *chip8, uint8_t x) {
    chip8->V[x] = chip8->delay_timer;
}

// Fx0A - LD Vx, K
// Wait for a key press, store the value of the key in Vx.
static void ld4(struct Chip8 *chip8, uint8_t x) {
    chip8->paused = true;
    chip8->waiting_for_key = true;
}

// Fx15 - LD DT, Vx
// Set delay timer = Vx.
static void ld5(struct Chip8 *chip8, uint8_t x) {
    chip8->delay_timer = chip8->V[x];
}

// Fx18 - LD ST, Vx
// Set sound timer = Vx.
static void ld6(struct Chip8 *chip8, uint8_t x) {
    chip8->sound_timer = chip8->V[x];
}

// Fx1E - ADD I, Vx
// Set I = I + Vx.
static void add2(struct Chip8 *chip8, uint8_t x) {
    chip8->I += chip8->V[x];
}

// Fx29 - LD F, Vx
// Set I = location of sprite for digit Vx.
static void ld7(struct Chip8 *chip8, uint8_t x) {
    chip8->I = chip8->V[x] * 5;
}

// Fx33 - LD B, Vx
// Store BCD representation of Vx in memory locations I, I+1, and I+2.
static void ld8(struct Chip8 *chip8, uint8_t x) {
    chip8->memory[chip8->I] = chip8->V[x] / 100;
    chip8->memory[chip8->I + 1] = (chip8->V[x] / 10) % 10;
    chip8->memory[chip8->I + 2] = (chip8->V[x] % 100) % 10;
}

// Fx55 - LD [I], Vx
// Store registers V0 through Vx in memory starting at location I.
static void ld9(struct Chip8 *chip8, uint8_t x) {
    for (int i = 0; i <= x; i++) {
        chip8->memory[chip8->I + i] = chip8->V[i];
    }
}

// Fx65 - LD Vx, [I]
// Read registers V0 through Vx from memory starting at location I.
static void ld10(struct Chip8 *chip8, uint8_t x) {
    for (int i = 0; i <= x; i++) {
        chip8->V[i] = chip8->memory[chip8->I + i];
    }
}

static void ExecuteOpcode(struct Chip8 *chip8, uint16_t opcode) {

    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint8_t n = opcode & 0x000F;
    uint8_t kk = opcode & 0x00FF;
    uint16_t nnn = opcode & 0x0FFF;

    //fprintf(stderr, "[%d] Executing opcode: 0x%X\n", chip8->pc - 2, opcode);

    switch (opcode & 0xF000) {
    case 0x0000:
        switch (opcode) {
        case 0x00E0: cls(chip8); break;
        case 0x00EE: ret(chip8); break;
        default: fprintf(stderr, "[0x%X] Unknown opcode: 0x%X\n", chip8->pc - 2, opcode);
        }
        break;
    case 0x1000: jp(chip8, nnn); break;
    case 0x2000: call(chip8, nnn); break;
    case 0x3000: se(chip8, x, kk); break;
    case 0x4000: sne(chip8, x, kk); break;
    case 0x5000: se1(chip8, x, y); break;
    case 0x6000: ld(chip8, x, kk); break;
    case 0x7000: add(chip8, x, kk); break;
    case 0x8000:
        switch (opcode & 0x000F) {
        case 0x0000: ld1(chip8, x, y); break;
        case 0x0001: or(chip8, x, y); break;
        case 0x0002: and(chip8, x, y); break;
        case 0x0003: xor(chip8, x, y); break;
        case 0x0004: add1(chip8, x, y); break;
        case 0x0005: sub(chip8, x, y); break;
        case 0x0006: shr(chip8, x, y); break;
        case 0x0007: subn(chip8, x, y); break;
        case 0x000E: shl(chip8, x, y); break;
        default: fprintf(stderr, "[0x%X] Unknown opcode: 0x%X\n", chip8->pc - 2, opcode);
        }
        break;
    case 0x9000: sne1(chip8, x, y); break;
    case 0xA000: ld2(chip8, nnn); break;
    case 0xB000: jp1(chip8, nnn); break;
    case 0xC000: rnd(chip8, x, kk); break;
    case 0xD000: drw(chip8, x, y, n); break;
    case 0xE000:
        switch (opcode & 0x00FF) {
        case 0x009E: skp(chip8, x); break;
        case 0x00A1: sknp(chip8, x); break;
        default: fprintf(stderr, "[0x%X] Unknown opcode: 0x%X\n", chip8->pc - 2, opcode);
        }
        break;
    case 0xF000:
        switch (opcode & 0x00FF) {
        case 0x0007: ld3(chip8, x); break;
        case 0x000A: ld4(chip8, x); break;
        case 0x0015: ld5(chip8, x); break;
        case 0x0018: ld6(chip8, x); break;
        case 0x001E: add2(chip8, x); break;
        case 0x0029: ld7(chip8, x); break;
        case 0x0033: ld8(chip8, x); break;
        case 0x0055: ld9(chip8, x); break;
        case 0x0065: ld10(chip8, x); break;
        default: fprintf(stderr, "[0x%X] Unknown opcode: 0x%X\n", chip8->pc - 2, opcode);
        }
        break;
    default: fprintf(stderr, "[0x%X] Unknown opcode: 0x%X\n", chip8->pc - 2, opcode);
    }
}
