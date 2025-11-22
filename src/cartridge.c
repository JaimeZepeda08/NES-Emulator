#include <stdio.h>
#include <stdlib.h>
#include "../include/nes.h"
#include "../include/cartridge.h"
#include "../include/log.h"

void load_rom(Cartridge *cart, const char *filename);

Cartridge *cart_init(const char *filename) {
    Cartridge *cart = (Cartridge *)malloc(sizeof(Cartridge));
    if (cart == NULL) {
        fprintf(stderr, " Memory allocation for Cartridge failed!\n");
        exit(1);
    }

    // Initialize to NULL
    cart->prg_rom = NULL;
    cart->chr_rom = NULL;
    cart->prg_size = 0;
    cart->chr_size = 0;
    cart->mapper_id = 0;
    cart->mirroring = 0;
    cart->battery = 0;

    // Load ROM data
    load_rom(cart, filename);

    return cart;
}

void cart_free(Cartridge *cart) {
    if (cart) {
        if (cart->prg_rom) {
            free(cart->prg_rom);
        }
        if (cart->chr_rom) {
            free(cart->chr_rom);
        }
        free(cart);
    }
}

void load_rom(Cartridge *cart, const char *filename) {
    //////////////////////////////////////////////////////
    //                   iNES Format                    //
    //==================================================//
    //  0-3         |   ASCII "NES" followed by EOF     //
    //  4           |   Size of PRG ROM in 16KB units   //
    //  5           |   Size of CHR ROM in 8KB units    //
    //  6 (Flags)   |   Mapper, mirroring, battery      //
    //  7 (Flags)   |   Mapper, VS/Playchoice NES 2.0   //
    //  8 (Flags)   |   PRG RAM size (rarely used)      //
    //  9 (Flags)   |   TV system (rarely used)         //
    //  10 (Flags)  |   TV system (rarely used)         //
    //  11-15       |   Unused padding                  //
    //////////////////////////////////////////////////////

    FILE *rom = fopen(filename, "rb");  // Open file in binary mode
    if (!rom) {
        FATAL_ERROR("Memory", "Failed to open ROM file: %s", filename);
    }

    // Read iNES header (16 bytes)
    uint8_t header[16];
    fread(header, 1, 16, rom);

    // Validate NES file signature ("NES" followed by 0x1A)
    if (!(header[0] == 'N' && header[1] == 'E' && header[2] == 'S' && header[3] == 0x1A)) {
        fclose(rom);
        FATAL_ERROR("ROM Loader", "Invalid ROM format (Missing NES header).");
    }

    // Get ROM size
    cart->prg_size = header[4] * 16384;  // PRG ROM (16KB units)
    cart->chr_size = header[5] * 8192;   // CHR ROM (8KB units)

    uint8_t flag6 = header[6];
    uint8_t mapper_low = (flag6 >> 4);
    uint8_t flag7 = header[7];
    uint8_t mapper_high = (flag7 & 0xF0);
    cart->mapper_id = mapper_low | mapper_high;

    // Check mapper — for now only NROM (mapper 0) is supported
    if (cart->mapper_id != 0) {
        fclose(rom);
        FATAL_ERROR("ROM Loader", "Unsupported mapper: %d", cart->mapper_id);
    }

    // Set mirroring and battery flags
    cart->mirroring = (flag6 & 0x01) ? MIRROR_VERTICAL : MIRROR_HORIZONTAL;
    cart->battery = (flag6 & 0x02) ? 1 : 0;

    // Check for trainer (512 bytes between header and PRG data)
    if (flag6 & 0x04) {
        fseek(rom, 512, SEEK_CUR); // Skip trainer
    }

    // Allocate PRG ROM
    cart->prg_rom = (uint8_t *)malloc(cart->prg_size);
    if (!cart->prg_rom) {
        fclose(rom);
        FATAL_ERROR("ROM Loader", "Failed to allocate PRG ROM memory");
    }

    // Load PRG ROM
    fread(cart->prg_rom, 1, cart->prg_size, rom);

    // Allocate and load CHR ROM or CHR RAM
    if (cart->chr_size > 0) {
        cart->chr_rom = (uint8_t *)malloc(cart->chr_size);
        if (!cart->chr_rom) {
            fclose(rom);
            free(cart->prg_rom);
            FATAL_ERROR("ROM Loader", "Failed to allocate CHR ROM memory");
        }
        fread(cart->chr_rom, 1, cart->chr_size, rom);
    } else {
        // CHR RAM — allocate 8KB
        cart->chr_size = 8192;
        cart->chr_rom = (uint8_t *)calloc(1, cart->chr_size);
        if (!cart->chr_rom) {
            fclose(rom);
            free(cart->prg_rom);
            FATAL_ERROR("ROM Loader", "Failed to allocate CHR RAM memory");
        }
    }

    fclose(rom);
    printf("ROM Loaded: %s (%d KB PRG, %d KB CHR)\n", 
           filename, cart->prg_size / 1024, cart->chr_size / 1024);
}