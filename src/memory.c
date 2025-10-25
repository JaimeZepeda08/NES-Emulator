#include "../include/memory.h"
#include "../include/log.h"
#include "../include/ppu.h"
#include "../include/cpu.h"
#include "../include/input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

MEM *memory_init() {
    printf("Initializing system memory...");

    MEM *memory = (MEM *)malloc(sizeof(MEM));
    if (memory == NULL) {
        printf("\tFAILED\n");
        fprintf(stderr, "Memory allocation for addess space failed\n");
        exit(1);
    }

    memset(memory->ram, 0, RAM_SIZE);
    memset(memory->prg_rom, 0, PROG_ROM);

    printf("\tDONE\n");
    return memory;
}

void memory_free(MEM *memory) {
    free(memory);
}

uint8_t memory_read(uint16_t address, MEM *memory, PPU *ppu) {
    // PPU Memory Mapped Registers
    if (address >= 0x2000 && address <= 0x3FFF) {
        address = 0x2000 + (address % 8); // Mirrored every 8 bytes
        return ppu_read(ppu, address);
    }

    // Controller Shift Registers
    if (address == 0x4016 || address == 0x4017) {
        return cntrl_read(controller);
    }

    // Internal RAM (mirrored every 0x0800)
    if (address <= 0x1FFF) {
        uint8_t value = memory->ram[address % 0x0800];
        DEBUG_MSG_MEM("Reading 0x%02X from RAM at 0x%04X", value, address);
        return value;
    }

    // PRG ROM (NROM, mapped to 0x8000 - 0xFFFF)
    if (address >= 0x8000) {
        uint8_t value = memory->prg_rom[address - 0x8000];
        DEBUG_MSG_MEM("Reading 0x%02X from ROM at 0x%04X", value, address);
        return value;
    }

    // Open bus / unmapped memory
    DEBUG_MSG_MEM("Read from unmapped memory at 0x%04X", address);
    return 0xFF;
}

void memory_write(uint16_t address, uint8_t value, MEM *memory, PPU *ppu) {
    // OAMDMA
    if (address == 0x4014) {
        uint16_t base = value << 8; // high byte of address (page aligned)
        for (int i = 0; i < 256; i++) {
            uint8_t byte = memory_read(base + i, memory, ppu);
            ppu->oam[(ppu->OAMADDR + i) % 256] = byte;
        }
        return;
    }

    // PPU Memory Mapped Registers
    if (address >= 0x2000 && address <= 0x3FFF) {
        address = 0x2000 + (address % 8);
        ppu_write(ppu, address, value);
        return;
    }

    // Controller Shift Registers
    if (address == 0x4016 || address == 0x4017) {
        cntrl_write(controller, value);
        return;
    }

    // Internal RAM
    if (address <= 0x1FFF) {
        memory->ram[address % 0x0800] = value;
        DEBUG_MSG_MEM("Wrote 0x%02X to RAM at 0x%04X", value, address);
        return;
    }

    // Writes to PRG ROM are ignored in NROM
    if (address >= 0x8000) {
        DEBUG_MSG_MEM("Attempted to write 0x%02X to ROM at 0x%04X (ignored)", value, address);
        return;
    }

    // Unmapped
    DEBUG_MSG_MEM("Write to unmapped memory at 0x%04X", address);
}

void stack_push(uint8_t value, CPU *cpu, MEM *memory, PPU *ppu) {
    DEBUG_MSG_CPU("Pushing value 0x%02X to stack", value);
    memory_write(STACK_BASE + cpu->S--, value, memory, ppu);
}

uint8_t stack_pop(CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(STACK_BASE + ++cpu->S, memory, ppu);
    DEBUG_MSG_CPU("Popping value 0x%02X from stack", value);
    return value;
}

void memory_dump(FILE *output, MEM *memory) {
    if (!memory || !output) {
        fprintf(stderr, "[ERROR] [Memory] Invalid memory or output stream!\n");
        return;
    }

    for (size_t i = 0; i < MEMORY_SIZE; i += 16) {
        fprintf(output, "%04lX: ", i);  // Print memory address (offset)
        
        // Print hex bytes
        for (size_t j = 0; j < 16; j++) {
            if (i + j < MEMORY_SIZE) {
                if (i + j < RAM_SIZE) {
                    fprintf(output, "%02X ", memory->ram[i + j]); // Print hex value
                } else if (i + j > 0x7FFF) {
                    fprintf(output, "%02X ", memory->prg_rom[i + j - PROG_ROM]); // Print hex value
                } else {
                    fprintf(output, "00 "); // Print hex value
                }
            } else {
                fprintf(output, "   ");  // Padding for alignment
            }
        }

        fprintf(output, " | "); // Separator

        // Print ASCII representation
        for (size_t j = 0; j < 16; j++) {
            if (i + j < MEMORY_SIZE) {
                uint8_t byte = 0x00;
                if (i + j < RAM_SIZE) {
                    byte = memory->ram[i + j];
                } else if (i + j > 0x7FFF) {
                    byte = memory->prg_rom[i + j];
                } 
                fprintf(output, "%c", (byte >= 32 && byte <= 126) ? byte : '.'); // Printable ASCII or '.'
            }
        }
        
        fprintf(output, " |\n"); // End of line
    }
}