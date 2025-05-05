#include <stdio.h>
#include <stdlib.h>
#include "../include/cpu.h"
#include "../include/log.h"
#include "../include/memory.h"
#include "../include/ppu.h"

uint16_t cpu_zero_page(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu);
uint16_t cpu_zero_page_x(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu);
uint16_t cpu_zero_page_y(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu);
uint16_t cpu_absolute(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu);
uint16_t cpu_absolute_x(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu);
uint16_t cpu_absolute_y(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu);
uint16_t cpu_indirect_x(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu);
uint16_t cpu_indirect_y(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu);
void update_zero_and_negative_flags(CPU* cpu, uint8_t value);

CPU *cpu_init(uint8_t *memory) {
    printf("Initializing CPU...");

    CPU *cpu = (CPU *)malloc(sizeof(CPU));
    if (cpu == NULL) {
        printf("\tFAILED\n");
        FATAL_ERROR("CPU", "CPU memory allocation failed");
    }

    // Make sure that reset vector is set
    if (memory[0xFFFC] == 0 && memory[0xFFFD] == 0) {
        printf("\tFAILED\n");
        FATAL_ERROR("CPU", "Reset vector at 0xFFFC and 0xFFFD not set");
    }

    // Set initial register values
    cpu->A = 0;
    cpu->X = 0;
    cpu->Y = 0;
    cpu->PC = memory[0xFFFC] | (memory[0xFFFD] << 8); // reset vector at 0xFFFC and 0xFFFD (little endian) // 0xC000
    cpu->S = 0xFD; 
    cpu->P = 0x24;

    cpu->cycles = 0;
    cpu->page_crossed = 0;
    cpu->service_int = 0;

    printf("\tDONE\n");
    return cpu;
}

void cpu_free(CPU *cpu) {
    free(cpu);
}

void cpu_execute_opcode(CPU *cpu, uint8_t opcode, uint8_t *memory, PPU *ppu) {
    cpu->cycles = 0;
    cpu->page_crossed = 0;

    if (ppu->nmi == 1 && cpu->service_int == 0) { // Handle NMI interrupt
        cpu->PC--; // decrement because we incremented when calling the function
        cpu_nmi(cpu, memory, ppu);
        return;
    }

    switch (opcode) {
        // ==================== Access ====================

        // ======== LDA ======== 

        case 0xA9: { // LDA #Immediate
            uint8_t immediate =  memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [LDA #Immediate]: %02X %02X", opcode, immediate);

            DEBUG_MSG_CPU("Writing 0x%02X to register A", immediate);
            cpu->A = immediate;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 2;
            break;
        }

        case 0xA5: { // LDA Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "LDA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu); 
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 3;
            break;
        }
        
        case 0xB5: { // LDA Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "LDA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu); 
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            break;
        }

        case 0xAD: { // LDA Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "LDA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            break;
        }
        
        case 0xBD: { // LDA Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "LDA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1; 
            }
            break;
        }
        
        case 0xB9: { // LDA Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "LDA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }
        
        case 0xA1: { // LDA (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "LDA", cpu, memory, ppu);

            // Update value of register A
            uint8_t value = memory_read(effective_addr, memory, ppu);
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 6;
            break;
        }
        
        case 0xB1: { // LDA (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "LDA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 5;
            if (cpu->page_crossed) {
                cpu->cycles += 1; 
            }
            break;
        }
        
        // ======== STA ======== 

        case 0x85: { // STA Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "STA", cpu, memory, ppu);
            memory_write(effective_addr, cpu->A, memory, ppu);

            // Set cycles
            cpu->cycles = 3;
            break;
        }
        
        case 0x95: { // STA Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "STA", cpu, memory, ppu);
            memory_write(effective_addr, cpu->A, memory, ppu);

            // Set cycles
            cpu->cycles = 4;
            break;
        }
        
        case 0x8D: { // STA Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "STA", cpu, memory, ppu);
            memory_write(effective_addr, cpu->A, memory, ppu);

            // Set cycles
            cpu->cycles = 4;
            break;
        }
        
        case 0x9D: { // STA Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "STA", cpu, memory, ppu);
            memory_write(effective_addr, cpu->A, memory, ppu);

            // Set cycles
            cpu->cycles = 5;
            break;
        }
        
        case 0x99: { // STA Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "STA", cpu, memory, ppu);
            memory_write(effective_addr, cpu->A, memory, ppu);

            // Set cycles
            cpu->cycles = 5;
            break;
        }

        case 0x81: { // STA (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "STA", cpu, memory, ppu);
            memory_write(effective_addr, cpu->A, memory, ppu);

            // Set cycles
            cpu->cycles = 6;
            break;
        }
        
        case 0x91: { // STA (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "STA", cpu, memory, ppu);
            memory_write(effective_addr, cpu->A, memory, ppu);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        // ======== LDX ======== 

        case 0xA2: { // LDX #Immediate
            uint8_t immediate =  memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [LDX #Immediate]: %02X %02X", opcode, immediate);

            DEBUG_MSG_CPU("Writing 0x%02X to register X", immediate);
            cpu->X = immediate;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->X);

            // Set cycles
            cpu->cycles = 2;
            break;
        }

        case 0xA6: { // LDX Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "LDX", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu); 
            DEBUG_MSG_CPU("Writing 0x%02X to register X", value);
            cpu->X = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->X);

            // Set cycles
            cpu->cycles = 3;
            break;
        }

        case 0xB6: { // LDX Zero Page, Y
            uint16_t effective_addr = cpu_zero_page_y(opcode, "LDX", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu); 
            DEBUG_MSG_CPU("Writing 0x%02X to register X", value);
            cpu->X = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->X);

            // Set cycles
            cpu->cycles = 4;
            break;
        }

        case 0xAE: { // LDX Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "LDX", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            DEBUG_MSG_CPU("Writing 0x%02X to register X", value);
            cpu->X = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->X);

            // Set cycles
            cpu->cycles = 4;
            break;
        }
        
        case 0xBE: { // LDX Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "LDX", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            DEBUG_MSG_CPU("Writing 0x%02X to register X", value);
            cpu->X = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->X);

            // Set cycles
            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        // ======== STX ======== 

        case 0x86: { // STX Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "STX", cpu, memory, ppu);
            memory_write(effective_addr, cpu->X, memory, ppu);

            // Set cycles
            cpu->cycles = 3;
            break;
        }

        case 0x96: { // STX Zero Page, Y
            uint16_t effective_addr = cpu_zero_page_y(opcode, "STX", cpu, memory, ppu);
            memory_write(effective_addr, cpu->X, memory, ppu);

            // Set cycles
            cpu->cycles = 4;
            break;
        }
        
        case 0x8E: { // STX Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "STX", cpu, memory, ppu);
            memory_write(effective_addr, cpu->X, memory, ppu);

            // Set cycles
            cpu->cycles = 4;
            break;
        }

        // ======== LDY ======== 

        case 0xA0: { // LDY #Immediate
            uint8_t immediate =  memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [LDY #Immediate]: %02X %02X", opcode, immediate);

            DEBUG_MSG_CPU("Writing 0x%02X to register Y", immediate);
            cpu->Y = immediate;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->Y);

            // Set cycles
            cpu->cycles = 2;
            break;
        }
        
        case 0xA4: { // LDY Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "LDY", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu); 
            DEBUG_MSG_CPU("Writing 0x%02X to register Y", value);
            cpu->Y = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->Y);

            // Set cycles
            cpu->cycles = 3;
            break;
        }
        
        case 0xB4: { // LDY Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "LDY", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu); 
            DEBUG_MSG_CPU("Writing 0x%02X to register Y", value);
            cpu->Y = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->Y);

            // Set cycles
            cpu->cycles = 4;
            break;
        }
        
        case 0xAC: { // LDY Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "LDY", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            DEBUG_MSG_CPU("Writing 0x%02X to register Y", value);
            cpu->Y = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->Y);

            // Set cycles
            cpu->cycles = 4;
            break;
        }
        
        case 0xBC: { // LDY Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "LDY", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            DEBUG_MSG_CPU("Writing 0x%02X to register Y", value);
            cpu->Y = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->Y);

            // Set cycles
            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        // ======== STY ======== 

        case 0x84: { // STY Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "STY", cpu, memory, ppu);
            memory_write(effective_addr, cpu->Y, memory, ppu);

            // Set cycles
            cpu->cycles = 3;
            break;
        }

        case 0x94: { // STY Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "STY", cpu, memory, ppu);
            memory_write(effective_addr, cpu->Y, memory, ppu);

            // Set cycles
            cpu->cycles = 4;
            break;
        }
        
        case 0x8C: { // STY Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "STY", cpu, memory, ppu);
            memory_write(effective_addr, cpu->Y, memory, ppu);

            // Set cycles
            cpu->cycles = 4;
            break;
        }

        // ==================== Transfer ==================== 

        case 0xAA: { // TAX Implied
            DEBUG_MSG_CPU("Executing instruction [TAX]: %02X", opcode);

            DEBUG_MSG_CPU("Writing 0x%02X to register X", cpu->A);
            cpu->X = cpu->A;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->X);

            // Set cycles
            cpu->cycles = 2;
            break;
        }
        
        case 0xA8: { // TAY Implied
            DEBUG_MSG_CPU("Executing instruction [TAY]: %02X", opcode);

            DEBUG_MSG_CPU("Writing 0x%02X to register Y", cpu->A);
            cpu->Y = cpu->A;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->Y);

            // Set cycles
            cpu->cycles = 2;
            break;
        }

        case 0xBA: { // TSX Implied
            DEBUG_MSG_CPU("Executing instruction [TSX]: %02X", opcode);

            DEBUG_MSG_CPU("Writing 0x%02X to register X", cpu->S);
            cpu->X = cpu->S;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->X);

            // Set cycles
            cpu->cycles = 2;
            break;
        }
        
        case 0x8A: { // TXA Implied
            DEBUG_MSG_CPU("Executing instruction [TXA]: %02X", opcode);

            DEBUG_MSG_CPU("Writing 0x%02X to register A", cpu->X);
            cpu->A = cpu->X;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 2;
            break;
        }
        
        case 0x9A: { // TXS Implied
            DEBUG_MSG_CPU("Executing instruction [TXS]: %02X", opcode);

            DEBUG_MSG_CPU("Writing 0x%02X to register S", cpu->X);
            cpu->S = cpu->X;
            
            // Set cycles
            cpu->cycles = 2;
            break;
        }
        
        case 0x98: { // TYA Implied
            DEBUG_MSG_CPU("Executing instruction [TYA]: %02X", opcode);

            DEBUG_MSG_CPU("Writing 0x%02X to register A", cpu->Y);
            cpu->A = cpu->Y;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);
            
            // Set cycles
            cpu->cycles = 2;
            break;
        }

        // ==================== Arithmetic ====================

        // ======== ADC ======== 

        case 0x69: { // ADC #Immediate
            uint8_t operand = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [ADC #Immediate]: %02X %02X", opcode, operand);

            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + operand + carry_in;

            cpu->P = (result > 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ result) & (operand ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 2;
            break;
        }

        case 0x65: { // ADC Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "ADC", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + operand + carry_in;

            cpu->P = (result > 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ result) & (operand ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 3;
            break;
        }

        case 0x75: { // ADC Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "ADC", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + operand + carry_in;

            cpu->P = (result > 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ result) & (operand ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 4;
            break;
        }

        case 0x6D: { // ADC Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "ADC", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + operand + carry_in;

            cpu->P = (result > 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ result) & (operand ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 4;
            break;
        }

        case 0x7D: { // ADC Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "ADC", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + operand + carry_in;

            cpu->P = (result > 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ result) & (operand ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        case 0x79: { // ADC Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "ADC", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + operand + carry_in;

            cpu->P = (result > 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ result) & (operand ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        case 0x61: { // ADC (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "ADC", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + operand + carry_in;

            cpu->P = (result > 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ result) & (operand ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 6;
            break;
        }

        case 0x71: { // ADC (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "ADC", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + operand + carry_in;

            cpu->P = (result > 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ result) & (operand ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 5;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        // ======== SBC ======== 

        case 0xEB: // Unofficial Opcode
        case 0xE9: { // SBC #Immediate
            uint8_t operand = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [SBC #Immediate]: %02X %02X", opcode, operand);

            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + ~operand + carry_in;

            cpu->P = (result <= 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ operand) & (cpu->A ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 2;
            break;
        }

        case 0xE5: { // SBC Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "SBC", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);

            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + ~operand + carry_in;

            cpu->P = (result <= 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ operand) & (cpu->A ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 3;
            break;
        }

        case 0xF5: { // SBC Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "SBC", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + ~operand + carry_in;

            cpu->P = (result <= 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ operand) & (cpu->A ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 4;
            break;
        }

        case 0xED: { // SBC Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "SBC", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + ~operand + carry_in;

            cpu->P = (result <= 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ operand) & (cpu->A ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 4;
            break;
        }

        case 0xFD: { // SBC Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "SBC", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + ~operand + carry_in;

            cpu->P = (result <= 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ operand) & (cpu->A ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        case 0xF9: { // SBC Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "SBC", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + ~operand + carry_in;

            cpu->P = (result <= 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ operand) & (cpu->A ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        case 0xE1: { // SBC (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "SBC", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + ~operand + carry_in;

            cpu->P = (result <= 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ operand) & (cpu->A ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 6;
            break;
        }

        case 0xF1: { // SBC (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "SBC", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + ~operand + carry_in;

            cpu->P = (result <= 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ operand) & (cpu->A ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 5;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        // ======== INC ======== 

        case 0xE6: { // INC Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "INC", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Increment value and write back to memory
            value++; 
            memory_write(effective_addr, value, memory, ppu);

            // Set flags
            update_zero_and_negative_flags(cpu, value);

            // Set cycles
            cpu->cycles = 5;
            break;
        }

        case 0xF6: { // INC Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "INC", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Increment value and write back to memory
            value++; 
            memory_write(effective_addr, value, memory, ppu);

            // Set flags
            update_zero_and_negative_flags(cpu, value);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        case 0xEE: { // INC Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "INC", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Increment value and write back to memory
            value++; 
            memory_write(effective_addr, value, memory, ppu);

            // Set flags
            update_zero_and_negative_flags(cpu, value);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        case 0xFE: { // INC Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "INC", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Increment value and write back to memory
            value++; 
            memory_write(effective_addr, value, memory, ppu);

            // Set flags
            update_zero_and_negative_flags(cpu, value);

            // Set cycles
            cpu->cycles = 7;
            break;
        }

        // ======== DEC ======== 

        case 0xC6: { // DEC Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "DEC", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // decrement value and write back to memory
            value--;
            memory_write(effective_addr, value, memory, ppu);

            update_zero_and_negative_flags(cpu, value);

            // Set cycles
            cpu->cycles = 5;
            break;
        }

        case 0xD6: { // DEC Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "DEC", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // decrement value and write back to memory
            value--;
            memory_write(effective_addr, value, memory, ppu);

            update_zero_and_negative_flags(cpu, value);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        case 0xCE: { // DEC Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "DEC", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // decrement value and write back to memory
            value--;
            memory_write(effective_addr, value, memory, ppu);

            update_zero_and_negative_flags(cpu, value);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        case 0xDE: { // DEC Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "DEC", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // decrement value and write back to memory
            value--;
            memory_write(effective_addr, value, memory, ppu);

            update_zero_and_negative_flags(cpu, value);

            // Set cycles
            cpu->cycles = 7;
            break;
        }

        // ======== INX ======== 

        case 0xE8: { // INX (Increment X)
            DEBUG_MSG_CPU("Executing instruction [INX]: %02X", opcode);

            DEBUG_MSG_CPU("Incrementing register X by 0x01");
            cpu->X += 0x01; // increment X by 1, does not modify carry or overflow

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->X);
            break;
        }

        // ======== DEX ======== 

        case 0xCA: { // DEX (Decrement X)
            DEBUG_MSG_CPU("Executing instruction [DEX]: %02X", opcode);

            DEBUG_MSG_CPU("Decrementing register X by 0x01");
            cpu->X -= 0x01; // decrement X by 1, does not modify carry or overflow

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->X);
            break;
        }

        // ======== INY ======== 

        case 0xC8: { // INY (Increment Y)
            DEBUG_MSG_CPU("Executing instruction [INY]: %02X", opcode);

            DEBUG_MSG_CPU("Incrementing register Y by 0x01");
            cpu->Y += 0x01; // increment Y by 1, does not modify carry or overflow

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->Y);
            break;
        }

        // ======== DEY ======== 

        case 0x88: { // DEY (Decrement Y)
            DEBUG_MSG_CPU("Executing instruction [DEY]: %02X", opcode);

            DEBUG_MSG_CPU("Decrementing register Y by 0x01");
            cpu->Y -= 0x01; // decrement Y by 1, does not modify carry or overflow

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->Y);
            break;
        }

        // ==================== Shift ====================

        // ======== LSR ======== 

        case 0x4A: { // LSR Accumulator
            DEBUG_MSG_CPU("Executing instruction [LSR Accumulator]: %02X", opcode);

            // Carry Flag
            cpu->P = (cpu->A & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Logical Shift Right
            cpu->A >>= 1;

            // Set flags
            cpu->P = (cpu->A == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P &= ~FLAG_NEGATIVE; // bit 8 is always cleared after shift

            // Set cycles
            cpu->cycles = 2;
            break;
        }

        case 0x46: { // LSR Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "LSR", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Logical Shift Right
            value >>= 1;

            memory_write(effective_addr, value, memory, ppu);

            // Set flags
            cpu->P = (value == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P &= ~FLAG_NEGATIVE; // bit 8 is always cleared after shift

            // Set cycles
            cpu->cycles = 5;
            break;
        }

        case 0x56: { // LSR Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "LSR", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Logical Shift Right
            value >>= 1;

            memory_write(effective_addr, value, memory, ppu);

            // Set flags
            cpu->P = (value == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P &= ~FLAG_NEGATIVE; // bit 8 is always cleared after shift

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        case 0x4E: { // LSR Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "LSR", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Logical Shift Right
            value >>= 1;

            memory_write(effective_addr, value, memory, ppu);

            // Set flags
            cpu->P = (value == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P &= ~FLAG_NEGATIVE; // bit 8 is always cleared after shift

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        case 0x5E: { // LSR Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "LSR", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Logical Shift Right
            value >>= 1;

            memory_write(effective_addr, value, memory, ppu);

            // Set flags
            cpu->P = (value == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P &= ~FLAG_NEGATIVE; // bit 8 is always cleared after shift

            // Set cycles
            cpu->cycles = 7;
            break;
        }

        // ======== ASL ======== 

        case 0x0A: { // ASL Accumulator
            DEBUG_MSG_CPU("Executing instruction [ASL Accumulator]: %02X", opcode);

            // Carry Flag
            cpu->P = (cpu->A & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Arithmetic Shift Left
            cpu->A <<= 1;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 2;
            break;
        }

        case 0x06: { // ASL Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "ASL", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Arithmetic Shift Left
            value <<= 1;

            // Set flags
            update_zero_and_negative_flags(cpu, value);

            // write back to memory
            memory_write(effective_addr, value, memory, ppu);

            // Set cycles
            cpu->cycles = 5;
            break;
        }

        case 0x16: { // ASL Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "ASL", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Arithmetic Shift Left
            value <<= 1;

            // Set flags
            update_zero_and_negative_flags(cpu, value);

            // write back to memory
            memory_write(effective_addr, value, memory, ppu);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        case 0x0E: { // ASL Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "ASL", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Arithmetic Shift Left
            value <<= 1;

            // Set flags
            update_zero_and_negative_flags(cpu, value);

            // write back to memory
            memory_write(effective_addr, value, memory, ppu);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        case 0x1E: { // ASL Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "ASL", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Arithmetic Shift Left
            value <<= 1;

            // Set flags
            update_zero_and_negative_flags(cpu, value);

            // write back to memory
            memory_write(effective_addr, value, memory, ppu);

            // Set cycles
            cpu->cycles = 7;
            break;
        }

        // ======== ROR ========

        case 0x6A: { // ROR Accumulator
            DEBUG_MSG_CPU("Executing instruction [ROR Accumulator]: %02X", opcode);
        
            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint8_t new_carry = cpu->A & 0x01;
        
            cpu->A >>= 1;
            cpu->A |= (old_carry << 7);
        
            cpu->P = (new_carry) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
        
            update_zero_and_negative_flags(cpu, cpu->A);
        
            cpu->cycles = 2;
            break;
        }

        case 0x66: { // ROR Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "ROR", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint8_t new_carry = value & 0x01;
        
            value >>= 1;
            value |= (old_carry << 7);
        
            memory_write(effective_addr, value, memory, ppu);
        
            cpu->P = (new_carry) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
        
            update_zero_and_negative_flags(cpu, value);
        
            cpu->cycles = 5;
            break;
        }        

        case 0x76: { // ROR Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "ROR", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint8_t new_carry = value & 0x01;
        
            value >>= 1;
            value |= (old_carry << 7);
        
            memory_write(effective_addr, value, memory, ppu);
        
            cpu->P = (new_carry) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
        
            update_zero_and_negative_flags(cpu, value);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        case 0x6E: { // ROR Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "ROR", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint8_t new_carry = value & 0x01;
        
            value >>= 1;
            value |= (old_carry << 7);
        
            memory_write(effective_addr, value, memory, ppu);
        
            cpu->P = (new_carry) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
        
            update_zero_and_negative_flags(cpu, value);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        case 0x7E: { // ROR Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "ROR", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint8_t new_carry = value & 0x01;
        
            value >>= 1;
            value |= (old_carry << 7);
        
            memory_write(effective_addr, value, memory, ppu);
        
            cpu->P = (new_carry) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
        
            update_zero_and_negative_flags(cpu, value);

            // Set cycles
            cpu->cycles = 7;
            break;
        }

        // ======== ROL ========

        case 0x2A: { // ROL Accumulator
            DEBUG_MSG_CPU("Executing instruction [ROL Accumulator]: %02X", opcode);

            // Save old carry to rotate in
            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 1 : 0;

            // Update carry with bit 7 of A
            cpu->P = (cpu->A & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Rotate left: shift and bring in old carry
            cpu->A = (cpu->A << 1) | old_carry;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 2;
            break;
        }

        case 0x26: { // ROL Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "ROL", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 1 : 0;
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            value = (value << 1) | old_carry;

            memory_write(effective_addr, value, memory, ppu);

            update_zero_and_negative_flags(cpu, value);

            // Set cycles
            cpu->cycles = 5;
            break;
        }

        case 0x36: { // ROL Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "ROL", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 1 : 0;
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            value = (value << 1) | old_carry;

            memory_write(effective_addr, value, memory, ppu);

            update_zero_and_negative_flags(cpu, value);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        case 0x2E: { // ROL Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "ROL", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 1 : 0;
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            value = (value << 1) | old_carry;

            memory_write(effective_addr, value, memory, ppu);

            update_zero_and_negative_flags(cpu, value);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        case 0x3E: { // ROL Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "ROL", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 1 : 0;
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            value = (value << 1) | old_carry;

            memory_write(effective_addr, value, memory, ppu);

            update_zero_and_negative_flags(cpu, value);

            // Set cycles
            cpu->cycles = 7;
            break;
        }

        // ==================== Bitwise ====================

        // ======== AND ======== 

        case 0x29: { // AND #Immediate
            uint8_t operand = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [AND #Immediate]: %02X %02X", opcode, operand);

            uint8_t value = cpu->A & operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 2;
            break;
        }

        case 0x25: { // AND Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "AND", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A & operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 3;
            break;
        }

        case 0x35: { // AND Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "AND", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A & operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            break;
        }

        case 0x2D: { // AND Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "AND", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A & operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            break;
        }

        case 0x3D: { // AND Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "AND", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A & operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        case 0x39: { // AND Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "AND", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A & operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        case 0x21: { // AND (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "AND", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A & operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        case 0x31: { // AND (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "AND", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A & operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 5;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        // ======== ORA ======== 

        case 0x09: { // ORA #Immedaite
            uint8_t operand = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [ORA #Immediate]: %02X %02X", opcode, operand);

            uint8_t value = cpu->A | operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 2;
            break;
        }

        case 0x05: { // ORA Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "ORA", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A | operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 3;
            break;
        }

        case 0x15: { // ORA Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "ORA", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A | operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            break;
        }

        case 0x0D: { // ORA Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "ORA", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A | operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            break;
        }

        case 0x1D: { // ORA Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "ORA", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A | operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        case 0x19: { // ORA Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "ORA", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A | operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        case 0x01: { // ORA (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "ORA", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A | operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        case 0x11: { // ORA (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "ORA", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A | operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 5;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        // ======== EOR ======== 

        case 0x49: { // EOR #Immedaite
            uint8_t operand = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [EOR #Immediate]: %02X %02X", opcode, operand);

            uint8_t value = cpu->A ^ operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 2;
            break;
        }

        case 0x45: { // EOR Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "EOR", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A ^ operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 3;
            break;
        }

        case 0x55: { // EOR Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "EOR", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A ^ operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            break;
        }

        case 0x4D: { // EOR Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "EOR", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A ^ operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            break;
        }

        case 0x5D: { // EOR Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "EOR", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A ^ operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        case 0x59: { // EOR Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "EOR", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A ^ operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        case 0x41: { // EOR (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "EOR", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A ^ operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        case 0x51: { // EOR (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "EOR", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);
            uint8_t value = cpu->A ^ operand;
            DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
            cpu->A = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 5;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        // ======== BIT ======== 

        case 0x24: { // BIT Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "BIT", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);

            // Perform bitwise AND 
            uint8_t result = cpu->A & operand;

            // Set flags
            cpu->P = (result == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (operand & 0x40) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);
            cpu->P = (operand & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);

            // Set cycles
            cpu->cycles = 3;
            break;
        }
        
        case 0x2C: { // BIT Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "BIT", cpu, memory, ppu);
            uint8_t operand = memory_read(effective_addr, memory, ppu);

            // Perform bitwise AND 
            uint8_t result = cpu->A & operand;

            // Set flags
            cpu->P = (result == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (operand & 0x40) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);
            cpu->P = (operand & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);

            // Set cycles
            cpu->cycles = 4;
            break;
        }

        // ==================== Compare ====================

        // ======== CMP ======== 

        case 0xC9: { // CMP #Immediate
            uint8_t value = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [CMP #Immediate]: %02X %02X", opcode, value);
        
            uint8_t result = cpu->A - value;
        
            // Set flags
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = ((result & 0xFF) == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
        
            cpu->cycles = 2;
            break;
        }

        case 0xC5: { // CMP Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "CMP", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            uint8_t result = cpu->A - value;

            // Set flags
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = ((result & 0xFF) == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);

            cpu->cycles = 3;
            break;
        }

        case 0xD5: { // CMP Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "CMP", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            uint8_t result = cpu->A - value;

            // Set flags
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = ((result & 0xFF) == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);

            cpu->cycles = 4;
            break;
        }

        case 0xCD: { // CMP Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "CMP", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            uint8_t result = cpu->A - value;
        
            // Set flags
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = ((result & 0xFF) == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
        
            cpu->cycles = 4;
            break;
        }

        case 0xDD: { // CMP Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "CMP", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            uint8_t result = cpu->A - value;
        
            // Set flags
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = ((result & 0xFF) == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
        
            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        case 0xD9: { // CMP Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "CMP", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            uint8_t result = cpu->A - value;
        
            // Set flags
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = ((result & 0xFF) == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
        
            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        case 0xC1: { // CMP (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "CMP", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            uint8_t result = cpu->A - value;
        
            // Set flags
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = ((result & 0xFF) == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
        
            cpu->cycles = 6;
            break;
        }

        case 0xD1: { // CMP (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "CMP", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            uint8_t result = cpu->A - value;
        
            // Set flags
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = ((result & 0xFF) == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
        
            cpu->cycles = 5;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        // ======== CPX ======== 

        case 0xE0: { // CPX #Immediate
            uint8_t value = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [CPX #Immediate]: %02X %02X", opcode, value);
        
            uint8_t result = cpu->X - value;
        
            // Set flags
            cpu->P = (cpu->X >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (cpu->X == value) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
        
            cpu->cycles = 2;
            break;
        }
        
        case 0xE4: { // CPX Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "CPX", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            uint8_t result = cpu->X - value;
        
            // Set flags
            cpu->P = (cpu->X >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (cpu->X == value) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
        
            cpu->cycles = 3;
            break;
        }
        
        case 0xEC: { // CPX Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "CPX", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            uint8_t result = cpu->X - value;
        
            // Set flags
            cpu->P = (cpu->X >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (cpu->X == value) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
        
            cpu->cycles = 4;
            break;
        }

        // ======== CPY ======== 

        case 0xC0: { // CPY #Immediate
            uint8_t value = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [CPY #Immediate]: %02X %02X", opcode, value);
        
            uint8_t result = cpu->Y - value;
        
            // Set flags
            cpu->P = (cpu->Y >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (cpu->Y == value) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
        
            cpu->cycles = 2;
            break;
        }
        
        case 0xC4: { // CPY Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "CPY", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            uint8_t result = cpu->Y - value;
        
            // Set flags
            cpu->P = (cpu->Y >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (cpu->Y == value) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
        
            cpu->cycles = 3;
            break;
        }
        
        case 0xCC: { // CPY Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "CPY", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
            uint8_t result = cpu->Y - value;
        
            // Set flags
            cpu->P = (cpu->Y >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (cpu->Y == value) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
        
            cpu->cycles = 4;
            break;
        }

        // ==================== Branch ====================

        case 0x90: { // BCC (Branch if Carry Clear)
            int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu); // signed int
            DEBUG_MSG_CPU("Executing instruction [BCC Relative]: %02X %02X", opcode, (uint8_t) offset);

            int cycles = 2;

            if (!(cpu->P & FLAG_CARRY)) { // branch if carry flag is clear
                uint16_t address = cpu->PC + offset;
                cycles++;
                if ((cpu->PC & 0xFF00) != (address & 0xFF00)) { // page crossed
                    cycles++; 
                }
                DEBUG_MSG_CPU("Branching to 0x%04X", address);
                cpu->PC = address;
            }

            // Set cycles
            cpu->cycles = cycles;
            break;
        }

        case 0xB0: { // BCS (Branch if Carry Set)
            int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu); // signed int
            DEBUG_MSG_CPU("Executing instruction [BCS Relative]: %02X %02X", opcode, (uint8_t) offset);

            int cycles = 2;

            if (cpu->P & FLAG_CARRY) { // branch if carry flag is set
                uint16_t address = cpu->PC + offset;
                cycles++;
                if ((cpu->PC & 0xFF00) != (address & 0xFF00)) { // page crossed
                    cycles++; 
                }
                DEBUG_MSG_CPU("Branching to 0x%04X", address);
                cpu->PC = address;
            }

            // Set cycles
            cpu->cycles = cycles;
            break;
        }

        case 0xF0: { // BEQ (Branch if Equal)
            int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu); // signed int
            DEBUG_MSG_CPU("Executing instruction [BEQ Relative]: %02X %02X", opcode, (uint8_t) offset);

            int cycles = 2;

            if (cpu->P & FLAG_ZERO) { // branch if zero flag is set
                uint16_t address = cpu->PC + offset;
                cycles++;
                if ((cpu->PC & 0xFF00) != (address & 0xFF00)) { // page crossed
                    cycles++; 
                }
                DEBUG_MSG_CPU("Branching to 0x%04X", address);
                cpu->PC = address;
            }

            // Set cycles
            cpu->cycles = cycles;
            break;
        }

        case 0x30: { // BMI (Branch if Minus)
            int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu); // signed int
            DEBUG_MSG_CPU("Executing instruction [BMI Relative]: %02X %02X", opcode, (uint8_t) offset);

            int cycles = 2;

            if (cpu->P & FLAG_NEGATIVE) { // branch if negative flag is set
                uint16_t address = cpu->PC + offset;
                cycles++;
                if ((cpu->PC & 0xFF00) != (address & 0xFF00)) { // page crossed
                    cycles++; 
                }
                DEBUG_MSG_CPU("Branching to 0x%04X", address);
                cpu->PC = address;
            }

            // Set cycles
            cpu->cycles = cycles;
            break;
        }

        case 0xD0: { // BNE (Branch if Not Equal)
            int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu); // signed int
            DEBUG_MSG_CPU("Executing instruction [BNE Relative]: %02X %02X", opcode, (uint8_t) offset);

            int cycles = 2;

            if (!(cpu->P & FLAG_ZERO)) { // branch if zero flag is clear
                uint16_t address = cpu->PC + offset;
                cycles++;
                if ((cpu->PC & 0xFF00) != (address & 0xFF00)) { // page crossed
                    cycles++; 
                }
                DEBUG_MSG_CPU("Branching to 0x%04X", address);
                cpu->PC = address;
            }

            // Set cycles
            cpu->cycles = cycles;
            break;
        }

        case 0x10: { // BPL (Branch if Plus)
            int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu); // signed int
            DEBUG_MSG_CPU("Executing instruction [BPL Relative]: %02X %02X", opcode, (uint8_t) offset);

            int cycles = 2;

            if (!(cpu->P & FLAG_NEGATIVE)) { // branch if negative flag is clear
                uint16_t address = cpu->PC + offset;
                cycles++;
                if ((cpu->PC & 0xFF00) != (address & 0xFF00)) { // page crossed
                    cycles++; 
                }
                DEBUG_MSG_CPU("Branching to 0x%04X", address);
                cpu->PC = address;
            }

            // Set cycles
            cpu->cycles = cycles;
            break;
        }

        case 0x50: { // BVC (Branch if Overflow Clear)
            int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu); // signed int
            DEBUG_MSG_CPU("Executing instruction [BVC Relative]: %02X %02X", opcode, (uint8_t) offset);

            int cycles = 2;

            if (!(cpu->P & FLAG_OVERFLOW)) { // branch if overflow flag is clear
                uint16_t address = cpu->PC + offset;
                cycles++;
                if ((cpu->PC & 0xFF00) != (address & 0xFF00)) { // page crossed
                    cycles++; 
                }
                DEBUG_MSG_CPU("Branching to 0x%04X", address);
                cpu->PC = address;
            }

            // Set cycles
            cpu->cycles = cycles;
            break;
        }

        case 0x70: { // BVS (Branch if Overflow Set)
            int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu); // signed int
            DEBUG_MSG_CPU("Executing instruction [BVS Relative]: %02X %02X", opcode, (uint8_t) offset);

            int cycles = 2;

            if (cpu->P & FLAG_OVERFLOW) { // branch if overflow flag is set
                uint16_t address = cpu->PC + offset;
                cycles++;
                if ((cpu->PC & 0xFF00) != (address & 0xFF00)) { // page crossed
                    cycles++; 
                }
                DEBUG_MSG_CPU("Branching to 0x%04X", address);
                cpu->PC = address;
            }

            // Set cycles
            cpu->cycles = cycles;
            break;
        }

        // ==================== Jump ====================

        // ======== JMP ======== 

        case 0x4C: { // JMP Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "JMP", cpu, memory, ppu);
            DEBUG_MSG_CPU("Jumping to address 0x%04X", effective_addr);
            cpu->PC = effective_addr;

            // Set cycles
            cpu->cycles = 3;
            break;
        }

        case 0x6C: { // JMP (Indirect)
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            uint16_t ptr = (high << 8) | low;

            // Emulate 6502 page boundary bug
            uint8_t jump_low = memory_read(ptr, memory, ppu);
            uint8_t jump_high;
            if ((ptr & 0x00FF) == 0x00FF) {
                // Wrap around same page
                jump_high = memory_read(ptr & 0xFF00, memory, ppu);
            } else {
                jump_high = memory_read(ptr + 1, memory, ppu);
            }

            cpu->PC = (jump_high << 8) | jump_low;

            DEBUG_MSG_CPU("Executing instruction [JMP (Indirect)]: %02X %02X %02X", opcode, low, high);

            // Set cycles
            cpu->cycles = 5;
            break;
        }
        
        // ======== JSR ======== 

        case 0x20: { // JSR (Jump to Subroutine)
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [JSR]: %02X %02X %02X", opcode, low, high);
            uint16_t new_addr = (high << 8) | low;

            uint16_t return_addr = cpu->PC - 1; // decrement PC because it is pointing at next instruction

            // Push return address to stack
            stack_push((return_addr >> 8) & 0xFF, cpu, memory, ppu); // high byte
            stack_push(return_addr & 0xFF, cpu, memory, ppu); // low byte

            // Jump to new address
            DEBUG_MSG_CPU("Jumping to address 0x%04X", new_addr);
            cpu->PC = new_addr;

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        // ======== RTS ======== 

        case 0x60: { // RTS (Return from Subroutine)
            DEBUG_MSG_CPU("Executing instruction [RTS]: %02X", opcode);

            // Pop address from top of the stack
            uint8_t low = stack_pop(cpu, memory, ppu);
            uint8_t high = stack_pop(cpu, memory, ppu);

            // Get return address
            uint16_t return_addr = (high << 8) | low;

            // Set PC to address + 1
            DEBUG_MSG_CPU("Returning to address 0x%04X", return_addr + 1);
            cpu->PC = return_addr + 1;

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        // ==================== Stack ====================

        // ======== PHP ======== 

        case 0x08: { // PHP (Push Processor Status)
            DEBUG_MSG_CPU("Executing instruction [PHP]: %02X", opcode);
            uint8_t status = cpu->P | FLAG_BREAK;
            stack_push(status, cpu, memory, ppu);

            cpu->cycles = 3;
            break;
        }

        // ======== PLP ======== 

        case 0x28: { // PLP (Pull Processor Status)
            DEBUG_MSG_CPU("Executing instruction [PLP]: %02X", opcode);
            uint8_t value = stack_pop(cpu, memory, ppu);

            value &= ~(1 << 4);     // Clear B flag
            value |= (1 << 5);      // Set unused bit to 1
            cpu->P = value;

            cpu->cycles = 4;
            break;
        }

        // ======== PLA ======== 

        case 0x68: { // Pull A
            DEBUG_MSG_CPU("Executing instruction [PLA]: %02X", opcode);
            cpu->A = stack_pop(cpu, memory, ppu);

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 4;
            break;
        }

        // ======== PHA ======== 

        case 0x48: { // Push A
            DEBUG_MSG_CPU("Executing instruction [PHA]: %02X", opcode);
            stack_push(cpu->A, cpu, memory, ppu);

            cpu->cycles = 3;
            break;
        }

        // ==================== Flags ====================

        case 0x18: { // CLC (Clear Carry)
            DEBUG_MSG_CPU("Executing instruction [CLC]: %02X", opcode);
            cpu->P &= ~FLAG_CARRY;
            break;
        }
        
        case 0x38: { // SEC (Set Carry)
            DEBUG_MSG_CPU("Executing instruction [SEC]: %02X", opcode);
            cpu->P |= FLAG_CARRY;
            break;
        }
        
        case 0xD8: { // CLD (Clear Decimal)
            DEBUG_MSG_CPU("Executing instruction [CLD]: %02X", opcode);
            cpu->P &= ~FLAG_DECIMAL;
            break;
        }

        case 0xF8: { // SED (Set Decimal)
            DEBUG_MSG_CPU("Executing instruction [SED]: %02X", opcode);
            cpu->P |= FLAG_DECIMAL;
            break;
        }

        case 0x58: { // CLI (Clear Interrupt Disable)
            DEBUG_MSG_CPU("Executing instruction [CLI]: %02X", opcode);
            cpu->P &= ~FLAG_INT;
            break;
        }

        case 0x78: { // SEI (Set Interrupt Disable)
            DEBUG_MSG_CPU("Executing instruction [SEI]: %02X", opcode);
            cpu->P |= FLAG_INT;
            break;
        }

        case 0xB8: { // CLV (Clear Overflow)
            DEBUG_MSG_CPU("Executing instruction [CLV]: %02X", opcode);
            cpu->P &= ~FLAG_OVERFLOW;
            break;
        }

        // ==================== Unofficial Opcodes ==================== 

        // ======== SLO ======== 

        case 0x07: { // SLO Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "SLO", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Arithmetic Shift Left
            value <<= 1;

            memory_write(effective_addr, value, memory, ppu);

            // OR result with A and store back in A
            cpu->A |= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            // set cycles
            cpu->cycles = 5;
            break;
        }

        case 0x17: { // SLO Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "SLO", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Arithmetic Shift Left
            value <<= 1;

            memory_write(effective_addr, value, memory, ppu);

            // OR result with A and store back in A
            cpu->A |= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            // set cycles
            cpu->cycles = 6;
            break;
        }

        case 0x0F: { // SLO Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "SLO", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Arithmetic Shift Left
            value <<= 1;

            memory_write(effective_addr, value, memory, ppu);

            // OR result with A and store back in A
            cpu->A |= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            // set cycles
            cpu->cycles = 6;
            break;
        }

        case 0x1F: { // SLO Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "SLO", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Arithmetic Shift Left
            value <<= 1;

            memory_write(effective_addr, value, memory, ppu);

            // OR result with A and store back in A
            cpu->A |= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            // set cycles
            cpu->cycles = 7;
            break;
        }

        case 0x1B: { // SLO Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "SLO", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Arithmetic Shift Left
            value <<= 1;

            memory_write(effective_addr, value, memory, ppu);

            // OR result with A and store back in A
            cpu->A |= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            // set cycles
            cpu->cycles = 7;
            break;
        }

        case 0x03: { // SLO (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "SLO", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Arithmetic Shift Left
            value <<= 1;

            memory_write(effective_addr, value, memory, ppu);

            // OR result with A and store back in A
            cpu->A |= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            // set cycles
            cpu->cycles = 8;
            break;
        }

        case 0x13: { // SLO (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "SLO", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Arithmetic Shift Left
            value <<= 1;

            memory_write(effective_addr, value, memory, ppu);

            // OR result with A and store back in A
            cpu->A |= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            // set cycles
            cpu->cycles = 8;
            break;
        }

        // ======== LAX ======== 

        case 0xAB: { // LAX Immediate (Highly Unstable)
            uint8_t value = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [LAX]: %02X %02X", opcode, value);
            cpu->A = cpu->X = value;
        
            update_zero_and_negative_flags(cpu, value);
            cpu->cycles = 2;
            break;
        }        

        case 0xA7: { // LAX Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "LAX", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            cpu->A = value;
            cpu->X = value;

            update_zero_and_negative_flags(cpu, value);

            cpu->cycles = 3;
            break;
        }

        case 0xB7: { // LAX Zero Page, Y
            uint16_t effective_addr = cpu_zero_page_y(opcode, "LAX", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            cpu->A = value;
            cpu->X = value;

            update_zero_and_negative_flags(cpu, value);

            cpu->cycles = 4;
            break;
        }

        case 0xAF: { // LAX Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "LAX", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            cpu->A = value;
            cpu->X = value;

            update_zero_and_negative_flags(cpu, value);

            cpu->cycles = 4;
            break;
        }

        case 0xBF: { // LAX Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "LAX", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            cpu->A = value;
            cpu->X = value;

            update_zero_and_negative_flags(cpu, value);

            cpu->cycles = 4;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        case 0xA3: { // LAX (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "LAX", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            cpu->A = value;
            cpu->X = value;

            update_zero_and_negative_flags(cpu, value);

            cpu->cycles = 6;
            break;
        }

        case 0xB3: { // LAX (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "LAX", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            cpu->A = value;
            cpu->X = value;

            update_zero_and_negative_flags(cpu, value);

            cpu->cycles = 5;
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        // ======== SAX ======== 

        case 0x87: { // SAX Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "SAX", cpu, memory, ppu);

            // Store A & X into memory
            uint8_t result = cpu->A & cpu->X;
            memory_write(effective_addr, result, memory, ppu);

            // Set cycles
            cpu->cycles = 3;
            break;
        }

        case 0x97: { // SAX Zero Page, Y
            uint16_t effective_addr = cpu_zero_page_y(opcode, "SAX", cpu, memory, ppu);
            
            // Store A & X into memory
            uint8_t result = cpu->A & cpu->X;
            memory_write(effective_addr, result, memory, ppu);

            // Set cycles
            cpu->cycles = 4;
            break;
        }

        case 0x8F: { // SAX Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "SAX", cpu, memory, ppu);
            
            // Store A & X into memory
            uint8_t result = cpu->A & cpu->X;
            memory_write(effective_addr, result, memory, ppu);

            // Set cycles
            cpu->cycles = 4;
            break;
        }

        case 0x83: { // SAX (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "SAX", cpu, memory, ppu);
            
            // Store A & X into memory
            uint8_t result = cpu->A & cpu->X;
            memory_write(effective_addr, result, memory, ppu);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        // ======== DCP ======== 

        case 0xC7: { // DCP Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "DCP", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            value--; // Decrement memory
            memory_write(effective_addr, value, memory, ppu);

            // Perform CMP (A - value)
            uint8_t result = cpu->A - value;

            // Set Carry Flag if A >= M
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            update_zero_and_negative_flags(cpu, result);

            cpu->cycles = 5;
            break;
        }

        case 0xD7: { // DCP Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "DCP", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            value--; // Decrement memory
            memory_write(effective_addr, value, memory, ppu);

            // Perform CMP (A - value)
            uint8_t result = cpu->A - value;

            // Set Carry Flag if A >= M
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            update_zero_and_negative_flags(cpu, result);

            cpu->cycles = 6;
            break;
        }

        case 0xCF: { // DCP Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "DCP", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            value--; // Decrement memory
            memory_write(effective_addr, value, memory, ppu);

            // Perform CMP (A - value)
            uint8_t result = cpu->A - value;

            // Set Carry Flag if A >= M
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            update_zero_and_negative_flags(cpu, result);

            cpu->cycles = 6;
            break;
        }

        case 0xDF: { // DCP Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "DCP", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            value--; // Decrement memory
            memory_write(effective_addr, value, memory, ppu);

            // Perform CMP (A - value)
            uint8_t result = cpu->A - value;

            // Set Carry Flag if A >= M
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            update_zero_and_negative_flags(cpu, result);

            cpu->cycles = 7;
            break;
        }

        case 0xDB: { // DCP Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "DCP", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            value--; // Decrement memory
            memory_write(effective_addr, value, memory, ppu);

            // Perform CMP (A - value)
            uint8_t result = cpu->A - value;

            // Set Carry Flag if A >= M
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            update_zero_and_negative_flags(cpu, result);

            cpu->cycles = 7;
            break;
        }

        case 0xC3: { // DCP (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "DCP", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            value--; // Decrement memory
            memory_write(effective_addr, value, memory, ppu);

            // Perform CMP (A - value)
            uint8_t result = cpu->A - value;

            // Set Carry Flag if A >= M
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            update_zero_and_negative_flags(cpu, result);

            cpu->cycles = 8;
            break;
        }

        case 0xD3: { // DCP (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "DCP", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            value--; // Decrement memory
            memory_write(effective_addr, value, memory, ppu);

            // Perform CMP (A - value)
            uint8_t result = cpu->A - value;

            // Set Carry Flag if A >= M
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            update_zero_and_negative_flags(cpu, result);

            cpu->cycles = 8;
            break;
        }

        // ======== ISC ======== 

        case 0xE7: { // ISC Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "ISC", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            value += 1; // Increment memory
            memory_write(effective_addr, value, memory, ppu);
        
            // Perform SBC with incremented value
            uint8_t inverted = ~value;
            uint16_t sum = cpu->A + inverted + (cpu->P & FLAG_CARRY ? 1 : 0);
        
            // Set flags
            cpu->P = (cpu->P & ~FLAG_CARRY) | (sum > 0xFF ? FLAG_CARRY : 0);
            cpu->P = (cpu->P & ~FLAG_OVERFLOW) | (((cpu->A ^ sum) & 0x80) && ((cpu->A ^ inverted) & 0x80) ? FLAG_OVERFLOW : 0);
        
            cpu->A = (uint8_t)sum;
        
            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 5;
            break;
        }

        case 0xF7: { // ISC Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "ISC", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            value += 1; // Increment memory
            memory_write(effective_addr, value, memory, ppu);
        
            // Perform SBC with incremented value
            uint8_t inverted = ~value;
            uint16_t sum = cpu->A + inverted + (cpu->P & FLAG_CARRY ? 1 : 0);
        
            // Set flags
            cpu->P = (cpu->P & ~FLAG_CARRY) | (sum > 0xFF ? FLAG_CARRY : 0);
            cpu->P = (cpu->P & ~FLAG_OVERFLOW) | (((cpu->A ^ sum) & 0x80) && ((cpu->A ^ inverted) & 0x80) ? FLAG_OVERFLOW : 0);
        
            cpu->A = (uint8_t)sum;
        
            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 6;
            break;
        }

        case 0xEF: { // ISC Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "ISC", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            value += 1; // Increment memory
            memory_write(effective_addr, value, memory, ppu);
        
            // Perform SBC with incremented value
            uint8_t inverted = ~value;
            uint16_t sum = cpu->A + inverted + (cpu->P & FLAG_CARRY ? 1 : 0);
        
            // Set flags
            cpu->P = (cpu->P & ~FLAG_CARRY) | (sum > 0xFF ? FLAG_CARRY : 0);
            cpu->P = (cpu->P & ~FLAG_OVERFLOW) | (((cpu->A ^ sum) & 0x80) && ((cpu->A ^ inverted) & 0x80) ? FLAG_OVERFLOW : 0);
        
            cpu->A = (uint8_t)sum;
        
            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 6;
            break;
        }

        case 0xFF: { // ISC Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "ISC", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            value += 1; // Increment memory
            memory_write(effective_addr, value, memory, ppu);
        
            // Perform SBC with incremented value
            uint8_t inverted = ~value;
            uint16_t sum = cpu->A + inverted + (cpu->P & FLAG_CARRY ? 1 : 0);
        
            // Set flags
            cpu->P = (cpu->P & ~FLAG_CARRY) | (sum > 0xFF ? FLAG_CARRY : 0);
            cpu->P = (cpu->P & ~FLAG_OVERFLOW) | (((cpu->A ^ sum) & 0x80) && ((cpu->A ^ inverted) & 0x80) ? FLAG_OVERFLOW : 0);
        
            cpu->A = (uint8_t)sum;
        
            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 7;
            break;
        }

        case 0xFB: { // ISC Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "ISC", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            value += 1; // Increment memory
            memory_write(effective_addr, value, memory, ppu);
        
            // Perform SBC with incremented value
            uint8_t inverted = ~value;
            uint16_t sum = cpu->A + inverted + (cpu->P & FLAG_CARRY ? 1 : 0);
        
            // Set flags
            cpu->P = (cpu->P & ~FLAG_CARRY) | (sum > 0xFF ? FLAG_CARRY : 0);
            cpu->P = (cpu->P & ~FLAG_OVERFLOW) | (((cpu->A ^ sum) & 0x80) && ((cpu->A ^ inverted) & 0x80) ? FLAG_OVERFLOW : 0);
        
            cpu->A = (uint8_t)sum;
        
            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 7;
            break;
        }

        case 0xE3: { // ISC (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "ISC", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            value += 1; // Increment memory
            memory_write(effective_addr, value, memory, ppu);
        
            // Perform SBC with incremented value
            uint8_t inverted = ~value;
            uint16_t sum = cpu->A + inverted + (cpu->P & FLAG_CARRY ? 1 : 0);
        
            // Set flags
            cpu->P = (cpu->P & ~FLAG_CARRY) | (sum > 0xFF ? FLAG_CARRY : 0);
            cpu->P = (cpu->P & ~FLAG_OVERFLOW) | (((cpu->A ^ sum) & 0x80) && ((cpu->A ^ inverted) & 0x80) ? FLAG_OVERFLOW : 0);
        
            cpu->A = (uint8_t)sum;
        
            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 8;
            break;
        }

        case 0xF3: { // ISC (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "ISC", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            value += 1; // Increment memory
            memory_write(effective_addr, value, memory, ppu);
        
            // Perform SBC with incremented value
            uint8_t inverted = ~value;
            uint16_t sum = cpu->A + inverted + (cpu->P & FLAG_CARRY ? 1 : 0);
        
            // Set flags
            cpu->P = (cpu->P & ~FLAG_CARRY) | (sum > 0xFF ? FLAG_CARRY : 0);
            cpu->P = (cpu->P & ~FLAG_OVERFLOW) | (((cpu->A ^ sum) & 0x80) && ((cpu->A ^ inverted) & 0x80) ? FLAG_OVERFLOW : 0);
        
            cpu->A = (uint8_t)sum;
        
            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 8;
            break;
        }

        // ======== RLA ======== 

        case 0x27: { // RLA Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "RLA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Rotate Left through Carry
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            value = (value << 1) | carry_in;

            memory_write(effective_addr, value, memory, ppu);

            // AND with accumulator
            cpu->A &= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 5;
            break;
        }

        case 0x37: { // RLA Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "RLA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Rotate Left through Carry
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            value = (value << 1) | carry_in;

            memory_write(effective_addr, value, memory, ppu);

            // AND with accumulator
            cpu->A &= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 6;
            break;
        }

        case 0x2F: { // RLA Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "RLA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Rotate Left through Carry
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            value = (value << 1) | carry_in;

            memory_write(effective_addr, value, memory, ppu);

            // AND with accumulator
            cpu->A &= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 6;
            break;
        }

        case 0x3F: { // RLA Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "RLA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Rotate Left through Carry
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            value = (value << 1) | carry_in;

            memory_write(effective_addr, value, memory, ppu);

            // AND with accumulator
            cpu->A &= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 7;
            break;
        }

        case 0x3B: { // RLA Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "RLA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Rotate Left through Carry
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            value = (value << 1) | carry_in;

            memory_write(effective_addr, value, memory, ppu);

            // AND with accumulator
            cpu->A &= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 7;
            break;
        }

        case 0x23: { // RLA (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "RLA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Rotate Left through Carry
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            value = (value << 1) | carry_in;

            memory_write(effective_addr, value, memory, ppu);

            // AND with accumulator
            cpu->A &= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 8;
            break;
        }

        case 0x33: { // RLA (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "RLA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Rotate Left through Carry
            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            value = (value << 1) | carry_in;

            memory_write(effective_addr, value, memory, ppu);

            // AND with accumulator
            cpu->A &= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 8;
            break;
        }

        // ======== SRE ======== 

        case 0x47: { // SRE Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "SRE", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag from bit 0 before shift
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Logical Shift Right
            value >>= 1;
            memory_write(effective_addr, value, memory, ppu);

            // EOR with accumulator
            cpu->A ^= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 5;
            break;
        }

        case 0x57: { // SRE Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "SRE", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag from bit 0 before shift
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Logical Shift Right
            value >>= 1;
            memory_write(effective_addr, value, memory, ppu);

            // EOR with accumulator
            cpu->A ^= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 6;
            break;
        }

        case 0x4F: { // SRE Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "SRE", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag from bit 0 before shift
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Logical Shift Right
            value >>= 1;
            memory_write(effective_addr, value, memory, ppu);

            // EOR with accumulator
            cpu->A ^= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 6;
            break;
        }

        case 0x5F: { // SRE Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "SRE", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag from bit 0 before shift
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Logical Shift Right
            value >>= 1;
            memory_write(effective_addr, value, memory, ppu);

            // EOR with accumulator
            cpu->A ^= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 7;
            break;
        }

        case 0x5B: { // SRE Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "SRE", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag from bit 0 before shift
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Logical Shift Right
            value >>= 1;
            memory_write(effective_addr, value, memory, ppu);

            // EOR with accumulator
            cpu->A ^= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 7;
            break;
        }

        case 0x43: { // SRE (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "SRE", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag from bit 0 before shift
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Logical Shift Right
            value >>= 1;
            memory_write(effective_addr, value, memory, ppu);

            // EOR with accumulator
            cpu->A ^= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 8;
            break;
        }

        case 0x53: { // SRE (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "SRE", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // Carry Flag from bit 0 before shift
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Logical Shift Right
            value >>= 1;
            memory_write(effective_addr, value, memory, ppu);

            // EOR with accumulator
            cpu->A ^= value;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 8;
            break;
        }

        // ======== RRA ======== 

        case 0x67: { // RRA Zero Page
            uint16_t effective_addr = cpu_zero_page(opcode, "RRA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            // Rotate Right
            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 0x80 : 0;
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            value = (value >> 1) | old_carry;
        
            memory_write(effective_addr, value, memory, ppu);
        
            // ADC logic
            uint16_t sum = cpu->A + value + ((cpu->P & FLAG_CARRY) ? 1 : 0);
            cpu->P = (cpu->P & ~FLAG_OVERFLOW) |
                     (((cpu->A ^ sum) & (value ^ sum) & 0x80) ? FLAG_OVERFLOW : 0);
            cpu->P = (cpu->P & ~FLAG_CARRY) |
                     ((sum > 0xFF) ? FLAG_CARRY : 0);
        
            cpu->A = sum & 0xFF;
            update_zero_and_negative_flags(cpu, cpu->A);
        
            cpu->cycles = 5;
            break;
        }
        
        case 0x77: { // RRA Zero Page, X
            uint16_t effective_addr = cpu_zero_page_x(opcode, "RRA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 0x80 : 0;
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            value = (value >> 1) | old_carry;
        
            memory_write(effective_addr, value, memory, ppu);
        
            uint16_t sum = cpu->A + value + ((cpu->P & FLAG_CARRY) ? 1 : 0);
            cpu->P = (cpu->P & ~FLAG_OVERFLOW) |
                     (((cpu->A ^ sum) & (value ^ sum) & 0x80) ? FLAG_OVERFLOW : 0);
            cpu->P = (cpu->P & ~FLAG_CARRY) |
                     ((sum > 0xFF) ? FLAG_CARRY : 0);
        
            cpu->A = sum & 0xFF;
            update_zero_and_negative_flags(cpu, cpu->A);
        
            cpu->cycles = 6;
            break;
        }
        
        case 0x6F: { // RRA Absolute
            uint16_t effective_addr = cpu_absolute(opcode, "RRA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 0x80 : 0;
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            value = (value >> 1) | old_carry;
        
            memory_write(effective_addr, value, memory, ppu);
        
            uint16_t sum = cpu->A + value + ((cpu->P & FLAG_CARRY) ? 1 : 0);
            cpu->P = (cpu->P & ~FLAG_OVERFLOW) |
                     (((cpu->A ^ sum) & (value ^ sum) & 0x80) ? FLAG_OVERFLOW : 0);
            cpu->P = (cpu->P & ~FLAG_CARRY) |
                     ((sum > 0xFF) ? FLAG_CARRY : 0);
        
            cpu->A = sum & 0xFF;
            update_zero_and_negative_flags(cpu, cpu->A);
        
            cpu->cycles = 6;
            break;
        }
        
        case 0x7F: { // RRA Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "RRA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 0x80 : 0;
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            value = (value >> 1) | old_carry;
        
            memory_write(effective_addr, value, memory, ppu);
        
            uint16_t sum = cpu->A + value + ((cpu->P & FLAG_CARRY) ? 1 : 0);
            cpu->P = (cpu->P & ~FLAG_OVERFLOW) |
                     (((cpu->A ^ sum) & (value ^ sum) & 0x80) ? FLAG_OVERFLOW : 0);
            cpu->P = (cpu->P & ~FLAG_CARRY) |
                     ((sum > 0xFF) ? FLAG_CARRY : 0);
        
            cpu->A = sum & 0xFF;
            update_zero_and_negative_flags(cpu, cpu->A);
        
            cpu->cycles = 7;
            break;
        }
        
        case 0x7B: { // RRA Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "RRA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 0x80 : 0;
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            value = (value >> 1) | old_carry;
        
            memory_write(effective_addr, value, memory, ppu);
        
            uint16_t sum = cpu->A + value + ((cpu->P & FLAG_CARRY) ? 1 : 0);
            cpu->P = (cpu->P & ~FLAG_OVERFLOW) |
                     (((cpu->A ^ sum) & (value ^ sum) & 0x80) ? FLAG_OVERFLOW : 0);
            cpu->P = (cpu->P & ~FLAG_CARRY) |
                     ((sum > 0xFF) ? FLAG_CARRY : 0);
        
            cpu->A = sum & 0xFF;
            update_zero_and_negative_flags(cpu, cpu->A);
        
            cpu->cycles = 7;
            break;
        }
        
        case 0x63: { // RRA (Indirect, X)
            uint16_t effective_addr = cpu_indirect_x(opcode, "RRA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 0x80 : 0;
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            value = (value >> 1) | old_carry;
        
            memory_write(effective_addr, value, memory, ppu);
        
            uint16_t sum = cpu->A + value + ((cpu->P & FLAG_CARRY) ? 1 : 0);
            cpu->P = (cpu->P & ~FLAG_OVERFLOW) |
                     (((cpu->A ^ sum) & (value ^ sum) & 0x80) ? FLAG_OVERFLOW : 0);
            cpu->P = (cpu->P & ~FLAG_CARRY) |
                     ((sum > 0xFF) ? FLAG_CARRY : 0);
        
            cpu->A = sum & 0xFF;
            update_zero_and_negative_flags(cpu, cpu->A);
        
            cpu->cycles = 8;
            break;
        }
        
        case 0x73: { // RRA (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "RRA", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu);
        
            uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 0x80 : 0;
            cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            value = (value >> 1) | old_carry;
        
            memory_write(effective_addr, value, memory, ppu);
        
            uint16_t sum = cpu->A + value + ((cpu->P & FLAG_CARRY) ? 1 : 0);
            cpu->P = (cpu->P & ~FLAG_OVERFLOW) |
                     (((cpu->A ^ sum) & (value ^ sum) & 0x80) ? FLAG_OVERFLOW : 0);
            cpu->P = (cpu->P & ~FLAG_CARRY) |
                     ((sum > 0xFF) ? FLAG_CARRY : 0);
        
            cpu->A = sum & 0xFF;
            update_zero_and_negative_flags(cpu, cpu->A);
        
            cpu->cycles = 8;
            break;
        }
        
        // ======== ANC ======== 
        
        case 0x0B: 
        case 0x2B: { // ANC Immediate
            uint8_t value = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [ANC]: %02X %02X", opcode, value);
            cpu->A &= value;

            // Set Carry to bit 7 of result
            cpu->P = (cpu->A & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 2;
            break;
        }

        // ======== ALR ======== 

        case 0x4B: { // ALR Immediate
            uint8_t value = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [ALR]: %02X %02X", opcode, value);
            cpu->A &= value;
        
            // Set Carry to bit 0 before shift
            cpu->P = (cpu->A & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
        
            // Logical Shift Right
            cpu->A >>= 1;
        
            update_zero_and_negative_flags(cpu, cpu->A);
            cpu->cycles = 2;
            break;
        }

        // ======== ARR ======== 
        
        case 0x6B: { // ARR Immediate
            uint8_t value = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [ARR]: %02X %02X", opcode, value);
            cpu->A &= value;
        
            // Rotate Right through Carry
            uint8_t carry = (cpu->P & FLAG_CARRY) ? 0x80 : 0x00;
            uint8_t result = (cpu->A >> 1) | carry;
            cpu->A = result;
        
            // Set Carry to bit 6, Overflow from bits 6 and 5
            cpu->P = (result & 0x40) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = ((result ^ (result << 1)) & 0x40) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);
        
            update_zero_and_negative_flags(cpu, cpu->A);
            cpu->cycles = 2;
            break;
        }

        // ======== XAA ======== 

        case 0x8B: { // XAA Immediate (highly unstable)
            uint8_t value = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [XXA]: %02X %02X", opcode, value);
        
            // Output is AND of A, X, and immediate
            cpu->A = (cpu->A & cpu->X) & value;
        
            update_zero_and_negative_flags(cpu, cpu->A);
            cpu->cycles = 2;
            break;
        }

        // ======== AXS ======== 

        case 0xCB: { // AXS Immediate
            uint8_t value = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [AXS]: %02X %02X", opcode, value);
            uint8_t result = cpu->X & cpu->A;
            result -= value;
        
            // Set Carry if result did not borrow
            cpu->P = (cpu->X & cpu->A) >= value ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->X = result;
        
            update_zero_and_negative_flags(cpu, result);
            cpu->cycles = 2;
            break;
        }   
        
        // ======== AHX ======== 
        
        case 0x93: { // AHX (Indirect), Y
            uint16_t effective_addr = cpu_indirect_y(opcode, "AHX", cpu, memory, ppu);
            uint16_t temp = ((effective_addr >> 8) + 1) & 0xFF;
            uint8_t value = cpu->A & cpu->X & temp;
            memory_write(effective_addr, value, memory, ppu);
            cpu->cycles = 8;
            break;
        }    
        
        // ======== SHY ======== 

        case 0x9C: { // SHY Absolute, X
            uint16_t effective_addr = cpu_absolute_x(opcode, "SHY", cpu, memory, ppu);
            uint8_t value = cpu->Y & ((effective_addr >> 8) + 1);
            memory_write(effective_addr, value, memory, ppu);
            cpu->cycles = 5;
            break;
        }

        // ======== AHX ======== 

        case 0x9F: { // AHX Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "AHX", cpu, memory, ppu);
            uint8_t value = cpu->A & cpu->X & ((effective_addr >> 8) + 1);
            memory_write(effective_addr, value, memory, ppu);
            cpu->cycles = 5;
            break;
        }

        // ======== SHX ======== 
        
        case 0x9E: { // SHX Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "SHX", cpu, memory, ppu);
            uint8_t value = cpu->X & ((effective_addr >> 8) + 1);
            memory_write(effective_addr, value, memory, ppu);
            cpu->cycles = 5;
            break;
        }

        // ======== TAS ======== 
        
        case 0x9B: { // TAS Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "TAS", cpu, memory, ppu);
            cpu->S = cpu->A & cpu->X;
            uint8_t value = cpu->S & ((effective_addr >> 8) + 1);
            memory_write(effective_addr, value, memory, ppu);
            cpu->cycles = 5;
            break;
        }

        // ======== LAS ======== 

        case 0xBB: { // LAS Absolute, Y
            uint16_t effective_addr = cpu_absolute_y(opcode, "LAS", cpu, memory, ppu);
            uint8_t value = memory_read(effective_addr, memory, ppu) & cpu->S;
            cpu->A = cpu->X = cpu->S = value;
            update_zero_and_negative_flags(cpu, value);
            cpu->cycles = 4;
            break;
        }        

        // ======== NOP ======== 

        case 0x1A:
        case 0x3A: 
        case 0x5A:
        case 0x7A:
        case 0xDA:
        case 0xFA: { // NOP Implied
            DEBUG_MSG_CPU("Executing instruction [NOP Implied]: %02X", opcode);
            cpu->cycles = 2;
            break;
        }

        case 0x80:
        case 0x89:
        case 0x82:
        case 0xC2:
        case 0xE2: { //  NOP #Immediate
            uint8_t immediate = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [NOP #Immediate]: %02X %02X", opcode, immediate);
            cpu->cycles = 2;
            break;
        }

        case 0x04:
        case 0x44:
        case 0x64: { // NOP Zero Page
            cpu_zero_page(opcode, "NOP", cpu, memory, ppu);
            cpu->cycles = 3;
            break;
        }

        case 0x0C: { // NOP Absolute 
            cpu_absolute(opcode, "NOP", cpu, memory, ppu);
            cpu->cycles = 4;
            break;
        }

        case 0x14:
        case 0x34:
        case 0x54:
        case 0x74:
        case 0xD4:
        case 0xF4: { // NOP Zero Page, X
            cpu_zero_page_x(opcode, "NOP", cpu, memory, ppu);
            cpu->cycles = 4;
            break;
        }

        case 0x1C:
        case 0x3C:
        case 0x5C:
        case 0x7C:
        case 0xDC:
        case 0xFC: { // NOP Absolute, X
            cpu_absolute_x(opcode, "NOP", cpu, memory, ppu);
            cpu->cycles = 4;   
            if (cpu->page_crossed) {
                cpu->cycles += 1;
            }
            break;
        }

        // ======== INV ======== 

        case 0x02:
        case 0x22:
        case 0x42:
        case 0x62:
        case 0x12:
        case 0x32:
        case 0x52:
        case 0x72:
        case 0x92:
        case 0xB2:
        case 0xD2:
        case 0xF2: {
            cpu->PC++;
            cpu->cycles = 0;
            break;
        }

        // ==================== Other ====================

        case 0xEA: { // NOP 
            DEBUG_MSG_CPU("Executing instruction [NOP]: %02X", opcode);
            cpu->cycles = 2;
            break;
        }

        case 0x00: { // BRK
            DEBUG_MSG_CPU("Executing instruction [BRK]: %02X", opcode);

            // The return address is PC + 2
            uint16_t return_addr = cpu->PC + 2;

            // push PC to stack
            stack_push((return_addr >> 8) & 0xFF, cpu, memory, ppu); // high byte
            stack_push(return_addr & 0xFF, cpu, memory, ppu);        // low byte

            // Push status register with Break flag set
            uint8_t status_with_B = cpu->P | FLAG_BREAK | FLAG_UNUSED; 
            stack_push(status_with_B, cpu, memory, ppu);

            // Set Interrupt Disable flag
            cpu->P |= FLAG_INT; 

            // Load new PC from IRQ vector at 0xFFFE/0xFFFF
            uint8_t low = memory_read(0xFFFE, memory, ppu);
            uint8_t high = memory_read(0xFFFF, memory, ppu);
            cpu->PC = (high << 8) | low;

            // Set cycles
            cpu->cycles = 7;
            break;
        }

        case 0x40: { // RTI
            DEBUG_MSG_CPU("Executing instruction [RTI]: %02X", opcode);

            // Pop status flag from stack
            uint8_t status = stack_pop(cpu, memory, ppu);
            status |= FLAG_UNUSED;
            cpu->P = status;

            // Pop address from top of the stack
            uint8_t low = stack_pop(cpu, memory, ppu);
            uint8_t high = stack_pop(cpu, memory, ppu);

            // Get return address
            uint16_t return_addr = (high << 8) | low;

            // Set PC to address
            DEBUG_MSG_CPU("Returning to address 0x%04X", return_addr);
            cpu->PC = return_addr;

            // Set cycles
            cpu->cycles = 6;

            ppu->nmi = 0; // reset ppu's NMI flag
            cpu->service_int = 0; // reset service flag
            break;
        }
        
        default: 
            FATAL_ERROR("CPU", "Missing Opcode [%02X]", opcode);
            break;
    }
}

void cpu_irq(CPU *cpu, uint8_t *memory, PPU *ppu) { // HARDWARE interrupts 
    if (!(cpu->P & FLAG_INT)) {
        DEBUG_MSG_CPU("Hardware Interrupt Triggered");

        // The return address is PC
        uint16_t return_addr = cpu->PC;

        // push PC to stack
        stack_push((return_addr >> 8) & 0xFF, cpu, memory, ppu); // high byte
        stack_push(return_addr & 0xFF, cpu, memory, ppu);        // low byte

        // Push status register with Break flag set
        uint8_t status_no_B = (cpu->P & ~FLAG_BREAK) | FLAG_UNUSED; 
        stack_push(status_no_B, cpu, memory, ppu);

        // Set Interrupt Disable flag
        cpu->P |= FLAG_INT; 

        // Load new PC from IRQ vector at 0xFFFE/0xFFFF
        uint8_t low = memory_read(0xFFFE, memory, ppu);
        uint8_t high = memory_read(0xFFFF, memory, ppu);
        cpu->PC = (high << 8) | low;

        // Set cycles
        cpu->cycles = 7;
    }
}

void cpu_nmi(CPU *cpu, uint8_t *memory, PPU *ppu) { // Non-Maskable Interrupt
    DEBUG_MSG_CPU("NMI Triggered");

    cpu->service_int = 1; // signal that the interrupt is being serviced

    // The return address is PC
    uint16_t return_addr = cpu->PC;

    // push PC to stack
    stack_push((return_addr >> 8) & 0xFF, cpu, memory, ppu); // high byte
    stack_push(return_addr & 0xFF, cpu, memory, ppu);        // low byte

    uint8_t status = cpu->P & ~FLAG_BREAK;
    status |= FLAG_UNUSED; 
    stack_push(status, cpu, memory, ppu);

    // Set Interrupt Disable flag
    cpu->P |= FLAG_INT; 

    // Load new PC from IRQ vector at 0xFFFA/0xFFFB
    uint8_t low = memory_read(0xFFFA, memory, ppu);
    uint8_t high = memory_read(0xFFFB, memory, ppu);
    cpu->PC = (high << 8) | low;

    // Set cycles
    cpu->cycles = 8;
}

// ======================= Addressing Modes =======================

uint16_t cpu_zero_page(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu) {
    uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [%s ZeroPage]: %02X %02X", instruction, opcode, zero_addr);

    uint16_t effective_addr = (uint16_t) zero_addr;

    return effective_addr;
}

uint16_t cpu_zero_page_x(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu) {
    uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [%s ZeroPage, X]: %02X %02X", instruction, opcode, zero_addr);

    uint8_t wrapped_addr = (zero_addr + cpu->X) & 0xFF; // wraps around 0x00 - 0xFF
    uint16_t effective_addr = (uint16_t) wrapped_addr; 

    return effective_addr;
}

uint16_t cpu_zero_page_y(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu) {
    uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [%s ZeroPage, Y]: %02X %02X", instruction, opcode, zero_addr);

    uint8_t wrapped_addr = (zero_addr + cpu->Y) & 0xFF; // wraps around 0x00 - 0xFF
    uint16_t effective_addr = (uint16_t) wrapped_addr; 

    return effective_addr;
}

uint16_t cpu_absolute(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu) {
    uint8_t low = memory_read(cpu->PC++, memory, ppu);
    uint8_t high = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [%s Absolute]: %02X %02X %02X", instruction, opcode, low, high);

    uint16_t effective_addr = (uint16_t) (high << 8 | low);

    return effective_addr;
}

uint16_t cpu_absolute_x(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu) {
    uint8_t low = memory_read(cpu->PC++, memory, ppu);
    uint8_t high = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [%s Absolute, X]: %02X %02X %02X", instruction, opcode, low, high);

    uint16_t base_addr = (uint16_t) (high << 8 | low);
    uint16_t effective_addr = base_addr + cpu->X;

    if ((base_addr & 0xFF00) != (effective_addr & 0xFF00)) {
        cpu->page_crossed = 1; // page boundary crossed
    }

    return effective_addr;
}

uint16_t cpu_absolute_y(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu) {
    uint8_t low = memory_read(cpu->PC++, memory, ppu);
    uint8_t high = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [%s Absolute, Y]: %02X %02X %02X", instruction, opcode, low, high);

    uint16_t base_addr = (uint16_t) (high << 8 | low);
    uint16_t effective_addr = base_addr + cpu->Y;

    if ((base_addr & 0xFF00) != (effective_addr & 0xFF00)) {
        cpu->page_crossed = 1; // page boundary crossed
    }

    return effective_addr;
}

uint16_t cpu_indirect_x(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu) {
    uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [%s (Indirect, X)]: %02X %02X", instruction, opcode, zero_addr);

    // calculate full zero page address
    uint8_t wrapped_addr = (zero_addr + cpu->X) & 0xFF; 
    uint16_t addr = (uint16_t) wrapped_addr;

    // get store location address from memory
    uint8_t low = memory_read(addr, memory, ppu);
    uint8_t high = memory_read((addr + 1) & 0xFF, memory, ppu);
    uint16_t effective_addr = (high << 8) | low;

    return effective_addr;
}

uint16_t cpu_indirect_y(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu) {
    uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [%s (Indirect), Y]: %02X %02X", instruction, opcode, zero_addr);

    // calculate full zero page address
    uint16_t addr = (uint16_t) zero_addr;

    // get store location address from memory
    uint8_t low = memory_read(addr, memory, ppu);
    uint8_t high = memory_read((addr + 1) & 0xFF, memory, ppu);
    uint16_t base_addr = (high << 8) | low;
    uint16_t effective_addr = base_addr + cpu->Y;

    if ((base_addr & 0xFF00) != (effective_addr & 0xFF00)) {
        cpu->page_crossed = 1; // page boundary crossed
    }

    return effective_addr;
}

void update_zero_and_negative_flags(CPU* cpu, uint8_t value) {
    cpu->P = (value == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
    cpu->P = (value & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
}

void cpu_dump_registers(CPU *cpu) {
    DEBUG_MSG_CPU("A: %02X  X: %02X  Y: %02X  P: %02X  S: %02X  PC: %04X",
           cpu->A, cpu->X, cpu->Y, cpu->P, cpu->S, cpu->PC);
}