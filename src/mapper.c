#include <stdlib.h>
#include <string.h>
#include "../include/mapper.h"
#include "../include/log.h"

Mapper *mapper_nrom_init(Cartridge *cart);

Mapper *mapper_init(Cartridge *cart) {
    if (!cart) {
        FATAL_ERROR("Mapper", "Cannot initialize mapper with NULL cartridge");
        return NULL;
    }

    Mapper *mapper = NULL;

    switch (cart->mapper_id) {
        case 0:  // NROM
            mapper = mapper_nrom_init(cart);
            break;

        // TODO: Implement other mappers
        // case 1:  // MMC1
        //     mapper = mapper_mmc1_init(cart);
        //     break;
        // case 2:  // UxROM
        //     mapper = mapper_uxrom_init(cart);
        //     break;
        
        default:
            FATAL_ERROR("Mapper", "Unsupported mapper ID: %d", cart->mapper_id);
            return NULL;
    }

    if (!mapper) {
        FATAL_ERROR("Mapper", "Failed to initialize mapper %d", cart->mapper_id);
    }

    printf("Mapper %d initialized\n", cart->mapper_id);
    return mapper;
}

void mapper_free(Mapper *mapper) {
    if (mapper) {
        free(mapper);
    }
}

uint8_t mapper_nrom_cpu_read(Mapper *m, uint16_t addr) {
    Cartridge *cart = m->cart;
    
    // PRG ROM: 0x8000-0xFFFF
    if (addr >= 0x8000) {
        uint16_t prg_addr = addr - 0x8000;
        
        // If PRG ROM is 16KB, mirror it to fill 32KB space
        if (cart->prg_size == 16384) {
            prg_addr = prg_addr % 16384;
        }
        
        return cart->prg_rom[prg_addr];
    }
    
    return 0; // Open bus behavior
}

// NROM CPU Write (PRG ROM is read-only for NROM)
void mapper_nrom_cpu_write(Mapper *m, uint16_t addr, uint8_t value) {
    (void)m;
    (void)addr;
    (void)value;
    // NROM has no writable registers
    // Writes to 0x8000-0xFFFF are ignored
}

// NROM PPU Read
uint8_t mapper_nrom_ppu_read(Mapper *m, uint16_t addr) {
    Cartridge *cart = m->cart;
    
    // CHR ROM/RAM: 0x0000-0x1FFF
    if (addr < 0x2000) {
        return cart->chr_rom[addr];
    }
    
    return 0; // Out of range
}

// NROM PPU Write
void mapper_nrom_ppu_write(Mapper *m, uint16_t addr, uint8_t value) {
    Cartridge *cart = m->cart;
    
    // CHR RAM: 0x0000-0x1FFF (only if CHR ROM size was 0, meaning CHR RAM)
    if (addr < 0x2000 && cart->chr_size == 8192) {
        // Check if this is CHR RAM (not ROM) by seeing if original size was 0
        // For simplicity, allow writes to CHR memory
        cart->chr_rom[addr] = value;
    }
}

// NROM Mapper Initialization
Mapper *mapper_nrom_init(Cartridge *cart) {
    Mapper *mapper = (Mapper *)malloc(sizeof(Mapper));
    if (!mapper) {
        FATAL_ERROR("Mapper", "Failed to allocate NROM mapper");
        return NULL;
    }

    // Clear mapper state
    memset(mapper, 0, sizeof(Mapper));

    // Set cartridge reference
    mapper->cart = cart;

    // Set function pointers
    mapper->cpu_read = mapper_nrom_cpu_read;
    mapper->cpu_write = mapper_nrom_cpu_write;
    mapper->ppu_read = mapper_nrom_ppu_read;
    mapper->ppu_write = mapper_nrom_ppu_write;

    // NROM has no special registers
    memset(mapper->regs, 0, sizeof(mapper->regs));

    return mapper;
}