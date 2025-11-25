#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../include/nes.h"
#include "../include/cartridge.h"
#include "../include/log.h"

void load_rom(Cartridge *cart, const char *filename);
void save_prg_ram_to_file(Cartridge *cart);

Cartridge *cart_init(const char *filename) {
    Cartridge *cart = (Cartridge *)malloc(sizeof(Cartridge));
    if (cart == NULL) {
        fprintf(stderr, " Memory allocation for Cartridge failed!\n");
        exit(1);
    }

    // memory from cartridge
    cart->prg_rom = NULL;
    cart->chr_rom = NULL;
    cart->prg_ram = NULL;

    // memory sizes
    cart->prg_size = 0;
    cart->chr_size = 0;
    cart->prg_ram_size = 0;

    // mapper and mirroring info
    cart->mapper_id = 0;
    cart->mirroring = 0;
    cart->battery = 0;

    // Load ROM data
    load_rom(cart, filename);

    return cart;
}

void cart_free(Cartridge *cart) {
    if (cart) {
        // save PRG RAM to file if battery-backed
        if (cart->battery) {
            save_prg_ram_to_file(cart);
        }
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

    char rom_filename[256];
    snprintf(rom_filename, sizeof(rom_filename), "%s/%s", ROM_FILE_DIR, filename);
    FILE *rom = fopen(rom_filename, "rb");  
    if (!rom) {
        FATAL_ERROR("Memory", "Failed to open ROM file: %s", rom_filename);
    }

    cart->filename = strdup(filename);

    // Read iNES header (16 bytes)
    uint8_t header[16];
    if (fread(header, 1, 16, rom) != 16) {
        fclose(rom);
        FATAL_ERROR("ROM Loader", "Failed to read ROM header");
    }

    // Validate NES file signature ("NES" followed by 0x1A)
    if (!(header[0] == 'N' && header[1] == 'E' && header[2] == 'S' && header[3] == 0x1A)) {
        fclose(rom);
        FATAL_ERROR("ROM Loader", "Invalid ROM format (Missing NES header).");
    }

    // Get ROM size
    cart->prg_size = header[4] * 16384;  // PRG ROM (16KB units)
    cart->chr_size = header[5] * 8192;   // CHR ROM (8KB units)

    // Validate sizes
    if (cart->prg_size == 0) {
        fclose(rom);
        FATAL_ERROR("ROM Loader", "Invalid PRG ROM size");
    }

    uint8_t flag6 = header[6];
    uint8_t mapper_low = (flag6 >> 4);
    uint8_t flag7 = header[7];
    uint8_t mapper_high = (flag7 & 0xF0);
    cart->mapper_id = mapper_low | mapper_high;

    // Set mirroring and battery flags
    cart->mirroring = !(flag6 & 0x01);
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
    if (fread(cart->prg_rom, 1, cart->prg_size, rom) != (size_t)cart->prg_size) {
        free(cart->prg_rom);
        fclose(rom);
        FATAL_ERROR("ROM Loader", "Failed to read PRG ROM data");
    }

    // Allocate and load CHR ROM or CHR RAM
    if (cart->chr_size > 0) {
        cart->chr_rom = (uint8_t *)malloc(cart->chr_size);
        if (!cart->chr_rom) {
            fclose(rom);
            free(cart->prg_rom);
            FATAL_ERROR("ROM Loader", "Failed to allocate CHR ROM memory");
        }
        if (fread(cart->chr_rom, 1, cart->chr_size, rom) != (size_t)cart->chr_size) {
            free(cart->chr_rom);
            free(cart->prg_rom);
            fclose(rom);
            FATAL_ERROR("ROM Loader", "Failed to read CHR ROM data");
        }
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

    // PRG RAM size
    uint8_t flag8 = header[8];
    if (flag8 > 0) {
        cart->prg_ram_size = flag8 * 8192;  // 8KB units
    } else {
        cart->prg_ram_size = 8192;  // Default to 8KB if not specified
    }

    // allocate PRG RAM even if it is not battery-backed because some games still use ram
    // these games give user a password to continue instead of saving state
    cart->prg_ram = (uint8_t *)calloc(1, cart->prg_ram_size);
    if (!cart->prg_ram) {
        fclose(rom);
        free(cart->prg_rom);
        free(cart->chr_rom);
        FATAL_ERROR("ROM Loader", "Failed to allocate PRG RAM memory");
    }

    if (cart->battery) {        
        // load from save file if it exists
        char save_filename[256];
        snprintf(save_filename, sizeof(save_filename), "%s/%s.%s", SAVE_FILE_DIR, cart->filename, SAVE_RAM_FILE_EXT);
        FILE *save_file = fopen(save_filename, "rb");
        if (save_file) {
            fread(cart->prg_ram, 1, cart->prg_ram_size, save_file);
            fclose(save_file);
            printf("Loaded PRG RAM from save file: %s/%s.%s\n", SAVE_FILE_DIR, cart->filename, SAVE_RAM_FILE_EXT);
        }
        else {
            printf("No save file found for PRG RAM: %s/%s.%s\n", SAVE_FILE_DIR, cart->filename, SAVE_RAM_FILE_EXT);
        }
    }

    fclose(rom);
    printf("ROM Loaded: %s (%d KB PRG, %d KB CHR, %d KB PRG RAM)\n", 
           filename, cart->prg_size / 1024, cart->chr_size / 1024, cart->prg_ram_size / 1024);
}

void save_prg_ram_to_file(Cartridge *cart) {
    if (cart->battery && cart->prg_ram) {
        // create directory if it doesn't exist
        #ifdef _WIN32
            _mkdir(SAVE_FILE_DIR);
        #else
            mkdir(SAVE_FILE_DIR, 0755);
        #endif

        // save to file
        char save_filename[256];
        snprintf(save_filename, sizeof(save_filename), "%s/%s.%s", SAVE_FILE_DIR, cart->filename, SAVE_RAM_FILE_EXT);
        FILE *save_file = fopen(save_filename, "wb");
        if (save_file) {
            fwrite(cart->prg_ram, 1, cart->prg_ram_size, save_file);
            fclose(save_file);
            printf("Saved PRG RAM to save file: %s/%s.%s\n", SAVE_FILE_DIR, cart->filename, SAVE_RAM_FILE_EXT);
        } else {
            printf("Failed to open save file for writing: %s/%s.%s\n", SAVE_FILE_DIR, cart->filename, SAVE_RAM_FILE_EXT);
        }
    }
}