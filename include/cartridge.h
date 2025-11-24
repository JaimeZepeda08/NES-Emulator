#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#define ROM_FILE_DIR        "roms"
#define SAVE_FILE_DIR       "saves"
#define SAVE_RAM_FILE_EXT   "ram"

#include <stdint.h> 

typedef struct Cartridge {
    char *filename; // store file name for save RAM purposes
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

Cartridge *cart_init(const char *filename);
void cart_free(Cartridge *cart);

#endif