#ifndef MAPPER_H
#define MAPPER_H

#include <stdint.h>
#include "cartridge.h"

// default nametable mirroring types
#define MIRROR_VERTICAL     0
#define MIRROR_HORIZONTAL   1
#define MIRROR_SINGLE_LOWER 2
#define MIRROR_SINGLE_UPPER 3

typedef struct Mapper {
    // reference to the cartridge this mapper is associated with
    Cartridge *cart;

    // function pointers (defined by individual mappers)
    uint8_t (*cpu_read)(struct Mapper *m, uint16_t addr);
    void    (*cpu_write)(struct Mapper *m, uint16_t addr, uint8_t value);

    uint8_t (*ppu_read)(struct Mapper *m, uint16_t addr);
    void    (*ppu_write)(struct Mapper *m, uint16_t addr, uint8_t value);

    // pointer to mapper register struct (each mapper uses its own set of registers)
    void *regs;

    // current mirroring mode and function (can be changed by mapper)
    int mirroring; 
    uint16_t (*mirror_nametable)(struct Mapper *m, uint16_t address); // mapper specific nametable mirroring function

} Mapper;

Mapper *mapper_init(Cartridge *cart);
void mapper_free(Mapper *mapper);

#endif
