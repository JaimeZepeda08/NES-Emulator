#include <stdlib.h>
#include <string.h>
#include "../../include/mapper.h"
#include "../../include/log.h"

uint8_t mapper_nrom_cpu_read(Mapper *m, uint16_t addr);
void mapper_nrom_cpu_write(Mapper *m, uint16_t addr, uint8_t value);
uint8_t mapper_nrom_ppu_read(Mapper *m, uint16_t addr);
void mapper_nrom_ppu_write(Mapper *m, uint16_t addr, uint8_t value);

Mapper *mapper_nrom_init(Cartridge *cart) {
    Mapper *mapper = (Mapper *)malloc(sizeof(Mapper));
    if (!mapper) {
        FATAL_ERROR("Mapper", "Failed to allocate NROM mapper");
        return NULL;
    }

    // clear mapper state
    memset(mapper, 0, sizeof(Mapper));

    // set cartridge reference
    mapper->cart = cart;

    // set function pointers
    mapper->cpu_read = mapper_nrom_cpu_read;
    mapper->cpu_write = mapper_nrom_cpu_write;
    mapper->ppu_read = mapper_nrom_ppu_read;
    mapper->ppu_write = mapper_nrom_ppu_write;

    // NROM has no special registers
    memset(mapper->regs, 0, sizeof(mapper->regs));

    return mapper;
}

uint8_t mapper_nrom_cpu_read(Mapper *m, uint16_t addr) {
    // PRG ROM: 0x8000-0xFFFF
    if (addr >= 0x8000) {
        uint16_t prg_addr = addr - 0x8000;
        
        // If PRG ROM is 16KB, mirror it to fill 32KB space
        if (m->cart->prg_size == 16384) {
            prg_addr = prg_addr % 16384;
        }
        
        return m->cart->prg_rom[prg_addr];
    }
    
    return 0; // Open bus behavior
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
    
    return 0; // Out of range
}

void mapper_nrom_ppu_write(Mapper *m, uint16_t addr, uint8_t value) {    
    // CHR RAM: 0x0000-0x1FFF (only if CHR ROM size was 0, meaning CHR RAM)
    if (addr < 0x2000 && m->cart->chr_size == 8192) {
        m->cart->chr_rom[addr] = value;
    }
}