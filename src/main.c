#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <time.h>
#include "../include/cpu.h"
#include "../include/memory.h"
#include "../include/log.h"
#include "../include/ppu.h"
#include "../include/input.h"
#include "../include/display.h"

#define MEMORY_OUTPUT_FILE "memory.txt"

int cycle();
void load_rom(char *filename, uint8_t *memory, PPU *ppu);
void initialize_display();
void clean_up();
void dump_memory_to_file(char *filename);
void handle_sigint(int sig);

CPU *cpu;
uint8_t *memory;
PPU *ppu;
CNTRL *controller;

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *game_texture;
TTF_Font *font;

time_t init_time;

int debug_enable = 0;
uint16_t breakpoint = 0xFFFF;
int at_break = 0;

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 5) {
        fprintf(stderr, "Usage: %s <rom.nes> [--debug] [--break <address>]\n", argv[0]);
        exit(1);
    }

    char *rom = argv[1];

    // Check for debug flag
    if (argc >= 3 && strcmp(argv[2], "--debug") == 0) {
        debug_enable = 1;
    }

    // Check for breakpoint flag
    if (argc == 5 && strcmp(argv[3], "--break") == 0) {
        if (sscanf(argv[4], "%hx", &breakpoint) != 1) {
            fprintf(stderr, "Invalid address format for --break. Use hex format (e.g., 0x8000).\n");
            exit(1);
        }
    }

    printf("Booting up NES Emulator...\n");

    // get initial time
    time(&init_time);

    // Register signal handler for SIGINT
    signal(SIGINT, handle_sigint);

    // Initialize system memory
    memory = memory_init();

    // Initialize PPU
    ppu = ppu_init();

    // Read in ROM into memory
    load_rom(rom, memory, ppu);

    // Initialize CPU
    cpu = cpu_init(memory);

    // Initialize Display
    initialize_display();

    // Initialize NES Controller
    controller = cntrl_init();

    printf("\nStarting execution of program [%s]\n\n", rom); 

    if (debug_enable) {
        printf("DEBUG MODE enabled\n");
        if (breakpoint) {
            printf("BREAKPOINT SET at address 0x%04X\n", breakpoint);
        }
        printf("\n");
    }

    int running = 1;
    int step = 0; // Step mode (SPACE KEY to enable, P KEY for next instruction)
    if (debug_enable) {
        printf("STEP MODE Enabled [press 'p' for next instruction or 'SPACE' to begin execution]\n");
        step = 1;
    }
    while(running) {
        // Handle input
        SDL_Event event;
        while (SDL_PollEvent(&event)) {       
            // Controller input
            cntrl_handle_input(controller, &event);

            if (event.type == SDL_KEYUP) {
                if (event.key.keysym.sym == SDLK_q) {
                    running = 0; // Quit
                    clean_up();
                    return 0; 
                } else {
                    if (debug_enable) {
                        switch (event.key.keysym.sym) {
                            case SDLK_SPACE:
                                if (step) printf("STEP MODE Disabled\n"); else printf("STEP MODE Enabled [press `p` to run next instruction]\n");
                                step = !step; // Toggle step mode
                                break;
                            case SDLK_p:
                                running = cycle(); // Run next instruction
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
        }

        // Run continously
        if (!step){
            running = cycle();
        }

        // check for breakpoint
        if (debug_enable && cpu->PC == breakpoint && !at_break) {
            printf("BREAKPOINT HIT at 0x%04X\nSTEP MODE Enabled [press `p` to run next instruction]\n", breakpoint);
            step = !step;
            at_break = 1;
        } else if (debug_enable && cpu->PC != breakpoint && at_break) {
            at_break = 0;
        }
    }

    clean_up();
    return 0;
}

int cycle() {
    // Fetch-Decode-Execute next instruction
    uint8_t opcode = memory_read(cpu->PC++, memory, ppu);

    // Execute intstruction
    cpu_execute_opcode(cpu, opcode, memory, ppu);

    // Run PPU (3 * cycles completed by CPU)
    DEBUG_MSG_PPU("Running %i PPU cycles...", 3 * cpu->cycles);
    for (int i = 0; i < 3 * cpu->cycles; i++) {
        int frame_complete = ppu_run_cycle(ppu);
        if (frame_complete) {
            // calculate FPS
            time_t curr_time;
            time(&curr_time);
            ppu->FPS = ppu->frames / (curr_time - init_time);

            // render display
            render_display(renderer, ppu, cpu, game_texture, font);
        }
    }

    // Run APU
    // TODO

    if (debug_enable) {
        cpu_dump_registers(cpu);
        ppu_dump_registers(ppu);
        printf("\n");
    }

    return 1;
}

void load_rom(char *filename, uint8_t *memory, PPU *ppu) {
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
    size_t prg_size = header[4] * 16384;  // PRG ROM (16KB units)
    size_t chr_size = header[5] * 8192;   // CHR ROM (8KB units)

    uint8_t flag6 = header[6];
    uint8_t mapper_low = (flag6 >> 4);
    uint8_t flag7 = header[7];
    uint8_t mapper_high = (flag7 & 0xF0);
    uint8_t mapper_id = mapper_low | mapper_high;

    // Check mapper — for now only NROM (mapper 0) is supported
    if (mapper_id != 0) {
        fclose(rom);
        FATAL_ERROR("ROM Loader", "Unsupported mapper: %d", mapper_id);
    }

    // Check for trainer (512 bytes between header and PRG data)
    if (flag6 & 0x04) {
        fseek(rom, 512, SEEK_CUR); // Skip trainer
    }

    // Load PRG ROM into CPU memory (at 0x8000)
    fread(memory + 0x8000, 1, prg_size, rom);

    // Mirror PRG ROM if it's only 16KB
    if (prg_size == 16384) {
        memcpy(memory + 0xC000, memory + 0x8000, 16384);
    }

    // Load CHR ROM or allocate CHR RAM
    if (chr_size > 0) {
        fread(ppu->vram, 1, chr_size, rom);
    } else {
        // CHR RAM — clear VRAM instead of loading
        memset(ppu->vram, 0, 8192); // Allocate 8KB CHR RAM
    }

    // Set PPU nametable mirroring mode
    ppu->mirroring = (flag6 & 0x01) ? MIRROR_VERTICAL : MIRROR_HORIZONTAL;

    fclose(rom);
    printf("ROM Loaded: %s (%zu KB PRG, %zu KB CHR)\n", filename, prg_size / 1024, chr_size / 1024);
}

void initialize_display(){
    window = window_init();
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (TTF_Init() < 0) {
        printf("TTF Init failed: %s\n", TTF_GetError());
        exit(1);
    }
    font = TTF_OpenFont("fonts/Ubuntu_Mono/UbuntuMono-Regular.ttf", 20);
    if (!font) {
        printf("Failed to load font: %s\n", TTF_GetError());
        exit(1);
    }
    game_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 256, 240);
}

void clean_up() {
    printf("Cleaning up...\n");
    // Save memory contents
    dump_memory_to_file(MEMORY_OUTPUT_FILE); 
    // Free allocated memory
    printf("Freeing memory...\n");
    ppu_free(ppu);
    cpu_free(cpu);
    memory_free(memory);
    cntrl_free(controller);
    printf("DONE\n");
}

void dump_memory_to_file(char *filename) {
    // CPU Memory
    char cpu_filename[256];
    snprintf(cpu_filename, sizeof(cpu_filename), "cpu_%s", filename);

    FILE *mem_file = fopen(cpu_filename, "w");
    if (mem_file) {
        printf("Dumping main memory contents to [%s]...\n", cpu_filename);
        memory_dump(mem_file, memory);
        fclose(mem_file);
    } else {
        fprintf(stderr, "Failed to open memory dump file\n");
    }

    // PPU Memory
    char ppu_filename[256];
    snprintf(ppu_filename, sizeof(ppu_filename), "ppu_%s", filename);

    FILE *ppu_file = fopen(ppu_filename, "w");
    if (ppu_file) {
        printf("Dumping PPU memory contents to [%s]...\n", ppu_filename);
        ppu_memory_dump(ppu_file, ppu);
        fclose(ppu_file);
    } else {
        fprintf(stderr, "Failed to open memory dump file\n");
    }
}

void handle_sigint(int sig) {
    (void) sig;
    printf("\nCaught interrupt [SIGINT]\n");
    clean_up();
    exit(0);
}