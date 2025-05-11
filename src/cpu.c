#include <stdio.h>
#include <stdlib.h>
#include "../include/cpu.h"
#include "../include/log.h"
#include "../include/memory.h"
#include "../include/ppu.h"
#include "../include/opcode_handlers.h"

// Addressing Modes
uint16_t cpu_immediate(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu);
uint16_t cpu_zero_page(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu);
uint16_t cpu_zero_page_x(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu);
uint16_t cpu_zero_page_y(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu);
uint16_t cpu_absolute(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu);
uint16_t cpu_absolute_x(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu);
uint16_t cpu_absolute_y(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu);
uint16_t cpu_indirect_x(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu);
uint16_t cpu_indirect_y(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu);

// Access
void lda(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void sta(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void ldx(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void stx(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void ldy(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void sty(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
// Arithmetic
void adc(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void sbc(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void inc(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void dec(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
// Shift
void lsr(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void asl(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void rol(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void ror(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
// Bitwise
void and(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void ora(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void eor(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void bit(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
// Compare
void cmp(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void cpx(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void cpy(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
// Branch
void branch(int condition, int8_t offset, CPU *cpu);
// Unofficial
void slo(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void lax(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void sax(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void dcp(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void isc(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void rla(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void sre(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
void rra(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu);
// Other
void nop_implied(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu);
void nop_immediate(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu);
void nop_zero_page(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu);
void nop_zero_page_x(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu);
void nop_absolute(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu);
void nop_absolute_x(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu);
void invalid(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu);

// Helper Functions
void init_opcode_table();
void update_zero_and_negative_flags(CPU* cpu, uint8_t value); 

InstructionHandler opcode_table[256];

CPU *cpu_init(MEM *memory) {
    printf("Initializing CPU...");

    CPU *cpu = (CPU *)malloc(sizeof(CPU));
    if (cpu == NULL) {
        printf("\tFAILED\n");
        FATAL_ERROR("CPU", "CPU memory allocation failed");
    }

    // Make sure that reset vector is set 0xFFFC & 0xFFFD
    if (memory->prg_rom[0x7FFC] == 0 && memory->prg_rom[0x7FFD] == 0) { 
        printf("\tFAILED\n");
        FATAL_ERROR("CPU", "Reset vector at 0xFFFC and 0xFFFD not set");
    }

    // Set initial register values
    cpu->A = 0;
    cpu->X = 0;
    cpu->Y = 0;
    cpu->PC = memory->prg_rom[0x7FFC] | (memory->prg_rom[0x7FFD] << 8); // reset vector at 0xFFFC and 0xFFFD (little endian)
    cpu->S = 0xFD; 
    cpu->P = 0x24;

    cpu->cycles = 0;
    cpu->page_crossed = 0;
    cpu->service_int = 0;

    init_opcode_table();

    printf("\tDONE\n");
    return cpu;
}

void cpu_free(CPU *cpu) {
    free(cpu);
}

void cpu_execute_opcode(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    cpu->cycles = 0;
    cpu->page_crossed = 0;

    if (ppu->nmi == 1 && cpu->service_int == 0) { // Handle NMI interrupt
        cpu->PC--; // decrement because we incremented when calling the function
        cpu_nmi(cpu, memory, ppu);
        return;
    }

    opcode_table[opcode](cpu, opcode, memory, ppu);
}

// ==================== Access ====================

// ======== LDA ========

// LDA #Immediate
void handle_0xA9(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_immediate(opcode, "LDA", cpu, memory, ppu);
    lda(effective_addr, cpu, memory, ppu);
    cpu->cycles = 2;
    return;
}

// LDA Zero Page
void handle_0xA5(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "LDA", cpu, memory, ppu);
    lda(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

// LDA Zero Page, X
void handle_0xB5(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "LDA", cpu, memory, ppu);
    lda(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// LDA Absolute
void handle_0xAD(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "LDA", cpu, memory, ppu);
    lda(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// LDA Absolute, X
void handle_0xBD(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "LDA", cpu, memory, ppu);
    lda(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1; 
    }
    return;
}
 
// LDA Absolute, Y
void handle_0xB9(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "LDA", cpu, memory, ppu);
    lda(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1; 
    }
    return;
}
       
// LDA (Indirect, X)
void handle_0xA1(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "LDA", cpu, memory, ppu);
    lda(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}
       
// LDA (Indirect), Y
void handle_0xB1(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "LDA", cpu, memory, ppu);
    lda(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    if (cpu->page_crossed) {
        cpu->cycles += 1; 
    }
    return;
}
        
// ======== STA ======== 

// STA Zero Page
void handle_0x85(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "STA", cpu, memory, ppu);
    sta(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

// STA Zero Page, X
void handle_0x95(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "STA", cpu, memory, ppu);
    sta(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// STA Absolute
void handle_0x8D(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "STA", cpu, memory, ppu);
    sta(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}
        
// STA Absolute, X
void handle_0x9D(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "STA", cpu, memory, ppu);
    sta(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    return;
}

// STA Absolute, Y
void handle_0x99(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "STA", cpu, memory, ppu);
    sta(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    return;
}
       
// STA (Indirect, X)
void handle_0x81(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "STA", cpu, memory, ppu);
    sta(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}
     
// STA (Indirect), Y
void handle_0x91(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "STA", cpu, memory, ppu);
    sta(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// ======== LDX ======== 

// LDX #Immediate
void handle_0xA2(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_immediate(opcode, "LDX", cpu, memory, ppu);
    ldx(effective_addr, cpu, memory, ppu);
    cpu->cycles = 2;
    return;
}

// LDX Zero Page
void handle_0xA6(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "LDX", cpu, memory, ppu);
    ldx(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

// LDX Zero Page, Y
void handle_0xB6(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_y(opcode, "LDX", cpu, memory, ppu);
    ldx(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// LDX Absolute
void handle_0xAE(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "LDX", cpu, memory, ppu);
    ldx(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// LDX Absolute, Y
void handle_0xBE(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "LDX", cpu, memory, ppu);
    ldx(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// ======== STX ======== 

// STX Zero Page
void handle_0x86(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "STX", cpu, memory, ppu);
    stx(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

// STX Zero Page, Y
void handle_0x96(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_y(opcode, "STX", cpu, memory, ppu);
    stx(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// STX Absolute
void handle_0x8E(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "STX", cpu, memory, ppu);
    stx(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// ======== LDY ======== 

// LDY #Immediate
void handle_0xA0(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_immediate(opcode, "LDY", cpu, memory, ppu);
    ldy(effective_addr, cpu, memory, ppu);
    cpu->cycles = 2;
    return;
}

// LDY Zero Page
void handle_0xA4(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "LDY", cpu, memory, ppu);
    ldy(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

// LDY Zero Page, X
void handle_0xB4(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "LDY", cpu, memory, ppu);
    ldy(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}
        
// LDY Absolute
void handle_0xAC(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "LDY", cpu, memory, ppu);
    ldy(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}
       
// LDY Absolute, X
void handle_0xBC(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "LDY", cpu, memory, ppu);
    ldy(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// ======== STY ======== 
 
// STY Zero Page
void handle_0x84(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "STY", cpu, memory, ppu);
    sty(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

// STY Zero Page, X
void handle_0x94(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "STY", cpu, memory, ppu);
    sty(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// STY Absolute
void handle_0x8C(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "STY", cpu, memory, ppu);
    sty(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// ==================== Transfer ==================== 

// TAX Implied
void handle_0xAA(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [TAX]: %02X", opcode);
    DEBUG_MSG_CPU("Writing 0x%02X to register X", cpu->A);
    cpu->X = cpu->A;
    update_zero_and_negative_flags(cpu, cpu->X);
    cpu->cycles = 2;
    return;
}

// TXA Implied
void handle_0x8A(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [TXA]: %02X", opcode);
    DEBUG_MSG_CPU("Writing 0x%02X to register A", cpu->X);
    cpu->A = cpu->X;
    update_zero_and_negative_flags(cpu, cpu->A);
    cpu->cycles = 2;
    return;
}

// TAY Implied
void handle_0xA8(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [TAY]: %02X", opcode);
    DEBUG_MSG_CPU("Writing 0x%02X to register Y", cpu->A);
    cpu->Y = cpu->A;
    update_zero_and_negative_flags(cpu, cpu->Y);
    cpu->cycles = 2;
    return;
}

// TYA Implied
void handle_0x98(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [TYA]: %02X", opcode);
    DEBUG_MSG_CPU("Writing 0x%02X to register A", cpu->Y);
    cpu->A = cpu->Y;
    update_zero_and_negative_flags(cpu, cpu->A);
    cpu->cycles = 2;
    return;
}
    
// ==================== Arithmetic ====================

// ======== ADC ======== 

// ADC #Immediate
void handle_0x69(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_immediate(opcode, "ADC", cpu, memory, ppu);
    adc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 2;
    return;
}

// ADC Zero Page
void handle_0x65(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "ADC", cpu, memory, ppu);
    adc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

// ADC Zero Page, X
void handle_0x75(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "ADC", cpu, memory, ppu);
    adc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// ADC Absolute
void handle_0x6D(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "ADC", cpu, memory, ppu);
    adc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// ADC Absolute, X
void handle_0x7D(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "ADC", cpu, memory, ppu);
    adc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// ADC Absolute, Y
void handle_0x79(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "ADC", cpu, memory, ppu);
    adc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// ADC (Indirect, X)
void handle_0x61(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "ADC", cpu, memory, ppu);
    adc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// ADC (Indirect), Y
void handle_0x71(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "ADC", cpu, memory, ppu);
    adc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// ======== SBC ======== 

// SBC #Immediate (Unofficial Opcode)
void handle_0xEB(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_immediate(opcode, "SBC", cpu, memory, ppu);
    sbc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 2;
    return;
}

// SBC #Immediate
void handle_0xE9(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_immediate(opcode, "SBC", cpu, memory, ppu);
    sbc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 2;
    return;
}

// SBC Zero Page
void handle_0xE5(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "SBC", cpu, memory, ppu);
    sbc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

// SBC Zero Page, X
void handle_0xF5(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "SBC", cpu, memory, ppu);
    sbc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// SBC Absolute
void handle_0xED(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "SBC", cpu, memory, ppu);
    sbc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// SBC Absolute, X
void handle_0xFD(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "SBC", cpu, memory, ppu);
    sbc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// SBC Absolute, Y
void handle_0xF9(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "SBC", cpu, memory, ppu);
    sbc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// SBC (Indirect, X)
void handle_0xE1(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "SBC", cpu, memory, ppu);
    sbc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// SBC (Indirect), Y
void handle_0xF1(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "SBC", cpu, memory, ppu);
    sbc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// ======== INC ======== 

// INC Zero Page
void handle_0xE6(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "INC", cpu, memory, ppu);
    inc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    return;
}

// INC Zero Page, X
void handle_0xF6(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "INC", cpu, memory, ppu);
    inc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// INC Absolute
void handle_0xEE(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "INC", cpu, memory, ppu);
    inc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// INC Absolute, X
void handle_0xFE(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "INC", cpu, memory, ppu);
    inc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// ======== DEC ======== 

// DEC Zero Page
void handle_0xC6(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "DEC", cpu, memory, ppu);
    dec(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    return;
}

// DEC Zero Page, X
void handle_0xD6(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "DEC", cpu, memory, ppu);
    dec(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// DEC Absolute
void handle_0xCE(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "DEC", cpu, memory, ppu);
    dec(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// DEC Absolute, X
void handle_0xDE(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "DEC", cpu, memory, ppu);
    dec(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// ======== INX ======== 

// INX (Increment X)
void handle_0xE8(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [INX]: %02X", opcode);
    DEBUG_MSG_CPU("Incrementing register X by 0x01");
    cpu->X += 0x01; // increment X by 1, does not modify carry or overflow
    update_zero_and_negative_flags(cpu, cpu->X);
    cpu->cycles = 2;
    return;
}

// ======== DEX ======== 

// DEX (Decrement X)
void handle_0xCA(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [DEX]: %02X", opcode);
    DEBUG_MSG_CPU("Decrementing register X by 0x01");
    cpu->X -= 0x01; // decrement X by 1, does not modify carry or overflow
    update_zero_and_negative_flags(cpu, cpu->X);
    cpu->cycles = 2;
    return;
}

// ======== INY ======== 

// INY (Increment Y)
void handle_0xC8(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [INY]: %02X", opcode);
    DEBUG_MSG_CPU("Incrementing register Y by 0x01");
    cpu->Y += 0x01; // increment Y by 1, does not modify carry or overflow
    update_zero_and_negative_flags(cpu, cpu->Y);
    cpu->cycles = 2;
    return;
}

// ======== DEY ======== 

// DEY (Decrement Y)
void handle_0x88(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [DEY]: %02X", opcode);
    DEBUG_MSG_CPU("Decrementing register Y by 0x01");
    cpu->Y -= 0x01; // decrement Y by 1, does not modify carry or overflow
    update_zero_and_negative_flags(cpu, cpu->Y);
    cpu->cycles = 2;
    return;
}

// ==================== Shift ====================

// ======== LSR ======== 

// LSR Accumulator
void handle_0x4A(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
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
    return;
}

// LSR Zero Page
void handle_0x46(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "LSR", cpu, memory, ppu);
    lsr(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    return;
}

// LSR Zero Page, X
void handle_0x56(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "LSR", cpu, memory, ppu);
    lsr(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// LSR Absolute
void handle_0x4E(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "LSR", cpu, memory, ppu);
    lsr(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// LSR Absolute, X
void handle_0x5E(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "LSR", cpu, memory, ppu);
    lsr(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// ======== ASL ======== 

// ASL Accumulator
void handle_0x0A(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [ASL Accumulator]: %02X", opcode);

    // Carry Flag
    cpu->P = (cpu->A & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

    // Arithmetic Shift Left
    cpu->A <<= 1;

    // Set flags
    update_zero_and_negative_flags(cpu, cpu->A);

    // Set cycles
    cpu->cycles = 2;
    return;
}

// ASL Zero Page
void handle_0x06(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "ASL", cpu, memory, ppu);
    asl(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    return;
}

// ASL Zero Page, X
void handle_0x16(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "ASL", cpu, memory, ppu);
    asl(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// ASL Absolute
void handle_0x0E(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "ASL", cpu, memory, ppu);
    asl(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// ASL Absolute, X
void handle_0x1E(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "ASL", cpu, memory, ppu);
    asl(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// ======== ROR ========

// ROR Accumulator
void handle_0x6A(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [ROR Accumulator]: %02X", opcode);
        
    uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 1 : 0;
    uint8_t new_carry = cpu->A & 0x01;

    cpu->A >>= 1;
    cpu->A |= (old_carry << 7);

    cpu->P = (new_carry) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

    update_zero_and_negative_flags(cpu, cpu->A);

    cpu->cycles = 2;
    return;
}

// ROR Zero Page
void handle_0x66(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "ROR", cpu, memory, ppu);
    ror(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    return;
}

// ROR Zero Page, X    
void handle_0x76(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "ROR", cpu, memory, ppu);
    ror(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// ROR Absolute
void handle_0x6E(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "ROR", cpu, memory, ppu);
    ror(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// ROR Absolute, X
void handle_0x7E(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "ROR", cpu, memory, ppu);
    ror(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// ======== ROL ========

// ROL Accumulator
void handle_0x2A(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
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
    return;
}

// ROL Zero Page
void handle_0x26(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "ROL", cpu, memory, ppu);
    rol(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    return;
}

// ROL Zero Page, X
void handle_0x36(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "ROL", cpu, memory, ppu);
    rol(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// ROL Absolute
void handle_0x2E(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "ROL", cpu, memory, ppu);
    rol(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// ROL Absolute, X
void handle_0x3E(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "ROL", cpu, memory, ppu);
    rol(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// ==================== Bitwise ====================

// ======== AND ======== 

// AND #Immediate
void handle_0x29(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_immediate(opcode, "AND", cpu, memory, ppu);
    and(effective_addr, cpu, memory, ppu);
    cpu->cycles = 2;
    return;
}

// AND Zero Page
void handle_0x25(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "AND", cpu, memory, ppu);
    and(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

// AND Zero Page, X
void handle_0x35(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "AND", cpu, memory, ppu);
    and(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// AND Absolute
void handle_0x2D(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "AND", cpu, memory, ppu);
    and(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// AND Absolute, X
void handle_0x3D(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "AND", cpu, memory, ppu);
    and(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// AND Absolute, Y
void handle_0x39(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "AND", cpu, memory, ppu);
    and(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// AND (Indirect, X)
void handle_0x21(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "AND", cpu, memory, ppu);
    and(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// AND (Indirect), Y
void handle_0x31(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "AND", cpu, memory, ppu);
    and(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// ======== ORA ======== 

// ORA #Immedaite
void handle_0x09(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_immediate(opcode, "ORA", cpu, memory, ppu);
    ora(effective_addr, cpu, memory, ppu);
    cpu->cycles = 2;
    return;
}

// ORA Zero Page
void handle_0x05(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "ORA", cpu, memory, ppu);
    ora(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

// ORA Zero Page, X
void handle_0x15(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "ORA", cpu, memory, ppu);
    ora(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// ORA Absolute
void handle_0x0D(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "ORA", cpu, memory, ppu);
    ora(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// ORA Absolute, X
void handle_0x1D(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "ORA", cpu, memory, ppu);
    ora(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// ORA Absolute, Y
void handle_0x19(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "ORA", cpu, memory, ppu);
    ora(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// ORA (Indirect, X)
void handle_0x01(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "ORA", cpu, memory, ppu);
    ora(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// ORA (Indirect), Y
void handle_0x11(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "ORA", cpu, memory, ppu);
    ora(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// ======== EOR ======== 

// EOR #Immedaite
void handle_0x49(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_immediate(opcode, "EOR", cpu, memory, ppu);
    eor(effective_addr, cpu, memory, ppu);
    cpu->cycles = 2;
    return;
}

// EOR Zero Page
void handle_0x45(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "EOR", cpu, memory, ppu);
    eor(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

// EOR Zero Page, X
void handle_0x55(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "EOR", cpu, memory, ppu);
    eor(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// EOR Absolute
void handle_0x4D(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "EOR", cpu, memory, ppu);
    eor(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// EOR Absolute, X
void handle_0x5D(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "EOR", cpu, memory, ppu);
    eor(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// EOR Absolute, Y
void handle_0x59(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "EOR", cpu, memory, ppu);
    eor(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// EOR (Indirect, X)
void handle_0x41(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "EOR", cpu, memory, ppu);
    eor(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// EOR (Indirect), Y
void handle_0x51(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "EOR", cpu, memory, ppu);
    eor(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// ======== BIT ======== 

// BIT Zero Page
void handle_0x24(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "BIT", cpu, memory, ppu);
    bit(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

// BIT Absolute
void handle_0x2C(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "BIT", cpu, memory, ppu);
    bit(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}      

// ==================== Compare ====================

// ======== CMP ========

// CMP #Immediate
void handle_0xC9(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_immediate(opcode, "CMP", cpu, memory, ppu);
    cmp(effective_addr, cpu, memory, ppu);
    cpu->cycles = 2;
    return;
}  

// CMP Zero Page
void handle_0xC5(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "CMP", cpu, memory, ppu);
    cmp(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}  

// CMP Zero Page, X
void handle_0xD5(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "CMP", cpu, memory, ppu);
    cmp(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}  

// CMP Absolute
void handle_0xCD(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "CMP", cpu, memory, ppu);
    cmp(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}  

// CMP Absolute, X
void handle_0xDD(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "CMP", cpu, memory, ppu);
    cmp(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
} 

// CMP Absolute, Y
void handle_0xD9(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "CMP", cpu, memory, ppu);
    cmp(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
} 


// CMP (Indirect, X)
void handle_0xC1(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "CMP", cpu, memory, ppu);
    cmp(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}  

// CMP (Indirect), Y
void handle_0xD1(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "CMP", cpu, memory, ppu);
    cmp(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
} 

// ======== CPX ======== 

// CPX #Immediate
void handle_0xE0(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_immediate(opcode, "CPX", cpu, memory, ppu);
    cpx(effective_addr, cpu, memory, ppu);
    cpu->cycles = 2;
    return;
}  

// CPX Zero Page
void handle_0xE4(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "CPX", cpu, memory, ppu);
    cpx(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
} 

// CPX Absolute
void handle_0xEC(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "CPX", cpu, memory, ppu);
    cpx(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
} 

// ======== CPY ======== 

// CPY #Immediate
void handle_0xC0(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_immediate(opcode, "CPY", cpu, memory, ppu);
    cpy(effective_addr, cpu, memory, ppu);
    cpu->cycles = 2;
    return;
} 

// CPY Zero Page
void handle_0xC4(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "CPY", cpu, memory, ppu);
    cpy(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
} 
        
// CPY Absolute
void handle_0xCC(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "CPY", cpu, memory, ppu);
    cpy(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
} 
       
// ==================== Branch ====================

// BCC (Branch if Carry Clear)
void handle_0x90(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [BCC Relative]: %02X %02X", opcode, (uint8_t) offset);
    cpu->cycles = 2;
    int contiditon = !(cpu->P & FLAG_CARRY);
    branch(contiditon, offset, cpu);
    return;
}

// BCS (Branch if Carry Set)
void handle_0xB0(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [BCS Relative]: %02X %02X", opcode, (uint8_t) offset);
    cpu->cycles = 2;
    int contiditon = cpu->P & FLAG_CARRY;
    branch(contiditon, offset, cpu);
    return;
}

// BEQ (Branch if Equal)
void handle_0xF0(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [BEQ Relative]: %02X %02X", opcode, (uint8_t) offset);
    cpu->cycles = 2;
    int contiditon = cpu->P & FLAG_ZERO;
    branch(contiditon, offset, cpu);
    return;
}

// BMI (Branch if Minus)
void handle_0x30(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [BMI Relative]: %02X %02X", opcode, (uint8_t) offset);
    cpu->cycles = 2;
    int contiditon = cpu->P & FLAG_NEGATIVE;
    branch(contiditon, offset, cpu);
    return;
}

// BNE (Branch if Not Equal)
void handle_0xD0(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [BNE Relative]: %02X %02X", opcode, (uint8_t) offset);
    cpu->cycles = 2;
    int contiditon = !(cpu->P & FLAG_ZERO);
    branch(contiditon, offset, cpu);
    return;
}

// BPL (Branch if Plus)
void handle_0x10(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [BPL Relative]: %02X %02X", opcode, (uint8_t) offset);
    cpu->cycles = 2;
    int contiditon = !(cpu->P & FLAG_NEGATIVE);
    branch(contiditon, offset, cpu);
    return;
}

// BVC (Branch if Overflow Clear)
void handle_0x50(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [BVC Relative]: %02X %02X", opcode, (uint8_t) offset);
    cpu->cycles = 2;
    int contiditon = !(cpu->P & FLAG_OVERFLOW);
    branch(contiditon, offset, cpu);
    return;
}

// BVS (Branch if Overflow Set)
void handle_0x70(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    int8_t offset = (int8_t) memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [BVS Relative]: %02X %02X", opcode, (uint8_t) offset);
    cpu->cycles = 2;
    int contiditon = cpu->P & FLAG_OVERFLOW;
    branch(contiditon, offset, cpu);
    return;
}

// ==================== Jump ====================

// ======== JMP ======== 

// JMP Absolute
void handle_0x4C(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "JMP", cpu, memory, ppu);
    DEBUG_MSG_CPU("Jumping to address 0x%04X", effective_addr);
    cpu->PC = effective_addr;
    cpu->cycles = 3;
    return;
}

// JMP (Indirect)
void handle_0x6C(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
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
    return;
}
        
// ======== JSR ======== 

// JSR (Jump to Subroutine)
void handle_0x20(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
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
    return;
}

// ======== RTS ======== 

// RTS (Return from Subroutine)
void handle_0x60(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
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
    return;
}

// ======== BRK ======== 

// BRK (Break)
void handle_0x00(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
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
    return;
}

// ======== RTI ======== 

// RTI (Return from Interrupt)
void handle_0x40(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
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
    return;
}
        
// ==================== Stack ====================

// ======== PHP ======== 

// PHP (Push Processor Status)
void handle_0x08(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    DEBUG_MSG_CPU("Executing instruction [PHP]: %02X", opcode);
    uint8_t status = cpu->P | FLAG_BREAK;
    stack_push(status, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

// ======== PLP ======== 

// PLP (Pull Processor Status)
void handle_0x28(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    DEBUG_MSG_CPU("Executing instruction [PLP]: %02X", opcode);
    uint8_t value = stack_pop(cpu, memory, ppu);

    value &= ~(1 << 4);     // Clear B flag
    value |= (1 << 5);      // Set unused bit to 1
    cpu->P = value;

    cpu->cycles = 4;
    return;
}

// ======== PLA ======== 

// Pull A
void handle_0x68(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    DEBUG_MSG_CPU("Executing instruction [PLA]: %02X", opcode);
    cpu->A = stack_pop(cpu, memory, ppu);
    update_zero_and_negative_flags(cpu, cpu->A);
    cpu->cycles = 4;
    return;
}

// ======== PHA ======== 

// Push A
void handle_0x48(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    DEBUG_MSG_CPU("Executing instruction [PHA]: %02X", opcode);
    stack_push(cpu->A, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

// TSX Implied
void handle_0xBA(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [TSX]: %02X", opcode);
    DEBUG_MSG_CPU("Writing 0x%02X to register X", cpu->S);
    cpu->X = cpu->S;
    update_zero_and_negative_flags(cpu, cpu->X);
    cpu->cycles = 2;
    return;
}

// TXS Implied
void handle_0x9A(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [TXS]: %02X", opcode);
    DEBUG_MSG_CPU("Writing 0x%02X to register S", cpu->X);
    cpu->S = cpu->X;
    cpu->cycles = 2;
    return;
}

// ==================== Flags ====================

// CLC (Clear Carry)
void handle_0x18(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [CLC]: %02X", opcode);
    cpu->P &= ~FLAG_CARRY;
    cpu->cycles = 2;
    return;
}

// SEC (Set Carry)
void handle_0x38(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [SEC]: %02X", opcode);
    cpu->P |= FLAG_CARRY;
    cpu->cycles = 2;
    return;
}

// CLD (Clear Decimal)
void handle_0xD8(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [CLD]: %02X", opcode);
    cpu->P &= ~FLAG_DECIMAL;
    cpu->cycles = 2;
    return;
}
        
// SED (Set Decimal)
void handle_0xF8(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [SED]: %02X", opcode);
    cpu->P |= FLAG_DECIMAL;
    cpu->cycles = 2;
    return;
}

// CLI (Clear Interrupt Disable)
void handle_0x58(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [CLI]: %02X", opcode);
    cpu->P &= ~FLAG_INT;
    cpu->cycles = 2;
    return;
}

// SEI (Set Interrupt Disable)
void handle_0x78(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [SEI]: %02X", opcode);
    cpu->P |= FLAG_INT;
    cpu->cycles = 2;
    return;
}

// CLV (Clear Overflow)
void handle_0xB8(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [CLV]: %02X", opcode);
    cpu->P &= ~FLAG_OVERFLOW;
    cpu->cycles = 2;
    return;
}

// ==================== Unofficial Opcodes ==================== 

// ======== SLO ======== 

// SLO Zero Page
void handle_0x07(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "SLO", cpu, memory, ppu);
    slo(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    return;
}

// SLO Zero Page, X
void handle_0x17(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "SLO", cpu, memory, ppu);
    slo(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// SLO Absolute
void handle_0x0F(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "SLO", cpu, memory, ppu);
    slo(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// SLO Absolute, X
void handle_0x1F(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "SLO", cpu, memory, ppu);
    slo(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// SLO Absolute, Y
void handle_0x1B(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "SLO", cpu, memory, ppu);
    slo(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// SLO (Indirect, X)
void handle_0x03(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "SLO", cpu, memory, ppu);
    slo(effective_addr, cpu, memory, ppu);
    cpu->cycles = 8;
    return;
}

// SLO (Indirect), Y
void handle_0x13(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "SLO", cpu, memory, ppu);
    slo(effective_addr, cpu, memory, ppu);
    cpu->cycles = 8;
    return;
}

// ======== LAX ======== 

// LAX Immediate (Highly Unstable)
void handle_0xAB(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_immediate(opcode, "LAX", cpu, memory, ppu);
    lax(effective_addr, cpu, memory, ppu);
    cpu->cycles = 2;
    return;
}

// LAX Zero Page
void handle_0xA7(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "LAX", cpu, memory, ppu);
    lax(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}
       
// LAX Zero Page, Y
void handle_0xB7(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_y(opcode, "LAX", cpu, memory, ppu);
    lax(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// LAX Absolute
void handle_0xAF(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "LAX", cpu, memory, ppu);
    lax(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// LAX Absolute, Y
void handle_0xBF(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "LAX", cpu, memory, ppu);
    lax(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// LAX (Indirect, X)
void handle_0xA3(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "LAX", cpu, memory, ppu);
    lax(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// LAX (Indirect), Y
void handle_0xB3(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "LAX", cpu, memory, ppu);
    lax(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

// ======== SAX ======== 

// SAX Zero Page
void handle_0x87(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "SAX", cpu, memory, ppu);
    sax(effective_addr, cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

// SAX Zero Page, Y
void handle_0x97(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_y(opcode, "SAX", cpu, memory, ppu);
    sax(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// SAX Absolute
void handle_0x8F(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "SAX", cpu, memory, ppu);
    sax(effective_addr, cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

// SAX (Indirect, X)
void handle_0x83(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "SAX", cpu, memory, ppu);
    sax(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// ======== DCP ======== 

// DCP Zero Page
void handle_0xC7(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "DCP", cpu, memory, ppu);
    dcp(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    return;
}

// DCP Zero Page, X
void handle_0xD7(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "DCP", cpu, memory, ppu);
    dcp(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// DCP Absolute
void handle_0xCF(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "DCP", cpu, memory, ppu);
    dcp(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// DCP Absolute, X
void handle_0xDF(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "DCP", cpu, memory, ppu);
    dcp(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// DCP Absolute, Y
void handle_0xDB(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "DCP", cpu, memory, ppu);
    dcp(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// DCP (Indirect, X)
void handle_0xC3(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "DCP", cpu, memory, ppu);
    dcp(effective_addr, cpu, memory, ppu);
    cpu->cycles = 8;
    return;
}

// DCP (Indirect), Y
void handle_0xD3(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "DCP", cpu, memory, ppu);
    dcp(effective_addr, cpu, memory, ppu);
    cpu->cycles = 8;
    return;
}

// ======== ISC ======== 

// ISC Zero Page
void handle_0xE7(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "ISC", cpu, memory, ppu);
    isc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    return;
}

// ISC Zero Page, X
void handle_0xF7(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "ISC", cpu, memory, ppu);
    isc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// ISC Absolute
void handle_0xEF(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "ISC", cpu, memory, ppu);
    isc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// ISC Absolute, X
void handle_0xFF(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "ISC", cpu, memory, ppu);
    isc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// ISC Absolute, Y
void handle_0xFB(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "ISC", cpu, memory, ppu);
    isc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// ISC (Indirect, X)
void handle_0xE3(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "ISC", cpu, memory, ppu);
    isc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 8;
    return;
}

// ISC (Indirect), Y
void handle_0xF3(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "ISC", cpu, memory, ppu);
    isc(effective_addr, cpu, memory, ppu);
    cpu->cycles = 8;
    return;
}

// ======== RLA ======== 

// RLA Zero Page
void handle_0x27(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "RLA", cpu, memory, ppu);
    rla(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    return;
}

// RLA Zero Page, X
void handle_0x37(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "RLA", cpu, memory, ppu);
    rla(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// RLA Absolute
void handle_0x2F(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "RLA", cpu, memory, ppu);
    rla(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// RLA Absolute, X
void handle_0x3F(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "RLA", cpu, memory, ppu);
    rla(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// RLA Absolute, Y
void handle_0x3B(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "RLA", cpu, memory, ppu);
    rla(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// RLA (Indirect, X)
void handle_0x23(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "RLA", cpu, memory, ppu);
    rla(effective_addr, cpu, memory, ppu);
    cpu->cycles = 8;
    return;
}

// RLA (Indirect), Y
void handle_0x33(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "RLA", cpu, memory, ppu);
    rla(effective_addr, cpu, memory, ppu);
    cpu->cycles = 8;
    return;
}

// ======== SRE ======== 

// SRE Zero Page
void handle_0x47(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "SRE", cpu, memory, ppu);
    sre(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    return;
}

// SRE Zero Page, X
void handle_0x57(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "SRE", cpu, memory, ppu);
    sre(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// SRE Absolute
void handle_0x4F(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "SRE", cpu, memory, ppu);
    sre(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// SRE Absolute, X
void handle_0x5F(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "SRE", cpu, memory, ppu);
    sre(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// SRE Absolute, Y
void handle_0x5B(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "SRE", cpu, memory, ppu);
    sre(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// SRE (Indirect, X)
void handle_0x43(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "SRE", cpu, memory, ppu);
    sre(effective_addr, cpu, memory, ppu);
    cpu->cycles = 8;
    return;
}

// SRE (Indirect), Y
void handle_0x53(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "SRE", cpu, memory, ppu);
    sre(effective_addr, cpu, memory, ppu);
    cpu->cycles = 8;
    return;
}

// ======== RRA ======== 

// RRA Zero Page
void handle_0x67(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page(opcode, "RRA", cpu, memory, ppu);
    rra(effective_addr, cpu, memory, ppu);
    cpu->cycles = 5;
    return;
}

// RRA Zero Page, X
void handle_0x77(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_zero_page_x(opcode, "RRA", cpu, memory, ppu);
    rra(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}
        
// RRA Absolute
void handle_0x6F(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute(opcode, "RRA", cpu, memory, ppu);
    rra(effective_addr, cpu, memory, ppu);
    cpu->cycles = 6;
    return;
}

// RRA Absolute, X
void handle_0x7F(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "RRA", cpu, memory, ppu);
    rra(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}

// RRA Absolute, Y
void handle_0x7B(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "RRA", cpu, memory, ppu);
    rra(effective_addr, cpu, memory, ppu);
    cpu->cycles = 7;
    return;
}
    
// RRA (Indirect, X)
void handle_0x63(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_x(opcode, "RRA", cpu, memory, ppu);
    rra(effective_addr, cpu, memory, ppu);
    cpu->cycles = 8;
    return;
}

// RRA (Indirect), Y
void handle_0x73(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "RRA", cpu, memory, ppu);
    rra(effective_addr, cpu, memory, ppu);
    cpu->cycles = 8;
    return;
}
        
// ======== ANC ======== 

void handle_0x0B(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [ANC]: %02X %02X", opcode, value);
    cpu->A &= value;

    // Set Carry to bit 7 of result
    cpu->P = (cpu->A & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
    update_zero_and_negative_flags(cpu, cpu->A);

    cpu->cycles = 2;
    return;
}

void handle_0x2B(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [ANC]: %02X %02X", opcode, value);
    cpu->A &= value;

    // Set Carry to bit 7 of result
    cpu->P = (cpu->A & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
    update_zero_and_negative_flags(cpu, cpu->A);

    cpu->cycles = 2;
    return;
}
        

// ======== ALR ======== 

void handle_0x4B(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [ALR]: %02X %02X", opcode, value);
    cpu->A &= value;

    // Set Carry to bit 0 before shift
    cpu->P = (cpu->A & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

    // Logical Shift Right
    cpu->A >>= 1;

    update_zero_and_negative_flags(cpu, cpu->A);
    cpu->cycles = 2;
    return;
}

// ======== ARR ======== 

void handle_0x6B(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
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
    return;
}
        
// ======== XAA ======== 

void handle_0x8B(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [XXA]: %02X %02X", opcode, value);

    // Output is AND of A, X, and immediate
    cpu->A = (cpu->A & cpu->X) & value;

    update_zero_and_negative_flags(cpu, cpu->A);
    cpu->cycles = 2;
    return;
}

// ======== AXS ======== 

void handle_0xCB(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [AXS]: %02X %02X", opcode, value);
    uint8_t result = cpu->X & cpu->A;
    result -= value;

    // Set Carry if result did not borrow
    cpu->P = (cpu->X & cpu->A) >= value ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
    cpu->X = result;

    update_zero_and_negative_flags(cpu, result);
    cpu->cycles = 2;
    return;
}

// ======== AHX ======== 

void handle_0x93(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_indirect_y(opcode, "AHX", cpu, memory, ppu);
    uint16_t temp = ((effective_addr >> 8) + 1) & 0xFF;
    uint8_t value = cpu->A & cpu->X & temp;
    memory_write(effective_addr, value, memory, ppu);
    cpu->cycles = 8;
    return;
}
          
// ======== SHY ======== 

void handle_0x9C(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_x(opcode, "SHY", cpu, memory, ppu);
    uint8_t value = cpu->Y & ((effective_addr >> 8) + 1);
    memory_write(effective_addr, value, memory, ppu);
    cpu->cycles = 5;
    return;
}
 
// ======== AHX ======== 

void handle_0x9F(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "AHX", cpu, memory, ppu);
    uint8_t value = cpu->A & cpu->X & ((effective_addr >> 8) + 1);
    memory_write(effective_addr, value, memory, ppu);
    cpu->cycles = 5;
    return;
}

// ======== SHX ======== 

void handle_0x9E(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "SHX", cpu, memory, ppu);
    uint8_t value = cpu->X & ((effective_addr >> 8) + 1);
    memory_write(effective_addr, value, memory, ppu);
    cpu->cycles = 5;
    return;
}

// ======== TAS ======== 

void handle_0x9B(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "TAS", cpu, memory, ppu);
    cpu->S = cpu->A & cpu->X;
    uint8_t value = cpu->S & ((effective_addr >> 8) + 1);
    memory_write(effective_addr, value, memory, ppu);
    cpu->cycles = 5;
    return;
}
        
// ======== LAS ======== 

void handle_0xBB(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu_absolute_y(opcode, "LAS", cpu, memory, ppu);
    uint8_t value = memory_read(effective_addr, memory, ppu) & cpu->S;
    cpu->A = cpu->X = cpu->S = value;
    update_zero_and_negative_flags(cpu, value);
    cpu->cycles = 4;
    return;
}

void cpu_irq(CPU *cpu, MEM *memory, PPU *ppu) { // HARDWARE interrupts 
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

void cpu_nmi(CPU *cpu, MEM *memory, PPU *ppu) { // Non-Maskable Interrupt
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

// ======================= Instructions =======================

void lda(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu); 
    DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
    cpu->A = value;
    update_zero_and_negative_flags(cpu, cpu->A);
}

void sta(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    memory_write(effective_addr, cpu->A, memory, ppu);
}

void ldx(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu); 
    DEBUG_MSG_CPU("Writing 0x%02X to register X", value);
    cpu->X = value;
    update_zero_and_negative_flags(cpu, cpu->X);
}

void stx(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    memory_write(effective_addr, cpu->X, memory, ppu);
}

void ldy(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu); 
    DEBUG_MSG_CPU("Writing 0x%02X to register Y", value);
    cpu->Y = value;
    update_zero_and_negative_flags(cpu, cpu->Y);
}

void sty(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    memory_write(effective_addr, cpu->Y, memory, ppu);
}

void adc(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t operand = memory_read(effective_addr, memory, ppu);
    uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
    uint16_t result = cpu->A + operand + carry_in;

    cpu->P = (result > 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
    cpu->P = (((cpu->A ^ result) & (operand ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

    cpu->A = (uint8_t) result;

    update_zero_and_negative_flags(cpu, cpu->A);
}

void sbc(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t operand = memory_read(effective_addr, memory, ppu);

    uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
    uint16_t result = cpu->A + ~operand + carry_in;

    cpu->P = (result <= 0xFF) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
    cpu->P = (((cpu->A ^ operand) & (cpu->A ^ result) & 0x80) != 0) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);

    cpu->A = (uint8_t) result;

    update_zero_and_negative_flags(cpu, cpu->A);
}

void inc(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu);
    // Increment value and write back to memory
    value++; 
    memory_write(effective_addr, value, memory, ppu);
    update_zero_and_negative_flags(cpu, value);
}

void dec(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu);
    // decrement value and write back to memory
    value--;
    memory_write(effective_addr, value, memory, ppu);
    update_zero_and_negative_flags(cpu, value);
}

void lsr(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu);

    // Carry Flag
    cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

    // Logical Shift Right
    value >>= 1;

    memory_write(effective_addr, value, memory, ppu);

    // Set flags
    cpu->P = (value == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
    cpu->P &= ~FLAG_NEGATIVE; // bit 8 is always cleared after shift
}

void asl(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu);

    // Carry Flag
    cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

    // Arithmetic Shift Left
    value <<= 1;

    // Set flags
    update_zero_and_negative_flags(cpu, value);

    // write back to memory
    memory_write(effective_addr, value, memory, ppu);
}

void rol(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu);

    uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 1 : 0;
    cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

    value = (value << 1) | old_carry;

    memory_write(effective_addr, value, memory, ppu);

    update_zero_and_negative_flags(cpu, value);
}

void ror(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu);

    uint8_t old_carry = (cpu->P & FLAG_CARRY) ? 1 : 0;
    uint8_t new_carry = value & 0x01;

    value >>= 1;
    value |= (old_carry << 7);

    memory_write(effective_addr, value, memory, ppu);

    cpu->P = (new_carry) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

    update_zero_and_negative_flags(cpu, value);
}

void and(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t operand = memory_read(effective_addr, memory, ppu);
    uint8_t value = cpu->A & operand;
    DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
    cpu->A = value;
    update_zero_and_negative_flags(cpu, cpu->A);
}

void ora(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t operand = memory_read(effective_addr, memory, ppu);
    uint8_t value = cpu->A | operand;
    DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
    cpu->A = value;
    update_zero_and_negative_flags(cpu, cpu->A);
}

void eor(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t operand = memory_read(effective_addr, memory, ppu);
    uint8_t value = cpu->A ^ operand;
    DEBUG_MSG_CPU("Writing 0x%02X to register A", value);
    cpu->A = value;
    update_zero_and_negative_flags(cpu, cpu->A);
}

void bit(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t operand = memory_read(effective_addr, memory, ppu);

    // Perform bitwise AND 
    uint8_t result = cpu->A & operand;

    // Set flags
    cpu->P = (result == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
    cpu->P = (operand & 0x40) ? (cpu->P | FLAG_OVERFLOW) : (cpu->P & ~FLAG_OVERFLOW);
    cpu->P = (operand & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
}

void cmp(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu);
    uint8_t result = cpu->A - value;

    // Set flags
    cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
    cpu->P = ((result & 0xFF) == 0) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
    cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
}

void cpx(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu);
    uint8_t result = cpu->X - value;

    // Set flags
    cpu->P = (cpu->X >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
    cpu->P = (cpu->X == value) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
    cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
}

void cpy(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu);
    uint8_t result = cpu->Y - value;

    // Set flags
    cpu->P = (cpu->Y >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
    cpu->P = (cpu->Y == value) ? (cpu->P | FLAG_ZERO) : (cpu->P & ~FLAG_ZERO);
    cpu->P = (result & 0x80) ? (cpu->P | FLAG_NEGATIVE) : (cpu->P & ~FLAG_NEGATIVE);
}

void branch(int condition, int8_t offset, CPU *cpu) {
    if (condition) { 
        uint16_t address = cpu->PC + offset;
        cpu->cycles++;
        if ((cpu->PC & 0xFF00) != (address & 0xFF00)) { // page crossed
            cpu->cycles++; 
        }
        DEBUG_MSG_CPU("Branching to 0x%04X", address);
        cpu->PC = address;
    }
}

void slo(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu);

    // Carry Flag
    cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

    // Arithmetic Shift Left
    value <<= 1;

    memory_write(effective_addr, value, memory, ppu);

    // OR result with A and store back in A
    cpu->A |= value;

    update_zero_and_negative_flags(cpu, cpu->A);
}

void lax(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu);
    cpu->A = value;
    cpu->X = value;
    update_zero_and_negative_flags(cpu, value);
}

void sax(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    // Store A & X into memory
    uint8_t result = cpu->A & cpu->X;
    memory_write(effective_addr, result, memory, ppu);
}

void dcp(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu);

    value--; // Decrement memory
    memory_write(effective_addr, value, memory, ppu);

    // Perform CMP (A - value)
    uint8_t result = cpu->A - value;

    // Set Carry Flag if A >= M
    cpu->P = (cpu->A >= value) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

    update_zero_and_negative_flags(cpu, result);
}

void isc(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
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
}

void rla(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu);

    // Rotate Left through Carry
    uint8_t carry_in = (cpu->P & FLAG_CARRY) ? 1 : 0;
    cpu->P = (value & 0x80) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);
    value = (value << 1) | carry_in;

    memory_write(effective_addr, value, memory, ppu);

    // AND with accumulator
    cpu->A &= value;

    update_zero_and_negative_flags(cpu, cpu->A);
}

void sre(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t value = memory_read(effective_addr, memory, ppu);

    // Carry Flag from bit 0 before shift
    cpu->P = (value & 0x01) ? (cpu->P | FLAG_CARRY) : (cpu->P & ~FLAG_CARRY);

    // Logical Shift Right
    value >>= 1;
    memory_write(effective_addr, value, memory, ppu);

    // EOR with accumulator
    cpu->A ^= value;

    update_zero_and_negative_flags(cpu, cpu->A);
}

void rra(uint16_t effective_addr, CPU *cpu, MEM *memory, PPU *ppu) {
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
}

void nop_implied(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)cpu; (void)memory; (void)ppu;
    DEBUG_MSG_CPU("Executing instruction [NOP]: %02X", opcode);
    cpu->cycles = 2;
    return;
}

void nop_immediate(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    cpu_immediate(opcode, "NOP", cpu, memory, ppu);
    cpu->cycles = 2;
    return;
}

void nop_zero_page(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    cpu_zero_page(opcode, "NOP", cpu, memory, ppu);
    cpu->cycles = 3;
    return;
}

void nop_zero_page_x(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    cpu_zero_page_x(opcode, "NOP", cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

void nop_absolute(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    cpu_absolute(opcode, "NOP", cpu, memory, ppu);
    cpu->cycles = 4;
    return;
}

void nop_absolute_x(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    cpu_absolute_x(opcode, "NOP", cpu, memory, ppu);
    cpu->cycles = 4;
    if (cpu->page_crossed) {
        cpu->cycles += 1;
    }
    return;
}

void invalid(CPU *cpu, uint8_t opcode, MEM *memory, PPU *ppu) {
    (void)cpu; (void)opcode; (void)memory; (void)ppu;
    cpu->PC++;
    cpu->cycles = 0;
    return;
}

// ======================= Addressing Modes =======================

uint16_t cpu_immediate(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu) {
    uint16_t effective_addr = cpu->PC++;
    uint16_t immediate = memory_read(effective_addr, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [%s #Immediate]: %02X %02X", instruction, opcode, immediate);
    return effective_addr;
}

uint16_t cpu_zero_page(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [%s ZeroPage]: %02X %02X", instruction, opcode, zero_addr);

    uint16_t effective_addr = (uint16_t) zero_addr;

    return effective_addr;
}

uint16_t cpu_zero_page_x(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [%s ZeroPage, X]: %02X %02X", instruction, opcode, zero_addr);

    uint8_t wrapped_addr = (zero_addr + cpu->X) & 0xFF; // wraps around 0x00 - 0xFF
    uint16_t effective_addr = (uint16_t) wrapped_addr; 

    return effective_addr;
}

uint16_t cpu_zero_page_y(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t zero_addr = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [%s ZeroPage, Y]: %02X %02X", instruction, opcode, zero_addr);

    uint8_t wrapped_addr = (zero_addr + cpu->Y) & 0xFF; // wraps around 0x00 - 0xFF
    uint16_t effective_addr = (uint16_t) wrapped_addr; 

    return effective_addr;
}

uint16_t cpu_absolute(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu) {
    uint8_t low = memory_read(cpu->PC++, memory, ppu);
    uint8_t high = memory_read(cpu->PC++, memory, ppu);
    DEBUG_MSG_CPU("Executing instruction [%s Absolute]: %02X %02X %02X", instruction, opcode, low, high);

    uint16_t effective_addr = (uint16_t) (high << 8 | low);

    return effective_addr;
}

uint16_t cpu_absolute_x(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu) {
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

uint16_t cpu_absolute_y(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu) {
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

uint16_t cpu_indirect_x(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu) {
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

uint16_t cpu_indirect_y(uint8_t opcode, const char *instruction, CPU *cpu, MEM *memory, PPU *ppu) {
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

void init_opcode_table() {
    opcode_table[0x00] = handle_0x00;
    opcode_table[0x01] = handle_0x01;
    opcode_table[0x02] = invalid;
    opcode_table[0x03] = handle_0x03;
    opcode_table[0x04] = nop_zero_page;
    opcode_table[0x05] = handle_0x05;
    opcode_table[0x06] = handle_0x06;
    opcode_table[0x07] = handle_0x07;
    opcode_table[0x08] = handle_0x08;
    opcode_table[0x09] = handle_0x09;
    opcode_table[0x0A] = handle_0x0A;
    opcode_table[0x0B] = handle_0x0B;
    opcode_table[0x0C] = nop_absolute;
    opcode_table[0x0D] = handle_0x0D;
    opcode_table[0x0E] = handle_0x0E;
    opcode_table[0x0F] = handle_0x0F;
    opcode_table[0x10] = handle_0x10;
    opcode_table[0x11] = handle_0x11;
    opcode_table[0x12] = invalid;
    opcode_table[0x13] = handle_0x13;
    opcode_table[0x14] = nop_zero_page_x;
    opcode_table[0x15] = handle_0x15;
    opcode_table[0x16] = handle_0x16;
    opcode_table[0x17] = handle_0x17;
    opcode_table[0x18] = handle_0x18;
    opcode_table[0x19] = handle_0x19;
    opcode_table[0x1A] = nop_implied;
    opcode_table[0x1B] = handle_0x1B;
    opcode_table[0x1C] = nop_absolute_x;
    opcode_table[0x1D] = handle_0x1D;
    opcode_table[0x1E] = handle_0x1E;
    opcode_table[0x1F] = handle_0x1F;
    opcode_table[0x20] = handle_0x20;
    opcode_table[0x21] = handle_0x21;
    opcode_table[0x22] = invalid;
    opcode_table[0x23] = handle_0x23;
    opcode_table[0x24] = handle_0x24;
    opcode_table[0x25] = handle_0x25;
    opcode_table[0x26] = handle_0x26;
    opcode_table[0x27] = handle_0x27;
    opcode_table[0x28] = handle_0x28;
    opcode_table[0x29] = handle_0x29;
    opcode_table[0x2A] = handle_0x2A;
    opcode_table[0x2B] = handle_0x2B;
    opcode_table[0x2C] = handle_0x2C;
    opcode_table[0x2D] = handle_0x2D;
    opcode_table[0x2E] = handle_0x2E;
    opcode_table[0x2F] = handle_0x2F;
    opcode_table[0x30] = handle_0x30;
    opcode_table[0x31] = handle_0x31;
    opcode_table[0x32] = invalid;
    opcode_table[0x33] = handle_0x33;
    opcode_table[0x34] = nop_zero_page_x;
    opcode_table[0x35] = handle_0x35;
    opcode_table[0x36] = handle_0x36;
    opcode_table[0x37] = handle_0x37;
    opcode_table[0x38] = handle_0x38;
    opcode_table[0x39] = handle_0x39;
    opcode_table[0x3A] = nop_implied;
    opcode_table[0x3B] = handle_0x3B;
    opcode_table[0x3C] = nop_absolute_x;
    opcode_table[0x3D] = handle_0x3D;
    opcode_table[0x3E] = handle_0x3E;
    opcode_table[0x3F] = handle_0x3F;
    opcode_table[0x40] = handle_0x40;
    opcode_table[0x41] = handle_0x41;
    opcode_table[0x42] = invalid;
    opcode_table[0x43] = handle_0x43;
    opcode_table[0x44] = nop_zero_page;
    opcode_table[0x45] = handle_0x45;
    opcode_table[0x46] = handle_0x46;
    opcode_table[0x47] = handle_0x47;
    opcode_table[0x48] = handle_0x48;
    opcode_table[0x49] = handle_0x49;
    opcode_table[0x4A] = handle_0x4A;
    opcode_table[0x4B] = handle_0x4B;
    opcode_table[0x4C] = handle_0x4C;
    opcode_table[0x4D] = handle_0x4D;
    opcode_table[0x4E] = handle_0x4E;
    opcode_table[0x4F] = handle_0x4F;
    opcode_table[0x50] = handle_0x50;
    opcode_table[0x51] = handle_0x51;
    opcode_table[0x52] = invalid;
    opcode_table[0x53] = handle_0x53;
    opcode_table[0x54] = nop_zero_page_x;
    opcode_table[0x55] = handle_0x55;
    opcode_table[0x56] = handle_0x56;
    opcode_table[0x57] = handle_0x57;
    opcode_table[0x58] = handle_0x58;
    opcode_table[0x59] = handle_0x59;
    opcode_table[0x5A] = nop_implied;
    opcode_table[0x5B] = handle_0x5B;
    opcode_table[0x5C] = nop_absolute_x;
    opcode_table[0x5D] = handle_0x5D;
    opcode_table[0x5E] = handle_0x5E;
    opcode_table[0x5F] = handle_0x5F;
    opcode_table[0x60] = handle_0x60;
    opcode_table[0x61] = handle_0x61;
    opcode_table[0x62] = invalid;
    opcode_table[0x63] = handle_0x63;
    opcode_table[0x64] = nop_zero_page;
    opcode_table[0x65] = handle_0x65;
    opcode_table[0x66] = handle_0x66;
    opcode_table[0x67] = handle_0x67;
    opcode_table[0x68] = handle_0x68;
    opcode_table[0x69] = handle_0x69;
    opcode_table[0x6A] = handle_0x6A;
    opcode_table[0x6B] = handle_0x6B;
    opcode_table[0x6C] = handle_0x6C;
    opcode_table[0x6D] = handle_0x6D;
    opcode_table[0x6E] = handle_0x6E;
    opcode_table[0x6F] = handle_0x6F;
    opcode_table[0x70] = handle_0x70;
    opcode_table[0x71] = handle_0x71;
    opcode_table[0x72] = invalid;
    opcode_table[0x73] = handle_0x73;
    opcode_table[0x74] = nop_zero_page_x;
    opcode_table[0x75] = handle_0x75;
    opcode_table[0x76] = handle_0x76;
    opcode_table[0x77] = handle_0x77;
    opcode_table[0x78] = handle_0x78;
    opcode_table[0x79] = handle_0x79;
    opcode_table[0x7A] = nop_implied;
    opcode_table[0x7B] = handle_0x7B;
    opcode_table[0x7C] = nop_absolute_x;
    opcode_table[0x7D] = handle_0x7D;
    opcode_table[0x7E] = handle_0x7E;
    opcode_table[0x7F] = handle_0x7F;
    opcode_table[0x80] = nop_immediate;
    opcode_table[0x81] = handle_0x81;
    opcode_table[0x82] = nop_immediate;
    opcode_table[0x83] = handle_0x83;
    opcode_table[0x84] = handle_0x84;
    opcode_table[0x85] = handle_0x85;
    opcode_table[0x86] = handle_0x86;
    opcode_table[0x87] = handle_0x87;
    opcode_table[0x88] = handle_0x88;
    opcode_table[0x89] = nop_immediate;
    opcode_table[0x8A] = handle_0x8A;
    opcode_table[0x8B] = handle_0x8B;
    opcode_table[0x8C] = handle_0x8C;
    opcode_table[0x8D] = handle_0x8D;
    opcode_table[0x8E] = handle_0x8E;
    opcode_table[0x8F] = handle_0x8F;
    opcode_table[0x90] = handle_0x90;
    opcode_table[0x91] = handle_0x91;
    opcode_table[0x92] = invalid;
    opcode_table[0x93] = handle_0x93;
    opcode_table[0x94] = handle_0x94;
    opcode_table[0x95] = handle_0x95;
    opcode_table[0x96] = handle_0x96;
    opcode_table[0x97] = handle_0x97;
    opcode_table[0x98] = handle_0x98;
    opcode_table[0x99] = handle_0x99;
    opcode_table[0x9A] = handle_0x9A;
    opcode_table[0x9B] = handle_0x9B;
    opcode_table[0x9C] = handle_0x9C;
    opcode_table[0x9D] = handle_0x9D;
    opcode_table[0x9E] = handle_0x9E;
    opcode_table[0x9F] = handle_0x9F;
    opcode_table[0xA0] = handle_0xA0;
    opcode_table[0xA1] = handle_0xA1;
    opcode_table[0xA2] = handle_0xA2;
    opcode_table[0xA3] = handle_0xA3;
    opcode_table[0xA4] = handle_0xA4;
    opcode_table[0xA5] = handle_0xA5;
    opcode_table[0xA6] = handle_0xA6;
    opcode_table[0xA7] = handle_0xA7;
    opcode_table[0xA8] = handle_0xA8;
    opcode_table[0xA9] = handle_0xA9;
    opcode_table[0xAA] = handle_0xAA;
    opcode_table[0xAB] = handle_0xAB;
    opcode_table[0xAC] = handle_0xAC;
    opcode_table[0xAD] = handle_0xAD;
    opcode_table[0xAE] = handle_0xAE;
    opcode_table[0xAF] = handle_0xAF;
    opcode_table[0xB0] = handle_0xB0;
    opcode_table[0xB1] = handle_0xB1;
    opcode_table[0xB2] = invalid;
    opcode_table[0xB3] = handle_0xB3;
    opcode_table[0xB4] = handle_0xB4;
    opcode_table[0xB5] = handle_0xB5;
    opcode_table[0xB6] = handle_0xB6;
    opcode_table[0xB7] = handle_0xB7;
    opcode_table[0xB8] = handle_0xB8;
    opcode_table[0xB9] = handle_0xB9;
    opcode_table[0xBA] = handle_0xBA;
    opcode_table[0xBB] = handle_0xBB;
    opcode_table[0xBC] = handle_0xBC;
    opcode_table[0xBD] = handle_0xBD;
    opcode_table[0xBE] = handle_0xBE;
    opcode_table[0xBF] = handle_0xBF;
    opcode_table[0xC0] = handle_0xC0;
    opcode_table[0xC1] = handle_0xC1;
    opcode_table[0xC2] = nop_immediate;
    opcode_table[0xC3] = handle_0xC3;
    opcode_table[0xC4] = handle_0xC4;
    opcode_table[0xC5] = handle_0xC5;
    opcode_table[0xC6] = handle_0xC6;
    opcode_table[0xC7] = handle_0xC7;
    opcode_table[0xC8] = handle_0xC8;
    opcode_table[0xC9] = handle_0xC9;
    opcode_table[0xCA] = handle_0xCA;
    opcode_table[0xCB] = handle_0xCB;
    opcode_table[0xCC] = handle_0xCC;
    opcode_table[0xCD] = handle_0xCD;
    opcode_table[0xCE] = handle_0xCE;
    opcode_table[0xCF] = handle_0xCF;
    opcode_table[0xD0] = handle_0xD0;
    opcode_table[0xD1] = handle_0xD1;
    opcode_table[0xD2] = invalid;
    opcode_table[0xD3] = handle_0xD3;
    opcode_table[0xD4] = nop_zero_page_x;
    opcode_table[0xD5] = handle_0xD5;
    opcode_table[0xD6] = handle_0xD6;
    opcode_table[0xD7] = handle_0xD7;
    opcode_table[0xD8] = handle_0xD8;
    opcode_table[0xD9] = handle_0xD9;
    opcode_table[0xDA] = nop_implied;
    opcode_table[0xDB] = handle_0xDB;
    opcode_table[0xDC] = nop_absolute_x;
    opcode_table[0xDD] = handle_0xDD;
    opcode_table[0xDE] = handle_0xDE;
    opcode_table[0xDF] = handle_0xDF;
    opcode_table[0xE0] = handle_0xE0;
    opcode_table[0xE1] = handle_0xE1;
    opcode_table[0xE2] = nop_immediate;
    opcode_table[0xE3] = handle_0xE3;
    opcode_table[0xE4] = handle_0xE4;
    opcode_table[0xE5] = handle_0xE5;
    opcode_table[0xE6] = handle_0xE6;
    opcode_table[0xE7] = handle_0xE7;
    opcode_table[0xE8] = handle_0xE8;
    opcode_table[0xE9] = handle_0xE9;
    opcode_table[0xEA] = nop_implied;
    opcode_table[0xEB] = handle_0xEB;
    opcode_table[0xEC] = handle_0xEC;
    opcode_table[0xED] = handle_0xED;
    opcode_table[0xEE] = handle_0xEE;
    opcode_table[0xEF] = handle_0xEF;
    opcode_table[0xF0] = handle_0xF0;
    opcode_table[0xF1] = handle_0xF1;
    opcode_table[0xF2] = invalid;
    opcode_table[0xF3] = handle_0xF3;
    opcode_table[0xF4] = nop_zero_page_x;
    opcode_table[0xF5] = handle_0xF5;
    opcode_table[0xF6] = handle_0xF6;
    opcode_table[0xF7] = handle_0xF7;
    opcode_table[0xF8] = handle_0xF8;
    opcode_table[0xF9] = handle_0xF9;
    opcode_table[0xFA] = nop_implied;
    opcode_table[0xFB] = handle_0xFB;
    opcode_table[0xFC] = nop_absolute_x;
    opcode_table[0xFD] = handle_0xFD;
    opcode_table[0xFE] = handle_0xFE;
    opcode_table[0xFF] = handle_0xFF;
}