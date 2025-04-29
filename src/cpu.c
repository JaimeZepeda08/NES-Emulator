#include <stdio.h>
#include <stdlib.h>
#include "../include/cpu.h"
#include "../include/log.h"
#include "../include/memory.h"
#include "../include/ppu.h"

uint16_t cpu_zero_page(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu);
uint16_t cpu_zero_page_x(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu);
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
    cpu->PC = 0xC000; // memory[0xFFFC] | (memory[0xFFFD] << 8); // reset vector at 0xFFFC and 0xFFFD (little endian) 0xC000; 
    cpu->S = 0xFD; 
    cpu->P = 0x34; // 0011 0100

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
        // cpu_nmi(cpu, memory, ppu);
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
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [STA Absolute]: %02X %02X %02X", opcode, low, high);

            uint16_t address = (uint16_t) (high << 8 | low);
            memory_write(address, cpu->A, memory, ppu);

            // Set cycles
            cpu->cycles = 4;
            break;
        }
        
        case 0x9D: { // STA Absolute, X
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [STA Absolute, X]: %02X %02X %02X", opcode, low, high);

            uint16_t address = (uint16_t) (high << 8 | low) + cpu->X;
            memory_write(address, cpu->A, memory, ppu);

            // Set cycles
            cpu->cycles = 5;
            break;
        }
        
        case 0x99: { // STA Absolute, Y
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [STA Absolute, Y]: %02X %02X %02X", opcode, low, high);

            uint16_t address = (uint16_t) (high << 8 | low) + cpu->Y;
            memory_write(address, cpu->A, memory, ppu);

            // Set cycles
            cpu->cycles = 5;
            break;
        }

        case 0x81: { // STA (Indirect, X)
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [STA (Indirect, X)]: %02X %02X", opcode, zero_addr);

            // calculate full zero page address
            uint8_t wrapped_addr = (zero_addr + cpu->X) & 0xFF; 
            uint16_t addr = (uint16_t) wrapped_addr;

            // get store location address from memory
            uint8_t low = memory_read(addr, memory, ppu);
            uint8_t high = memory_read((addr + 1) & 0xFF, memory, ppu);
            uint16_t effective_addr = (high << 8) | low;

            // Store A at effective addres
            memory_write(effective_addr, cpu->A, memory, ppu);

            // Set cycles
            cpu->cycles = 6;
            break;
        }
        
        case 0x91: { // STA (Indirect), Y
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [STA (Indirect), Y]: %02X %02X", opcode, zero_addr);

            // calculate full zero page address
            uint16_t addr = (uint16_t) zero_addr;

            // get store location address from memory
            uint8_t low = memory_read(addr, memory, ppu);
            uint8_t high = memory_read((addr + 1) & 0xFF, memory, ppu);
            uint16_t effective_addr = ((high << 8) | low) + cpu->Y;

            // Store A at effective addres
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
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [LDX ZeroPage]: %02X %02X", opcode, zero_addr);

            // get effective addresss
            uint16_t effective_addr = (uint16_t) zero_addr;

            // get value stored in zero page
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
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [LDX ZeroPage, Y]: %02X %02X", opcode, zero_addr);

            // get effective addresss
            uint8_t wrapped_addr = (zero_addr + cpu->Y) & 0xFF; // wraps around 0x00 - 0xFF
            uint16_t effective_addr = (uint16_t) wrapped_addr; 

            // get value stored in zero page
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
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [LDX Absolute]: %02X %02X %02X", opcode, low, high);

            uint16_t address = (uint16_t) (high << 8 | low);
            uint8_t value = memory_read(address, memory, ppu);
            DEBUG_MSG_CPU("Writing 0x%02X to register X", value);
            cpu->X = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->X);

            // Set cycles
            cpu->cycles = 4;
            break;
        }
        
        // case 0xBE: // LDX Absolute, Y
        //     break;

        // // ======== STX ======== 

        case 0x86: { // STX Zero Page
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [STX ZeroPage]: %02X %02X", opcode, zero_addr);

            uint16_t effective_addr = (uint16_t) zero_addr;

            // store X at address
            memory_write(effective_addr, cpu->X, memory, ppu);

            // Set cycles
            cpu->cycles = 3;
            break;
        }

        // case 0x96: { // STX Zero Page, Y
        //     // Set cycles
        //     cpu->cycles = 4;
        //     break;
        // }
        
        case 0x8E: { // STX Absolute
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [STX Absolute]: %02X %02X %02X", opcode, low, high);

            uint16_t address = (uint16_t) (high << 8 | low);
            memory_write(address, cpu->X, memory, ppu);

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
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [LDY ZeroPage]: %02X %02X", opcode, zero_addr);

            // get effective addresss
            uint16_t effective_addr = (uint16_t) zero_addr;

            // get value stored in zero page
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
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [LDY ZeroPage, X]: %02X %02X", opcode, zero_addr);

            // get effective addresss
            uint8_t wrapped_addr = (zero_addr + cpu->X) & 0xFF; // wraps around 0x00 - 0xFF
            uint16_t effective_addr = (uint16_t) wrapped_addr; 

            // get value stored in zero page
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
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [LDY Absolute]: %02X %02X %02X", opcode, low, high);

            uint16_t address = (uint16_t) (high << 8 | low);
            uint8_t value = memory_read(address, memory, ppu);
            DEBUG_MSG_CPU("Writing 0x%02X to register Y", value);
            cpu->Y = value;

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->Y);

            // Set cycles
            cpu->cycles = 4;
            break;
        }
        
        // case 0xBC: // LDY Absolute, X
        //     break;

        // ======== STY ======== 

        case 0x84: { // STY Zero Page
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [STY ZeroPage]: %02X %02X", opcode, zero_addr);

            // get effective addresss
            uint16_t address = (uint16_t) zero_addr;

            // write register Y to zero page
            memory_write(address, cpu->Y, memory, ppu);

            // Set cycles
            cpu->cycles = 3;
            break;
        }

        case 0x94: { // STY Zero Page, X
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [STY ZeroPage, X]: %02X %02X", opcode, zero_addr);

            // get effective addresss
            uint8_t wrapped_addr = (zero_addr + cpu->X) & 0xFF; // wraps around 0x00 - 0xFF
            uint16_t address = (uint16_t) wrapped_addr; 

            // write register Y to zero page
            memory_write(address, cpu->Y, memory, ppu);

            // Set cycles
            cpu->cycles = 4;
            break;
        }
        
        case 0x8C: { // STY Absolute
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [STY Absolute]: %02X %02X %02X", opcode, low, high);

            uint16_t address = (uint16_t) (high << 8 | low);
            memory_write(address, cpu->Y, memory, ppu);

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
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [ADC ZeroPage]: %02X %02X", opcode, zero_addr);

            uint8_t effective_addr = (uint16_t) zero_addr;
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

        // case 0x75: { // ADC Zero Page

        // }

        // ======== SBC ======== 

        case 0xE9: { // SBC #Immediate
            uint8_t operand = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [SBC #Immediate]: %02X %02X", opcode, operand);

            uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
            uint16_t result = cpu->A + ~operand + carry_in;

            cpu->P = (result > 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = (((cpu->A ^ result) & (operand ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

            cpu->A = (uint8_t) result;

            update_zero_and_negative_flags(cpu, cpu->A);

            cpu->cycles = 2;
            break;
        }

        // ======== INC ======== 

        case 0xE6: { // INC Zero Page
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [INC ZeroPage]: %02X %02X", opcode, zero_addr);
            uint16_t effective_addr = (uint16_t) zero_addr;

            // Get value from memory
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
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [INC ZeroPage, X]: %02X %02X", opcode, zero_addr);

            // get effective addresss
            uint8_t wrapped_addr = (zero_addr + cpu->X) & 0xFF; // wraps around 0x00 - 0xFF
            uint16_t effective_addr = (uint16_t) wrapped_addr; 

            // Get value from memory
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
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [INC Absolute]: %02X %02X %02X", opcode, low, high);
            uint16_t effective_addr = (high << 8) | low;

            // Get value from memory
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
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [INC Absolute, X]: %02X %02X %02X", opcode, low, high);
            uint16_t effective_addr = ((high << 8) | low) + cpu->X;

            // Get value from memory
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
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [DEC ZeroPage]: %02X %02X", opcode, zero_addr);

            uint16_t effective_addr = (uint16_t) zero_addr;
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // decrement value and write back to memory
            value--;
            memory_write(effective_addr, value, memory, ppu);

            // Set cycles
            cpu->cycles = 5;
            break;
        }

        case 0xD6: { // DEC Zero Page, X
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [DEC ZeroPage, X]: %02X %02X", opcode, zero_addr);

            uint8_t wrapped_addr = (zero_addr + cpu->X) & 0xFF; // wraps around 0x00 - 0xFF
            uint16_t effective_addr = (uint16_t) wrapped_addr; 
            uint8_t value = memory_read(effective_addr, memory, ppu);

            // decrement value and write back to memory
            value--;
            memory_write(effective_addr, value, memory, ppu);

            // Set cycles
            cpu->cycles = 6;
            break;
        }

        // case 0xCE: { // DEC Absolute
        //     // Set cycles
        //     cpu->cycles = 6;
        //     break;
        // }

        // case 0xDE: { // DEC Absolute, X
        //     // Set cycles
        //     cpu->cycles = 7;
        //     break;
        // }

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

        // case 0x46: { // LSR Zero Page
        //     // Set cycles
        //     cpu->cycles = 5;
        //     break;
        // }

        // case 0x56: { // LSR Zero Page, X
        //     // Set cycles
        //     cpu->cycles = 6;
        //     break;
        // }

        // case 0x4E: { // LSR Absolute
        //     // Set cycles
        //     cpu->cycles = 6;
        //     break;
        // }

        // case 0x5E: { // LSR Absolute, X
        //     // Set cycles
        //     cpu->cycles = 7;
        //     break;
        // }

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
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [ASL ZeroPage]: %02X %02X", opcode, zero_addr);

            uint16_t effective_addr = (uint16_t) zero_addr;
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
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [ASL ZeroPage, X]: %02X %02X", opcode, zero_addr);

            uint8_t wrapped_addr = (zero_addr + cpu->X) & 0xFF; // wraps around 0x00 - 0xFF
            uint16_t effective_addr = (uint16_t) wrapped_addr; 
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
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [ASL Absolute]: %02X %02X %02X", opcode, low, high);
            uint16_t effective_addr = (high << 8) | low;

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
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [ASL Absolute, X]: %02X %02X %02X", opcode, low, high);
            uint16_t effective_addr = ((high << 8) | low) + cpu->X;

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

            // Carry Flag
            uint8_t carry = cpu->A & 0x01;
            cpu->P = (carry) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Rotate Right
            cpu->A >>= 1;
            cpu->A |= (carry << 7);

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 2;
            break;
        }

        // ======== ROL ========

        case 0x2A: { // ROL Accumulator
            DEBUG_MSG_CPU("Executing instruction [ROL Accumulator]: %02X", opcode);

            // Carry Flag
            uint8_t carry = cpu->A & 0x80;
            cpu->P = (carry) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

            // Rotate Left
            cpu->A <<= 1;
            cpu->A |= (carry >> 7);

            // Set flags
            update_zero_and_negative_flags(cpu, cpu->A);

            // Set cycles
            cpu->cycles = 2;
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
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [AND ZeroPage]: %02X %02X", opcode, zero_addr);

            uint16_t effective_addr = (uint16_t) zero_addr;
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

        // case 0x35: { // AND Zero Page, X

        // }

        // case 0x2D: { // AND Absolute

        // }

        // case 0x3D: { // AND Absolute, X

        // }

        // case 0x39: { // AND Absolute, Y

        // }

        // case 0x21: { // AND (Indirect, X)

        // }

        // case 0x31: { // AND (Indirect), Y

        // }

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
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [ORA ZeroPage]: %02X %02X", opcode, zero_addr);

            uint16_t effective_addr = (uint16_t) zero_addr;
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

        // case 0x15: { // ORA Zero Page, X

        // }

        // case 0x0D: { // ORA Absolute

        // }

        // case 0x1D: { // ORA Absolute, X

        // }

        // case 0x19: { // ORA Absolute, Y

        // }

        // case 0x01: { // ORA (Indirect, X)

        // }

        // case 0x11: { // ORA (Indirect), Y

        // }

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
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [EOR ZeroPage]: %02X %02X", opcode, zero_addr);

            uint16_t effective_addr = (uint16_t) zero_addr;
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

        // case 0x55: { // EOR Zero Page, X

        // }

        // case 0x4D: { // EOR Absolute

        // }

        // case 0x5D: { // EOR Absolute, X

        // }

        // case 0x59: { // EOR Absolute, Y

        // }

        // case 0x41: { // EOR (Indirect, X)

        // }

        // case 0x51: { // EOR (Indirect), Y

        // }

        // ======== BIT ======== 

        case 0x24: { // BIT Zero Page
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [BIT ZeroPage]: %02X %02X", opcode, zero_addr);

            uint16_t effective_addr = (uint16_t) zero_addr;
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
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [BIT Absolute]: %02X %02X %02X", opcode, low, high);

            uint16_t effective_addr = (high << 8) | low;
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
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [CMP ZeroPage]: %02X %02X", opcode, zero_addr);

            uint16_t effective_addr = (uint16_t) zero_addr;
            uint8_t value = memory_read(effective_addr, memory, ppu);
            uint8_t result = cpu->A - value;

            // Set flags
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = ((result & 0xFF) == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);

            cpu->cycles = 3;
            break;
        }

        // case 0xD5: { // CMP Zero Page, X

        // }

        case 0xCD: { // CMP Absolute
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [CMP Absolute]: %02X %02X %02X", opcode, low, high);
            uint16_t effective_addr = (high << 8) | low;
        
            uint8_t value = memory_read(effective_addr, memory, ppu);
            uint8_t result = cpu->A - value;
        
            // Set flags
            cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
            cpu->P = ((result & 0xFF) == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
            cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
        
            cpu->cycles = 4;
            break;
        }

        // case 0xDD: { // CMP Absolute, X

        // }

        // case 0xD9: { // CMP Absolute, Y

        // }

        // case 0xC1: { // CMP (Indirect, X)

        // }

        // case 0xD1: { // CMP (Indirect), Y

        // }

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
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [CPX ZeroPage]: %02X %02X", opcode, zero_addr);
        
            uint16_t effective_addr = (uint16_t) zero_addr;
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
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu); 
            DEBUG_MSG_CPU("Executing instruction [CPX Absolute]: %02X %02X %02X", opcode, low, high);

            uint16_t effective_addr = (high << 8) | low;   
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
            uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [CPY ZeroPage]: %02X %02X", opcode, zero_addr);
        
            uint16_t effective_addr = (uint16_t) zero_addr;
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
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu); 
            DEBUG_MSG_CPU("Executing instruction [CPY Absolute]: %02X %02X %02X", opcode, low, high);

            uint16_t effective_addr = (high << 8) | low;   
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
            uint8_t low = memory_read(cpu->PC++, memory, ppu);
            uint8_t high = memory_read(cpu->PC++, memory, ppu);
            DEBUG_MSG_CPU("Executing instruction [JMP Absolute]: %02X %02X %02X", opcode, low, high);

            uint16_t new_addr = (high << 8) | low;
            DEBUG_MSG_CPU("Jumping to address 0x%04X", new_addr);
            cpu->PC = new_addr;

            // Set cycles
            cpu->cycles = 3;
            break;
        }

        // case 0x6C: { // JMP (Indirect)
        //     // Set cycles
        //     cpu->cycles = 5;
        //     break;
        // }
        
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
            cpu->P = stack_pop(cpu, memory, ppu);

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

        // ==================== Other ====================

        case 0xEA: { // NOP 
            DEBUG_MSG_CPU("Executing instruction [NOP]: %02X", opcode);
            cpu->cycles = 2;
            break;
        }

        case 0x00: { // BRK
            DEBUG_MSG_CPU("Executing instruction [BRK]: %02X", opcode);

            // The return address is PC + 1 (skipping padding)
            uint16_t return_addr = cpu->PC + 1;

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
            ERROR_MSG("CPU", "Unhandled opcode: %02X", opcode);
            exit(1); 
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

    // Push status register with Break flag set
    uint8_t status_with_B = cpu->P | FLAG_BREAK | FLAG_UNUSED; 
    stack_push(status_with_B, cpu, memory, ppu);

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
    DEBUG_MSG_CPU("Executing instruction [%s ZeroPage]: %02X", instruction, opcode);

    uint16_t effective_addr = (uint16_t) zero_addr;

    return effective_addr;
}

uint16_t cpu_zero_page_x(uint8_t opcode, const char *instruction, CPU *cpu, uint8_t *memory, PPU *ppu) {
    uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [%s ZeroPage, X]: %02X", instruction, opcode);

    uint8_t wrapped_addr = (zero_addr + cpu->X) & 0xFF; // wraps around 0x00 - 0xFF
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