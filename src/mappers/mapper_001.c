#include <stdlib.h>
#include <string.h>
#include "../../include/mapper.h"
#include "../../include/log.h"

uint8_t mapper_mmc1_cpu_read(Mapper *m, uint16_t addr);
void mapper_mmc1_cpu_write(Mapper *m, uint16_t addr, uint8_t value);
uint8_t mapper_mmc1_ppu_read(Mapper *m, uint16_t addr);
void mapper_mmc1_ppu_write(Mapper *m, uint16_t addr, uint8_t value);

Mapper *mapper_mmc1_init(Cartridge *cart) {
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
    mapper->cpu_read = mapper_mmc1_cpu_read;
    mapper->cpu_write = mapper_mmc1_cpu_write;
    mapper->ppu_read = mapper_mmc1_ppu_read;
    mapper->ppu_write = mapper_mmc1_ppu_write;

    memset(mapper->regs, 0, sizeof(mapper->regs));

    return mapper;
}

uint8_t mapper_mmc1_cpu_read(Mapper *m, uint16_t addr) {
    
}

void mapper_mmc1_cpu_write(Mapper *m, uint16_t addr, uint8_t value) {
    
}

uint8_t mapper_mmc1_ppu_read(Mapper *m, uint16_t addr) {    
    
}

void mapper_mmc1_ppu_write(Mapper *m, uint16_t addr, uint8_t value) {    
    
}