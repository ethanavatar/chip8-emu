#ifndef CHIP8_H
#define CHIP8_H

#include <stdint.h>
#include "canvas.h"

struct Chip8 {
    uint16_t pc;
    uint8_t sp;
    uint16_t I;
    
    bool paused;
    bool waiting_for_key;
    bool draw_flag;

    uint8_t delay_timer;
    uint8_t sound_timer;

    uint8_t memory[4096];
    uint8_t V[16];
    uint16_t stack[16];
    uint8_t keypad[16];
    struct Canvas *canvas;
};

struct Chip8 *InitChip8(void);
void DeinitChip8(struct Chip8 *chip8);

void Reset(struct Chip8 *chip8);
void LoadROM(struct Chip8 *chip8, uint8_t* rom, int size);
void EmulateCycle(struct Chip8 *chip8);

#endif // !CHIP8_H
