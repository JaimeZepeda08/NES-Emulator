#ifndef MAPPER_H
#define MAPPER_H

#include <stdint.h>
#include "cartridge.h"

typedef struct Mapper {
    // Reference to the cartridge this mapper is associated with
    Cartridge *cart;

    // Function pointers (defined by individual mappers)
    uint8_t (*cpu_read)(struct Mapper *m, uint16_t addr);
    void    (*cpu_write)(struct Mapper *m, uint16_t addr, uint8_t value);

    uint8_t (*ppu_read)(struct Mapper *m, uint16_t addr);
    void    (*ppu_write)(struct Mapper *m, uint16_t addr, uint8_t value);

    // Mapper state (bank registers, counters, IRQs)
    uint32_t regs[16];

} Mapper;

Mapper *mapper_init(Cartridge *cart);
void mapper_free(Mapper *mapper);

#endif
