#include <stdlib.h>
#include <string.h>
#include "../../include/mapper.h"
#include "../../include/log.h"

uint8_t mapper_nrom_cpu_read(Mapper *m, uint16_t addr);
void mapper_nrom_cpu_write(Mapper *m, uint16_t addr, uint8_t value);
uint8_t mapper_nrom_ppu_read(Mapper *m, uint16_t addr);
void mapper_nrom_ppu_write(Mapper *m, uint16_t addr, uint8_t value);

void mapper_nrom_init(Mapper *m) {
    m->cpu_read = mapper_nrom_cpu_read;
    m->cpu_write = mapper_nrom_cpu_write;
    m->ppu_read = mapper_nrom_ppu_read;
    m->ppu_write = mapper_nrom_ppu_write;

    m->regs = NULL; // NROM has no registers
}

uint8_t mapper_nrom_cpu_read(Mapper *m, uint16_t addr) {
    // PRG ROM: 0x8000-0xFFFF
    if (addr >= 0x8000) {
        uint16_t prg_addr = addr - 0x8000;
        
        // if PRG ROM is 16KB, mirror it to fill 32KB space
        if (m->cart->prg_size == 16384) {
            prg_addr = prg_addr % 16384;
        }
        
        return m->cart->prg_rom[prg_addr];
    }
    
    return 0; 
}

void mapper_nrom_cpu_write(Mapper *m, uint16_t addr, uint8_t value) {
    (void)m;
    (void)addr;
    (void)value;
    // NROM has no writable registers
    // Writes to 0x8000-0xFFFF are ignored
}

uint8_t mapper_nrom_ppu_read(Mapper *m, uint16_t addr) {    
    // CHR ROM/RAM: 0x0000-0x1FFF
    if (addr < 0x2000) {
        return m->cart->chr_rom[addr];
    }
    return 0; 
}

void mapper_nrom_ppu_write(Mapper *m, uint16_t addr, uint8_t value) {    
    // CHR ROM/RAM: 0x0000-0x1FFF (RAM if chr_size == 0)
    if (addr < 0x2000 && m->cart->chr_size == 0) {
        m->cart->chr_rom[addr] = value;
    }
}