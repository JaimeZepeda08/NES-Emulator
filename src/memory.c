#include "../include/memory.h"
#include "../include/log.h"
#include "../include/ppu.h"
#include "../include/cpu.h"
#include "../include/input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t *memory_init() {
    printf("Initializing system memory...");

    uint8_t *memory = (u_int8_t *)calloc(MEMORY_SIZE, sizeof(uint8_t)); // Allocate 64KB of memory
    if (memory == NULL) {
        printf("\tFAILED\n");
        fprintf(stderr, "Memory allocation for addess space failed\n");
        exit(1);
    }

    printf("\tDONE\n");
    return memory;
}

void memory_free(uint8_t *memory) {
    free(memory);
}

uint8_t memory_read(uint16_t address, uint8_t *memory, PPU *ppu) {
    // PPU Memory Mapped Registers
    if (address >= 0x2000 && address <= 0x3FFF) {
        address = 0x2000 + (address % 8); // Mirrored every 8 bytes
        return ppu_read(ppu, address);
    }

    // Controller Shift Registers
    if (address == 0x4016 || address == 0x4017) {
        uint8_t value = cntrl_read(controller);
        return value;
    }

    if (address <= 0x1FFF) { // Mirror internal RAM
        address = address % 0x0800;
    }

    uint8_t value =  memory[address]; // Normal memory read
    DEBUG_MSG_MEM("Reading 0x%02X from address 0x%04X", value, address);
    return value;
}

void memory_write(uint16_t address, uint8_t value, uint8_t *memory, PPU *ppu) {
    // OAMDMA
    if (address == 0x4014) {
        uint16_t base = value << 8;
        for (int i = 0; i < 256; i++) {
            uint8_t byte = memory_read(base + i, memory, ppu);  
            ppu->oam[(ppu->OAMADDR + i) % 256] = byte; // write into PPU OAM
        }
        return;
    }

    // PPU Memory Mapped Registers
    if (address >= 0x2000 && address <= 0x3FFF) { 
        address = 0x2000 + (address % 8); // Mirrored every 8 bytes
        ppu_write(ppu, address, value);
        return;
    }

    // Controller Shift Registers
    if (address == 0x4016 || address == 0x4017) {
        cntrl_write(controller, value);
    }

    if (address <= 0x1FFF) { // Mirror internal RAM
        address = address % 0x0800;
    }
    DEBUG_MSG_MEM("Writing 0x%02X to address 0x%04X", value, address);
    memory[address] = value; // Normal memory write
}

void stack_push(uint8_t value, CPU *cpu, uint8_t *memory, PPU *ppu) {
    DEBUG_MSG_CPU("Pushing value 0x%02X to stack", value);
    memory_write(STACK_BASE + cpu->S--, value, memory, ppu);
}

uint8_t stack_pop(CPU *cpu, uint8_t *memory, PPU *ppu) {
    uint8_t value = memory_read(STACK_BASE + ++cpu->S, memory, ppu);
    DEBUG_MSG_CPU("Popping value 0x%02X from stack", value);
    return value;
}

void memory_dump(FILE *output, uint8_t *memory) {
    if (!memory || !output) {
        fprintf(stderr, "[ERROR] [Memory] Invalid memory or output stream!\n");
        return;
    }

    for (size_t i = 0; i < MEMORY_SIZE; i += 16) {
        fprintf(output, "%04lX: ", i);  // Print memory address (offset)
        
        // Print hex bytes
        for (size_t j = 0; j < 16; j++) {
            if (i + j < MEMORY_SIZE) {
                fprintf(output, "%02X ", memory[i + j]); // Print hex value
            } else {
                fprintf(output, "   ");  // Padding for alignment
            }
        }

        fprintf(output, " | "); // Separator

        // Print ASCII representation
        for (size_t j = 0; j < 16; j++) {
            if (i + j < MEMORY_SIZE) {
                uint8_t byte = memory[i + j];
                fprintf(output, "%c", (byte >= 32 && byte <= 126) ? byte : '.'); // Printable ASCII or '.'
            }
        }
        
        fprintf(output, " |\n"); // End of line
    }
}