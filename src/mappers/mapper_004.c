#include <stdlib.h>
#include <string.h>
#include "../../include/mapper.h"
#include "../../include/log.h"

#define MMC3_CHR_BANK_SIZE_8K 8192      // 8KB
#define MMC3_CHR_BANK_SIZE_2K 2048      // 2KB
#define MMC3_CHR_BANK_SIZE_1K 1024      // 1KB

uint8_t mapper_mmc3_cpu_read(Mapper *m, uint16_t addr);
void mapper_mmc3_cpu_write(Mapper *m, uint16_t addr, uint8_t value);
uint8_t mapper_mmc3_ppu_read(Mapper *m, uint16_t addr);
void mapper_mmc3_ppu_write(Mapper *m, uint16_t addr, uint8_t value);

void mapper_mmc3_irq_clock(Mapper *m);

typedef struct regs_mmc3 {
    // ========= 0x8000-0x9FFE =========
    // even
    uint8_t bank_select; // 3 bits for bank select
    uint8_t R0; // CHR bank 0 (2KB)
    uint8_t R1; // CHR bank 1 (2KB)
    uint8_t R2; // CHR bank 2 (1KB)
    uint8_t R3; // CHR bank 3 (1KB)
    uint8_t R4; // CHR bank 4 (1KB)
    uint8_t R5; // CHR bank 5 (1KB)
    uint8_t R6; // PRG bank (8KB) (ignores top 2 bits)
    uint8_t R7; // PRG bank (8KB) (ignores top 2 bits)
    int prg_rom_bank_mode;  // 0: 0x8000-0x9FFF swappable, and 0xC000-0xFFFF fixed to last bank
                            // 1: 0xC000-0xDFFF swappable, and 0xE000-0xFFFF fixed to last bank
    int chr_bank_mode;      // 0: two 2KB banks at 0x0000-0x0FFF, four 1KB banks at 0x1000-0x1FFF
                            // 1: four 1KB banks at 0x0000-0x0FFF, two 2KB banks at 0x1000-0x1FFF
    // odd
    // bank data handled during cpu write

    // ========= 0xA000-0xBFFE =========
    // even
    // nametable mirroring will be directly set in mapper 

    // odd
    int prg_ram_protect; // active low
    int prg_ram_enable;  // active high

    // ========= 0xC000-0xDFFE =========
    // even
    uint8_t irq_latch;

    // ========= 0xE000-0xFFFE =========
    int irq_en; // active high

    uint8_t irq_counter;
} regs_mmc3;

void mapper_mmc3_init(Mapper *m) {
    m->cpu_read = mapper_mmc3_cpu_read;
    m->cpu_write = mapper_mmc3_cpu_write;
    m->ppu_read = mapper_mmc3_ppu_read;
    m->ppu_write = mapper_mmc3_ppu_write;

    m->regs = (regs_mmc3 *)malloc(sizeof(regs_mmc3));
    memset(m->regs, 0, sizeof(regs_mmc3));

    m->irq_clock = mapper_mmc3_irq_clock; 
    m->irq = 1;
}

uint8_t mapper_mmc3_cpu_read(Mapper *m, uint16_t addr) {
    regs_mmc3 *regs = (regs_mmc3 *)m->regs;

    // PRG ram
    if (addr >= 0x6000 && addr < 0x8000) {
        // read from PRG RAM if enabled
        if (regs->prg_ram_enable) {
            uint16_t prg_ram_addr = (addr - 0x6000) % m->cart->prg_ram_size;
            return m->cart->prg_ram[prg_ram_addr];
        } else {
            return 0;
        }
    }
    else if (addr >= 0x8000 && addr < 0xA000) {
        if (regs->prg_rom_bank_mode == 0) {
            // use R6 for 0x8000-0x9FFF
            uint8_t bank = regs->R6 & 0x3F; // get bank index (ignore top 2 bits)
            uint16_t offset = addr - 0x8000; // offset into bank
            uint32_t prg_addr = (bank * MMC3_CHR_BANK_SIZE_8K) + offset; // calculate physical address
            prg_addr = prg_addr % m->cart->prg_size; // wrap around if out of bounds
            return m->cart->prg_rom[prg_addr];
        } else {
            // use second to last bank for 0x8000-0x9FFF
            uint32_t second_last_bank_start = m->cart->prg_size - (2 * MMC3_CHR_BANK_SIZE_8K); // start of second to last 8KB bank
            uint16_t offset = addr - 0x8000; // offset into bank
            uint32_t prg_addr = second_last_bank_start + offset; // calculate physical address 
            return m->cart->prg_rom[prg_addr];
        }
    }
    else if (addr >= 0xA000 && addr < 0xC000) {
        // use R7 for 0xA000-0xC000 regardless of mode
        uint8_t bank = regs->R7 & 0x3F; // get bank index (ignore top 2 bits)
        uint16_t offset = addr - 0xA000; // offset into bank
        uint32_t prg_addr = (bank * MMC3_CHR_BANK_SIZE_8K) + offset; // calculate physical address
        prg_addr = prg_addr % m->cart->prg_size; // wrap around if out of bounds
        return m->cart->prg_rom[prg_addr];
    }
    else if (addr >= 0xC000 && addr < 0xE000) {
        if (regs->prg_rom_bank_mode == 0) {
            // use second to last bank for 0xC000-0xE000
            uint32_t second_last_bank_start = m->cart->prg_size - (2 * MMC3_CHR_BANK_SIZE_8K); // start of second to last 8KB bank
            uint16_t offset = addr - 0xC000; // offset into bank
            uint32_t prg_addr = second_last_bank_start + offset; // calculate physical address 
            return m->cart->prg_rom[prg_addr];
        } else {
            // use R6 for 0xC000-0xE000
            uint8_t bank = regs->R6 & 0x3F; // get bank index (ignore top 2 bits)
            uint16_t offset = addr - 0xC000; // offset into bank
            uint32_t prg_addr = (bank * MMC3_CHR_BANK_SIZE_8K) + offset; // calculate physical address
            prg_addr = prg_addr % m->cart->prg_size; // wrap around if out of bounds
            return m->cart->prg_rom[prg_addr];
        }
    }
    // fixed to last bank because of reset vector
    else if (addr >= 0xE000 && addr <= 0xFFFF) {
        // use last bank for 0xE000-0xFFFF regardless of mode
        uint32_t last_bank_start = m->cart->prg_size - MMC3_CHR_BANK_SIZE_8K; // start of last 8KB bank
        uint16_t offset = addr - 0xE000; // offset into bank
        uint32_t prg_addr = last_bank_start + offset; // calculate physical address 
        return m->cart->prg_rom[prg_addr];
    }

    return 0; 
}

void mapper_mmc3_cpu_write(Mapper *m, uint16_t addr, uint8_t value) {
    regs_mmc3 *regs = (regs_mmc3 *)m->regs;

    // PRG ram
    if (addr >= 0x6000 && addr < 0x8000) {
        // write to PRG RAM if enabled and not protected
        if (regs->prg_ram_enable && !regs->prg_ram_protect) {
            uint16_t prg_ram_addr = (addr - 0x6000) % m->cart->prg_ram_size;
            m->cart->prg_ram[prg_ram_addr] = value;
        }
    }
    else if (addr >= 0x8000 && addr < 0xA000) {
        // odd
        if (addr & 0x01) {
            // bank data
            switch (regs->bank_select) {
                case 0:
                    regs->R0 = value;
                    break;
                case 1:
                    regs->R1 = value;
                    break;
                case 2:
                    regs->R2 = value;
                    break;
                case 3:
                    regs->R3 = value;
                    break;
                case 4:
                    regs->R4 = value;
                    break;
                case 5:
                    regs->R5 = value;
                    break;
                case 6:
                    regs->R6 = value;
                    break;
                case 7:
                    regs->R7 = value;
                    break;
            }
        } 
        // even
        else {
            // bank select
            regs->bank_select = addr & 0x07; // first 3 bits
            // PRG ROM bank mode
            regs->prg_rom_bank_mode = (addr >> 6) & 0x01; // bit 6
            // CHR bank mode
            regs->chr_bank_mode = (addr >> 7) & 0x01; // bit 7
        }
    } 
    else if (addr >= 0xA000 && addr < 0xC000) {
        // odd
        if (addr & 0x01) {
            // PRG RAM protect
            regs->prg_ram_protect = (value >> 6) & 0x01; // bit 6
            // PRG RAM enable
            regs->prg_ram_enable = (value >> 7) & 0x01; // bit 7
        } 
        // even
        else {
            // nametable mirroring
            if (value & 0x01) {
                m->mirroring = MIRROR_HORIZONTAL;
            } else {
                m->mirroring = MIRROR_VERTICAL;
            }
        }
    } 
    else if (addr >= 0xC000 && addr < 0xE000) {
        // odd
        if (addr & 0x01) {
            // reload IRQ counter
            regs->irq_counter = 0x00;
        } 
        // even
        else {
            // set IRQ latch
            regs->irq_latch = value;
        }
    } 
    else if (addr >= 0xE000 && addr <= 0xFFFF) {
        // odd
        if (addr & 0x01) {
            // enable IRQ
            regs->irq_en = 1;
        } 
        // even
        else {
            // disable IRQ and acknowledge any pending IRQ
            regs->irq_en = 0;
            m->irq = 0;
        }
    }
}

uint8_t mapper_mmc3_ppu_read(Mapper *m, uint16_t addr) {  
    regs_mmc3 *regs = (regs_mmc3 *)m->regs;

    if (regs->chr_bank_mode == 0) {
        // two 2KB banks at 0x0000-0x0FFF, four 1KB banks at 0x1000-0x1FFF
        if (addr >= 0x0000 && addr < 0x0800) {
            // first 2KB bank
            uint8_t bank = regs->R0;
            uint16_t offset = addr - 0x0000;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_2K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            return m->cart->chr_rom[chr_addr];
        } 
        else if (addr >= 0x0800 && addr < 0x1000) {
            // second 2KB bank
            uint8_t bank = regs->R1;
            uint16_t offset = addr - 0x0800;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_2K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            return m->cart->chr_rom[chr_addr];
        } 
        else if (addr >= 0x1000 && addr < 0x1400) {
            // first 1KB bank
            uint8_t bank = regs->R2;
            uint16_t offset = addr - 0x1000;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            return m->cart->chr_rom[chr_addr];
        } 
        else if (addr >= 0x1400 && addr < 0x1800) {
            // second 1KB bank
            uint8_t bank = regs->R3;
            uint16_t offset = addr - 0x1400;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            return m->cart->chr_rom[chr_addr];
        } 
        else if (addr >= 0x1800 && addr < 0x1C00) {
            // third 1KB bank
            uint8_t bank = regs->R4;
            uint16_t offset = addr - 0x1800;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            return m->cart->chr_rom[chr_addr];
        }
        else if (addr >= 0x1C00 && addr < 0x2000) {
            // fourth 1KB bank
            uint8_t bank = regs->R5;
            uint16_t offset = addr - 0x1C00;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            return m->cart->chr_rom[chr_addr];
        }
        return 0;
    } else {
        // four 1KB banks at 0x0000-0x0FFF, two 2KB banks at 0x1000-0x1FFF
        if (addr >= 0x0000 && addr < 0x0400) {
            // first 1KB bank
            uint8_t bank = regs->R2;
            uint16_t offset = addr - 0x0000;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            return m->cart->chr_rom[chr_addr];
        } 
        else if (addr >= 0x0400 && addr < 0x0800) {
            // second 1KB bank
            uint8_t bank = regs->R3;
            uint16_t offset = addr - 0x0400;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            return m->cart->chr_rom[chr_addr];
        } 
        else if (addr >= 0x0800 && addr < 0x0C00) {
            // third 1KB bank
            uint8_t bank = regs->R4;
            uint16_t offset = addr - 0x0800;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            return m->cart->chr_rom[chr_addr];
        } 
        else if (addr >= 0x0C00 && addr < 0x1000) {
            // fourth 1KB bank
            uint8_t bank = regs->R5;
            uint16_t offset = addr - 0x0C00;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            return m->cart->chr_rom[chr_addr];
        } 
        else if (addr >= 0x1000 && addr < 0x1800) {
            // first 2KB bank
            uint8_t bank = regs->R1;
            uint16_t offset = addr - 0x1000;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_2K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            return m->cart->chr_rom[chr_addr];
        }
        else if (addr >= 0x1800 && addr < 0x2000) {
            // second 2KB bank
            uint8_t bank = regs->R2;
            uint16_t offset = addr - 0x1800;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_2K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            return m->cart->chr_rom[chr_addr];
        }
        return 0;
    }
    return 0;
}

void mapper_mmc3_ppu_write(Mapper *m, uint16_t addr, uint8_t value) {    
    regs_mmc3 *regs = (regs_mmc3 *)m->regs;

    if (regs->chr_bank_mode == 0) {
        // two 2KB banks at 0x0000-0x0FFF, four 1KB banks at 0x1000-0x1FFF
        if (addr >= 0x0000 && addr < 0x0800) {
            // first 2KB bank
            uint8_t bank = regs->R0;
            uint16_t offset = addr - 0x0000;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_2K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            m->cart->chr_rom[chr_addr] = value;
            return;
        } 
        else if (addr >= 0x0800 && addr < 0x1000) {
            // second 2KB bank
            uint8_t bank = regs->R1;
            uint16_t offset = addr - 0x0800;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_2K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            m->cart->chr_rom[chr_addr] = value;
            return;
        } 
        else if (addr >= 0x1000 && addr < 0x1400) {
            // first 1KB bank
            uint8_t bank = regs->R2;
            uint16_t offset = addr - 0x1000;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            m->cart->chr_rom[chr_addr] = value;
            return;
        } 
        else if (addr >= 0x1400 && addr < 0x1800) {
            // second 1KB bank
            uint8_t bank = regs->R3;
            uint16_t offset = addr - 0x1400;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            m->cart->chr_rom[chr_addr] = value;
            return;
        } 
        else if (addr >= 0x1800 && addr < 0x1C00) {
            // third 1KB bank
            uint8_t bank = regs->R4;
            uint16_t offset = addr - 0x1800;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            m->cart->chr_rom[chr_addr] = value;
            return;
        }
        else if (addr >= 0x1C00 && addr < 0x2000) {
            // fourth 1KB bank
            uint8_t bank = regs->R5;
            uint16_t offset = addr - 0x1C00;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            m->cart->chr_rom[chr_addr] = value;
            return;
        }
    } else {
        // four 1KB banks at 0x0000-0x0FFF, two 2KB banks at 0x1000-0x1FFF
        if (addr >= 0x0000 && addr < 0x0400) {
            // first 1KB bank
            uint8_t bank = regs->R2;
            uint16_t offset = addr - 0x0000;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            m->cart->chr_rom[chr_addr] = value;
            return;
        } 
        else if (addr >= 0x0400 && addr < 0x0800) {
            // second 1KB bank
            uint8_t bank = regs->R3;
            uint16_t offset = addr - 0x0400;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            m->cart->chr_rom[chr_addr] = value;
            return;
        } 
        else if (addr >= 0x0800 && addr < 0x0C00) {
            // third 1KB bank
            uint8_t bank = regs->R4;
            uint16_t offset = addr - 0x0800;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            m->cart->chr_rom[chr_addr] = value;
            return;
        } 
        else if (addr >= 0x0C00 && addr < 0x1000) {
            // fourth 1KB bank
            uint8_t bank = regs->R5;
            uint16_t offset = addr - 0x0C00;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_1K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            m->cart->chr_rom[chr_addr] = value;
            return;
        } 
        else if (addr >= 0x1000 && addr < 0x1800) {
            // first 2KB bank
            uint8_t bank = regs->R1;
            uint16_t offset = addr - 0x1000;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_2K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            m->cart->chr_rom[chr_addr] = value;
            return;
        }
        else if (addr >= 0x1800 && addr < 0x2000) {
            // second 2KB bank
            uint8_t bank = regs->R2;
            uint16_t offset = addr - 0x1800;
            uint32_t chr_addr = (bank * MMC3_CHR_BANK_SIZE_2K) + offset;
            chr_addr = chr_addr % m->cart->chr_size;
            m->cart->chr_rom[chr_addr] = value;
            return;
        }
    }
}

void mapper_mmc3_irq_clock(Mapper *m) {
    regs_mmc3 *regs = (regs_mmc3 *)m->regs; 

    printf("register vals: latch=%02X counter=%02X en=%d\n", regs->irq_latch, regs->irq_counter, regs->irq_en);

    if (regs->irq_en == 1) {
        regs->irq_counter--;
        if (regs->irq_counter < 0) {
            regs->irq_counter = regs->irq_latch;
            m->irq = 1; // trigger IRQ
        }
    }
}