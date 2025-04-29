#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdio.h>
#include "cpu.h"
#include "ppu.h"

// NES has 16 bit address bus -> 2^16 = 64KB address space
#define MEMORY_SIZE 0x10000 // 64KB
// NES STACK starts at 0x01FF and grows down to 0x0100
#define STACK_BASE 0x0100

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

u_int8_t *memory_init();
void memory_free(uint8_t *memory);
uint8_t memory_read(uint16_t address, uint8_t *memory, PPU *ppu);
void memory_write(uint16_t address, uint8_t value, uint8_t *memory, PPU *ppu);
void stack_push(uint8_t value, CPU *cpu, uint8_t *memory, PPU *ppu);
uint8_t stack_pop(CPU *cpu, uint8_t *memory, PPU *ppu);
void memory_dump(FILE *output, uint8_t *memory);

#endif