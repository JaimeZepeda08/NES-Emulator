#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stddef.h>
#include "ppu.h"

// CPU status register (P) flags
#define FLAG_CARRY          0x01 // Bit 1 (C)
#define FLAG_ZERO           0x02 // Bit 2 (Z)
#define FLAG_INT            0x04 // Bit 3 (I)
#define FLAG_DECIMAL        0x08 // Bit 4 (D) (unused in NES)
#define FLAG_BREAK          0x10 // Bit 5 (B)
#define FLAG_UNUSED         0x20 // Bit 6 (1) (always set to 1)
#define FLAG_OVERFLOW       0x40 // Bit 7 (V)
#define FLAG_NEGATIVE       0x80 // Bit 8 (N)

typedef struct CPU {
    u_int8_t A;         // Accumulator
    u_int8_t X;         // X Register
    u_int8_t Y;         // Y Register
    u_int16_t PC;       // Program Counter
    u_int8_t S;         // Stack Pointer
    u_int8_t P;         // Status Register
    int cycles;         // Cycle counter --> important to synchronize with PPU and APU
    int page_crossed;   // 1 if page was crossed during instruction
    int service_int;    // if 1, then an interrupt is being serviced
} CPU;

CPU *cpu_init(uint8_t *memory);
void cpu_free(CPU *cpu);
void cpu_execute_opcode(CPU *cpu, uint8_t opcode, uint8_t *memory, PPU *ppu);
void cpu_irq(CPU *cpu, uint8_t *memory, PPU *ppu);
void cpu_nmi(CPU *cpu, uint8_t *memory, PPU *ppu);
void cpu_dump_registers(CPU *cpu);

#endif