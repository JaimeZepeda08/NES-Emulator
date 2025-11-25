#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include <stdint.h> 

typedef struct Cartridge {
    char *rom_filename;  // ROM file path
    char *save_filename; // save file path (can be NULL)
    // Cartridge memory (dynamically allocated because sizes vary)
    uint8_t *prg_rom;
    uint8_t *chr_rom;  // could be ROM or RAM
    uint8_t *prg_ram; // battery-backed RAM (if any)
    int prg_size;
    int chr_size;
    int prg_ram_size;
    int mapper_id;
    int mirroring; // initial mirroring mode set in header (can be changed by mapper)
    int battery;
} Cartridge;

Cartridge *cart_init(const char *rom_filename, const char *save_filename);
void cart_free(Cartridge *cart);

#endif