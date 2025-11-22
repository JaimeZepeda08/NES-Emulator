//////////////////////////////////////////////////////////////
// This file contains NES-specific definitions and constants
// It also implements the NES bus and memory map
//////////////////////////////////////////////////////////////

#ifndef NES_H
#define NES_H

#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "input.h"
#include "mapper.h"
#include "cartridge.h"
#include "display.h"
#include <SDL.h>
#include <SDL_ttf.h>

#define NES_CPU_CLOCK 1789773 // 1.789773 MHz
#define CYCLES_PER_FRAME (NES_CPU_CLOCK / 60) // ~29829.55 cycles per 1/60th second

#define CPU_MEMORY_SIZE     0xFFFF // 64KB CPU address space (16 bits 0x0000 - 0xFFFF)
#define RAM_SIZE            0x0800  // 2KB internal RAM

/////////////////////////////////////////////////////
//                  CPU MEMORY MAP                 //
//=================================================//
// 0x0000-0x07FF    |   2KB internal RAM           // 
// 0x0800-0x0FFF    |   Mirror of 0x0000-0x07FF    //
// 0x1000-0x17FF    |   Mirror of 0x0000-0x07FF    //
// 0x1800-0x1FFF    |   Mirror of 0x0000-0x07FF    //
//=================================================//
// 0x2000-0x2007    |   NES PPU registers          // 
// 0x2008-0x3FFF    |   Mirror of 0x2000-0x2007    //
//=================================================//
// 0x4000-0x4017    |   NES APU and I/O registers  //
//=================================================//
// 0x6000-0x7FFF    |   Cartridge RAM              //
// 0x8000-0xFFFF    |   Cartridge ROM              //
/////////////////////////////////////////////////////

#define PPU_MEMORY_SIZE     0x3FFF  // 16KB PPU address space (14 bits 0x0000 - 0x3FFF)
#define VRAM_SIZE           0x0800  // 2KB VRAM (Nametable 1 and 2)

////////////////////////////////////////////////////
//                PPU MEMORY MAP                  //
//================================================//
// 0x0000-0x0FFF    |   Pattern Table 0           //
// 0x1000-0x1FFF    |   Pattern Table 1           //
//================================================//
// 0x2000-0x23FF    |   Nametable 0               // 
// 0x2400-0x27FF    |   Nametable 1               // 
// 0x2800-0x2BFF    |   Nametable 2               //
// 0x2C00-0x2FFF    |   Nametable 3               //
// 0x3000-0x3EFF    |   Mirror of 0x2000-0x2FFF   //
//================================================//
// 0x3F00-0x3F1F    |   Pallete Ram               //
// 0x3F20-0x3FFF    |   Mirror of 0x3F00-0x3F1F   //
////////////////////////////////////////////////////

typedef struct NES {
    CPU *cpu;
    PPU *ppu;
    APU *apu;
    CNTRL *controller;

    Mapper *mapper; 

    uint8_t ram[RAM_SIZE];      // 2KB CPU RAM
    uint8_t vram[VRAM_SIZE];    // 2KB PPU VRAM 
} NES;


void nes_init(char *filename);
void nes_free();
int nes_cycle(uint32_t *last_time, SDL_Renderer *renderer, SDL_Texture *game_texture, TTF_Font *font, int pt_enable);
uint8_t nes_cpu_read(uint16_t address);
void nes_cpu_write(uint16_t address, uint8_t value);
uint8_t nes_ppu_read(uint16_t address);
void nes_ppu_write(uint16_t address, uint8_t value);

extern NES *nes; // global NES instance for simplicity

#endif