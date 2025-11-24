#include <stdlib.h>
#include <string.h>
#include "../../include/mapper.h"
#include "../../include/log.h"

uint8_t mapper_mmc1_cpu_read(Mapper *m, uint16_t addr);
void mapper_mmc1_cpu_write(Mapper *m, uint16_t addr, uint8_t value);
uint8_t mapper_mmc1_ppu_read(Mapper *m, uint16_t addr);
void mapper_mmc1_ppu_write(Mapper *m, uint16_t addr, uint8_t value);

uint16_t mirror_nametable_mmc1(Mapper *m, uint16_t address);

typedef struct regs_mmc1 {
    // shift register (5 bits)
    uint8_t shift_reg;     
    int shift_count; // keeps track of number of bits shifted in
    
    // control register
    uint8_t prg_bank_mode;       // 0,1: 32KB; 2: fix first bank; 3: fix last bank
    uint8_t chr_bank_mode;       // 0: 8KB; 1: two 4KB banks

    // CHR bank 0 register
    uint8_t chr_bank_0;

    // CHR bank 1 register
    uint8_t chr_bank_1;

    // PRG bank register
    uint8_t prg_bank;
    int prg_ram_en; // active low
} regs_mmc1;

void mapper_mmc1_init(Mapper *m) {
    m->cpu_read = mapper_mmc1_cpu_read;
    m->cpu_write = mapper_mmc1_cpu_write;
    m->ppu_read = mapper_mmc1_ppu_read;
    m->ppu_write = mapper_mmc1_ppu_write;

    m->regs = (struct regs_mmc1 *)malloc(sizeof(struct regs_mmc1));
    memset(m->regs, 0, sizeof(struct regs_mmc1));

    regs_mmc1 *regs = (regs_mmc1 *)m->regs; // load to set default values
    regs->prg_bank_mode = 3; // default to fix last bank because thats where reset vector is
    regs->chr_bank_mode = 0; // default to 8KB mode

    // convert initial mirroring from cartridge to MMC1 format
    switch (m->cart->mirroring) {
        case 0: 
            m->mirroring = MIRROR_SINGLE_LOWER;
            break;
        case 1: 
            m->mirroring = MIRROR_SINGLE_UPPER;
            break;
        case 2: 
            m->mirroring = MIRROR_VERTICAL;
            break;
        case 3: 
            m->mirroring = MIRROR_HORIZONTAL;
            break;
    }

    // override default nametable mirroring function
    m->mirror_nametable = mirror_nametable_mmc1; 
}

uint8_t mapper_mmc1_cpu_read(Mapper *m, uint16_t addr) {
    regs_mmc1 *regs = (regs_mmc1 *)m->regs;

    // handle PRG RAM
    if (addr >= 0x6000 && addr < 0x8000) {
        // check if PRG RAM is enabled
        if (regs->prg_ram_en == 0) { // active low enable
            // read from PRG RAM
            uint16_t prg_ram_addr = (addr - 0x6000) % m->cart->prg_ram_size;
            return  m->cart->prg_ram[prg_ram_addr];
        } else {
            return 0; 
        }
    }
    // handle PRG ROM
    else if (addr >= 0x8000 && addr <= 0xFFFF) {
        // 32KB bank size mode
        if (regs->prg_bank_mode == 0 || regs->prg_bank_mode == 1) {
            uint8_t bank = (regs->prg_bank & 0x0E) >> 1; // get bank index (ignore LSB for 32KB mode)
            uint16_t offset = addr - 0x8000; // offset into bank
            uint32_t prg_addr = (bank * 0x8000) + offset; // calculate physical address
            prg_addr = prg_addr % m->cart->prg_size; // wrap around if out of bounds
            return m->cart->prg_rom[prg_addr];
        }
        // 16KB fixed first bank mode
        else if (regs->prg_bank_mode == 2) {
            if (addr >= 0x8000 && addr < 0xC000) {
                // fixed to first bank
                return m->cart->prg_rom[addr - 0x8000];
            } else {
                // switchable bank
                uint8_t bank = regs->prg_bank & 0x0F; // get bank index (all bits for 16KB mode because we need to address more banks)
                uint16_t offset = addr - 0xC000; // offset into bank
                uint32_t prg_addr = (bank * 0x4000) + offset; // calculate physical address
                prg_addr = prg_addr % m->cart->prg_size; // wrap around if out of bounds
                return m->cart->prg_rom[prg_addr];
            }
        }
        // 16KB fixed last bank mode
        else if (regs->prg_bank_mode == 3) {
            if (addr >= 0x8000 && addr < 0xC000) {
                // switchable bank
                uint8_t bank = regs->prg_bank & 0x0F; // get bank index (all bits for 16KB mode because we need to address more banks)
                uint16_t offset = addr - 0x8000; // offset into bank
                uint32_t prg_addr = (bank * 0x4000) + offset; // calculate physical address
                prg_addr = prg_addr % m->cart->prg_size; // wrap around if out of bounds
                return m->cart->prg_rom[prg_addr];
            } else {
                // fixed to last bank
                uint32_t last_bank_start = m->cart->prg_size - 0x4000; // start of last 16KB bank
                uint16_t offset = addr - 0xC000; // offset into bank
                uint32_t prg_addr = last_bank_start + offset; // calculate physical address
                return m->cart->prg_rom[prg_addr];
            }
        } 
    } 

    return 0; 
}

void mapper_mmc1_cpu_write(Mapper *m, uint16_t addr, uint8_t value) {
    regs_mmc1 *regs = (regs_mmc1 *)m->regs;

    // handle PRG RAM
    if (addr >= 0x6000 && addr < 0x8000) {
        if (regs->prg_ram_en == 0) { // active low enable
            // write to PRG RAM
            uint16_t prg_ram_addr = (addr - 0x6000) % m->cart->prg_ram_size;
            m->cart->prg_ram[prg_ram_addr] = value;
        }
    }
    // handle registers
    else if (addr >= 0x8000 && addr <= 0xFFFF) {
        // if bit 7 is set, reset shift register
        if (value & 0x80) {
            regs->shift_reg = 0;
            regs->shift_count = 0;
            // set control register to default state
            regs->prg_bank_mode = 3;
            regs->chr_bank_mode = 0;

            // convert initial mirroring from cartridge to MMC1 format
            switch (m->cart->mirroring) {
                case 0: 
                    m->mirroring = MIRROR_SINGLE_LOWER;
                    break;
                case 1: 
                    m->mirroring = MIRROR_SINGLE_UPPER;
                    break;
                case 2: 
                    m->mirroring = MIRROR_VERTICAL;
                    break;
                case 3: 
                    m->mirroring = MIRROR_HORIZONTAL;
                    break;
            }
            return;
        }
        // shift in LSB of value into shift register
        else {
            regs->shift_reg >>= 1; // shift right
            regs->shift_reg |= (value & 0x01) << 4; // set bit 4 to LSB of value
            regs->shift_count++;

            // if 5 bits have been shifted in, write to appropriate register
            if (regs->shift_count == 5) {
                // control register logic (last write to 0x8000-0x9FFF)
                if (addr >= 0x8000 && addr <= 0x9FFF) {
                    regs->prg_bank_mode = (regs->shift_reg >> 2) & 0x03;
                    regs->chr_bank_mode = (regs->shift_reg >> 4) & 0x01;
                    // set mirroring mode
                    switch (regs->shift_reg & 0x03) {
                        case 0:
                            printf("New Mirroring Mode: SINGLE_LOWER\n");
                            m->mirroring = MIRROR_SINGLE_LOWER;
                            break;
                        case 1:
                            printf("New Mirroring Mode: SINGLE_UPPER\n");
                            m->mirroring = MIRROR_SINGLE_UPPER;
                            break;
                        case 2:
                            printf("New Mirroring Mode: VERTICAL\n");
                            m->mirroring = MIRROR_VERTICAL;
                            break;
                        case 3:
                            printf("New Mirroring Mode: HORIZONTAL\n");
                            m->mirroring = MIRROR_HORIZONTAL;
                            break;
                    }
                }
                // CHR bank 0 (last write to 0xA000-0xBFFF)
                else if (addr >= 0xA000 && addr <= 0xBFFF) {
                    regs->chr_bank_0 = regs->shift_reg & 0x1F;
                }  
                // CHR bank 1 (last write to 0xC000-0xDFFF)
                else if (addr >= 0xC000 && addr <= 0xDFFF) {
                    regs->chr_bank_1 = regs->shift_reg & 0x1F;
                }  
                // PRG bank (last write to 0xE000-0xFFFF)
                else if (addr >= 0xE000 && addr <= 0xFFFF) {
                    regs->prg_bank = regs->shift_reg & 0x0F;
                    regs->prg_ram_en = (regs->shift_reg >> 4) & 0x01; // bit 4 controls PRG RAM enable
                }

                // reset shift register
                regs->shift_reg = 0;
                regs->shift_count = 0;
            }
        }
    }
}

uint8_t mapper_mmc1_ppu_read(Mapper *m, uint16_t addr) {    
    regs_mmc1 *regs = (regs_mmc1 *)m->regs;

    // handle CHR ROM/RAM
    if (addr < 0x2000) {
        // 8KB bank mode
        if (regs->chr_bank_mode == 0) {
            uint8_t bank = (regs->chr_bank_0 & 0x1E) >> 1; // get bank index (ignore LSB for 8KB mode)
            uint16_t offset = addr; // offset into bank
            uint32_t chr_addr = (bank * 0x2000) + offset; // calculate physical address
            chr_addr = chr_addr % m->cart->chr_size; // wrap around if out of bounds
            return m->cart->chr_rom[chr_addr];
        }
        // two 4KB bank mode
        else if (regs->chr_bank_mode == 1) {
            if (addr < 0x1000) {
                // first 4KB bank
                uint8_t bank = regs->chr_bank_0 & 0x1F; // get bank index
                uint16_t offset = addr; // offset into bank
                uint32_t chr_addr = (bank * 0x1000) + offset; // calculate physical address
                chr_addr = chr_addr % m->cart->chr_size; // wrap around if out of bounds
                return m->cart->chr_rom[chr_addr];
            } 
            else {
                // second 4KB bank
                uint8_t bank = regs->chr_bank_1 & 0x1F; // get bank index
                uint16_t offset = addr - 0x1000; // offset into bank
                uint32_t chr_addr = (bank * 0x1000) + offset; // calculate physical address
                chr_addr = chr_addr % m->cart->chr_size; // wrap around if out of bounds
                return m->cart->chr_rom[chr_addr];
            }
        }
    } 
    
    return 0; 
}

void mapper_mmc1_ppu_write(Mapper *m, uint16_t addr, uint8_t value) {    
    regs_mmc1 *regs = (regs_mmc1 *)m->regs;
    
    // handle CHR RAM
    if (addr < 0x2000) {
        // 8KB bank mode
        if (regs->chr_bank_mode == 0) {
            uint8_t bank = (regs->chr_bank_0 & 0x1E) >> 1; // get bank index (ignore LSB for 8KB mode)
            uint16_t offset = addr; // offset into bank
            uint32_t chr_addr = (bank * 0x2000) + offset; // calculate physical address
            chr_addr = chr_addr % m->cart->chr_size; // wrap around if out of bounds
            m->cart->chr_rom[chr_addr] = value;
        }
        // two 4KB bank mode
        else if (regs->chr_bank_mode == 1) {
            if (addr < 0x1000) {
                // first 4KB bank
                uint8_t bank = regs->chr_bank_0 & 0x1F; // get bank index
                uint16_t offset = addr; // offset into bank
                uint32_t chr_addr = (bank * 0x1000) + offset; // calculate physical address
                chr_addr = chr_addr % m->cart->chr_size; // wrap around if out of bounds
                m->cart->chr_rom[chr_addr] = value;
            } 
            else {
                // second 4KB bank
                uint8_t bank = regs->chr_bank_1 & 0x1F; // get bank index
                uint16_t offset = addr - 0x1000; // offset into bank
                uint32_t chr_addr = (bank * 0x1000) + offset; // calculate physical address
                chr_addr = chr_addr % m->cart->chr_size; // wrap around if out of bounds
                m->cart->chr_rom[chr_addr] = value;
            }
        }
    }
}

uint16_t mirror_nametable_mmc1(Mapper *m, uint16_t address) {
    switch (m->mirroring) {
        case MIRROR_VERTICAL:
            if (address >= 0x2800 && address < 0x2C00)
                return address - 0x800;
            if (address >= 0x2C00 && address < 0x3000)
                return address - 0x800;
            break;
            
        case MIRROR_HORIZONTAL:
            if (address >= 0x2400 && address < 0x2800)
                return address - 0x400;
            if (address >= 0x2C00 && address < 0x3000)
                return address - 0x800;
            break;
            
        case MIRROR_SINGLE_LOWER:
            // All addresses map to 0x2000-0x23FF
            if (address >= 0x2400 && address < 0x2800)
                return address - 0x400;
            if (address >= 0x2800 && address < 0x2C00)
                return address - 0x800;
            if (address >= 0x2C00 && address < 0x3000)
                return address - 0xC00;
            break;
            
        case MIRROR_SINGLE_UPPER:
            // All addresses map to 0x2400-0x27FF
            if (address >= 0x2000 && address < 0x2400)
                return address + 0x400;
            if (address >= 0x2800 && address < 0x2C00)
                return address - 0x400;
            if (address >= 0x2C00 && address < 0x3000)
                return address - 0x800;
            break;
    }

    return address;
}