#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h> 

// namtable mirroring types
#define MIRROR_VERTICAL     0
#define MIRROR_HORIZONTAL   1

typedef struct Cartridge {
    // Cartridge memory (dynamically allocated because sizes vary)
    uint8_t *prg_rom;
    uint8_t *chr_rom;  // could be ROM or RAM
    int prg_size;
    int chr_size;
    int mapper_id;
    int mirroring;
    int battery;
} Cartridge;

Cartridge *cart_init(const char *filename);
void cart_free(Cartridge *cart);

#endif