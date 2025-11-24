#include <stdlib.h>
#include <string.h>
#include "../../include/mapper.h"
#include "../../include/log.h"

#define UxROM_PRG_BANK_SIZE 16384 // 16KB

uint8_t mapper_uxrom_cpu_read(Mapper *m, uint16_t addr);
void mapper_uxrom_cpu_write(Mapper *m, uint16_t addr, uint8_t value);
uint8_t mapper_uxrom_ppu_read(Mapper *m, uint16_t addr);
void mapper_uxrom_ppu_write(Mapper *m, uint16_t addr, uint8_t value);

typedef struct regs_uxrom {
    uint8_t prg_bank; // selected PRG bank number
} regs_uxrom;

void mapper_uxrom_init(Mapper *m) {
    m->cpu_read = mapper_uxrom_cpu_read;
    m->cpu_write = mapper_uxrom_cpu_write;
    m->ppu_read = mapper_uxrom_ppu_read;
    m->ppu_write = mapper_uxrom_ppu_write;

    m->regs = (regs_uxrom *)malloc(sizeof(regs_uxrom));
    memset(m->regs, 0, sizeof(regs_uxrom));
}

uint8_t mapper_uxrom_cpu_read(Mapper *m, uint16_t addr) {
    regs_uxrom *regs = (regs_uxrom *)m->regs;

    // two 16KB PRG banks
    // fixed last bank at 0xC000-0xFFFF
    if (addr >= 0xC000) {
        // fixed to last 16KB bank
        uint16_t offset_into_bank = addr - 0xC000;
        uint32_t prg_addr = (m->cart->prg_size - UxROM_PRG_BANK_SIZE) + offset_into_bank;
        return m->cart->prg_rom[prg_addr];
    } 
    // switchable bank at 0x8000-0xBFFF
    else if (addr >= 0x8000 && addr < 0xC000) {
        // switchable bank
        uint8_t bank = regs->prg_bank & 0x07; // get selected bank from regs[0]
        uint16_t offset_into_bank = addr - 0x8000;
        uint32_t prg_addr = (bank * UxROM_PRG_BANK_SIZE) + offset_into_bank;
        // wrap around if bank exceeds available banks
        prg_addr = prg_addr % m->cart->prg_size;
        return m->cart->prg_rom[prg_addr];
    }
    return 0;
}

void mapper_uxrom_cpu_write(Mapper *m, uint16_t addr, uint8_t value) {
    regs_uxrom *regs = (regs_uxrom *)m->regs;

    // writes to 0x8000-0xFFFF set PRG bank
    if (addr >= 0x8000) {
        uint8_t bank = value & 0x0F; // lower 4 bits
        regs->prg_bank = bank; // store selected bank in regs[0]
    }
}

uint8_t mapper_uxrom_ppu_read(Mapper *m, uint16_t addr) {  
    // CHR ROM/RAM: 0x0000-0x1FFF 
    if (addr < 0x2000) {
        return m->cart->chr_rom[addr];
    }
    return 0; 
}

void mapper_uxrom_ppu_write(Mapper *m, uint16_t addr, uint8_t value) {    
    // CHR ROM/RAM: 0x0000-0x1FFF 
    if (addr < 0x2000) {
        m->cart->chr_rom[addr] = value;
    } 
}