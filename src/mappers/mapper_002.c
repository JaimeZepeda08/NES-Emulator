#include <stdlib.h>
#include <string.h>
#include "../../include/mapper.h"
#include "../../include/log.h"

#define UxROM_PRG_BANK_SIZE 16384 // 16KB

uint8_t mapper_uxrom_cpu_read(Mapper *m, uint16_t addr);
void mapper_uxrom_cpu_write(Mapper *m, uint16_t addr, uint8_t value);
uint8_t mapper_uxrom_ppu_read(Mapper *m, uint16_t addr);
void mapper_uxrom_ppu_write(Mapper *m, uint16_t addr, uint8_t value);

Mapper *mapper_uxrom_init(Cartridge *cart) {
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
    mapper->cpu_read = mapper_uxrom_cpu_read;
    mapper->cpu_write = mapper_uxrom_cpu_write;
    mapper->ppu_read = mapper_uxrom_ppu_read;
    mapper->ppu_write = mapper_uxrom_ppu_write;

    memset(mapper->regs, 0, sizeof(mapper->regs));

    return mapper;
}

uint8_t mapper_uxrom_cpu_read(Mapper *m, uint16_t addr) {
    if (addr >= 0xC000) {
        // fixed to last 16KB bank
        uint16_t offset_into_bank = addr - 0xC000;
        uint32_t prg_addr = (m->cart->prg_size - UxROM_PRG_BANK_SIZE) + offset_into_bank;
        return m->cart->prg_rom[prg_addr];
    } 
    else if (addr >= 0x8000 && addr < 0xC000) {
        // switchable bank
        uint8_t bank = m->regs[0] & 0x07; // get selected bank from regs[0]
        uint16_t offset_into_bank = addr - 0x8000;
        uint32_t prg_addr = (bank * UxROM_PRG_BANK_SIZE) + offset_into_bank;
        // wrap around if bank exceeds available banks
        prg_addr = prg_addr % m->cart->prg_size;
        return m->cart->prg_rom[prg_addr];
    }
}

void mapper_uxrom_cpu_write(Mapper *m, uint16_t addr, uint8_t value) {
    // writes to 0x8000-0xFFFF set PRG bank
    if (addr >= 0x8000) {
        uint8_t bank = value & 0x0F; // lower 4 bits
        m->regs[0] = bank; // store selected bank in regs[0]
    }
}

uint8_t mapper_uxrom_ppu_read(Mapper *m, uint16_t addr) {    
    if (addr < 0x2000) {
        return m->cart->chr_rom[addr];
    }
    return 0; // Open bus behavior
}

void mapper_uxrom_ppu_write(Mapper *m, uint16_t addr, uint8_t value) {    
    if (addr < 0x2000) {
        m->cart->chr_rom[addr] = value;
    } 
}