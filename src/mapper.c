#include <stdlib.h>
#include <string.h>
#include "../include/log.h"
#include "../include/mapper.h"

// forward declarations of mapper init functions
void mapper_nrom_init(Mapper *m);
void mapper_mmc1_init(Mapper *m);
void mapper_uxrom_init(Mapper *m);
void mapper_mmc3_init(Mapper *m);

uint16_t mirror_nametable(Mapper *m, uint16_t address);

Mapper *mapper_init(Cartridge *cart) {
    if (!cart) {
        FATAL_ERROR("Mapper", "Cannot initialize mapper with NULL cartridge");
        return NULL;
    }

    Mapper *mapper = (Mapper *)malloc(sizeof(Mapper));
    if (!mapper) {
        FATAL_ERROR("Mapper", "Failed to allocate mapper");
        return NULL;
    }

    memset(mapper, 0, sizeof(Mapper));

    // set cartridge reference
    mapper->cart = cart;

    // set initial mirroring mode from cartridge
    mapper->mirroring = cart->mirroring;
    
    // set default nametable mirroring function
    mapper->mirror_nametable = mirror_nametable; 

    // initialize mapper specific stuff
    switch (cart->mapper_id) {
        case 0:  // NROM
            mapper_nrom_init(mapper);
            break;
        case 1:  // MMC1
            mapper_mmc1_init(mapper);
            break;
        case 2:  // UxROM
            mapper_uxrom_init(mapper);
            break;
        // case 4:  // MMC3
        //     mapper = mapper_mmc3_init(mapper, cart);
        //     break;
        
        default:
            FATAL_ERROR("Mapper", "Unsupported mapper ID: %d", cart->mapper_id);
            return NULL;
    }

    printf("Mapper %d initialized\n", cart->mapper_id);
    return mapper;
}

void mapper_free(Mapper *mapper) {
    if (mapper) {
        free(mapper);
    }
}

uint16_t mirror_nametable(Mapper *m, uint16_t address) {
    switch (m->mirroring) {
        case MIRROR_VERTICAL: {
            if (address >= 0x2800 && address < 0x2C00) {
                return address - 0x800;
            }
            if (address >= 0x2C00 && address < 0x3000) {
                return address - 0x800;
            }
            break;
        }
        case MIRROR_HORIZONTAL: {
            if (address >= 0x2400 && address < 0x2800) {
                return address - 0x400;
            }
            if (address >= 0x2800 && address < 0x2C00) {
                return address - 0x400;
            }
            if (address >= 0x2C00 && address < 0x3000) {
                return address - 0x800;
            }
            break;
        }
    }
    return address;
}