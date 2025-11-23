#include <stdlib.h>
#include <string.h>
#include "../include/log.h"
#include "../include/mapper.h"

// forward declarations of mapper init functions
Mapper *mapper_nrom_init(Cartridge *cart);
Mapper *mapper_mmc1_init(Cartridge *cart);
Mapper *mapper_uxrom_init(Cartridge *cart);
Mapper *mapper_mmc3_init(Cartridge *cart);

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
        // case 1:  // MMC1
        //     mapper = mapper_mmc1_init(cart);
        //     break;
        case 2:  // UxROM
            mapper = mapper_uxrom_init(cart);
            break;
        // case 4:  // MMC3
        //     mapper = mapper_mmc3_init(cart);
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