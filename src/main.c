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
FILE *log_file = NULL;

int cycle();
void load_rom(char *filename, MEM *memory, PPU *ppu);
void initialize_display();
void clean_up();
void dump_memory_to_file(char *filename);
void handle_sigint(int sig);

CPU *cpu;
MEM *memory;
PPU *ppu;
CNTRL *controller;

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *game_texture;
TTF_Font *font;

clock_t last_time;

int pt_enable = 0; // pattern table and register display

int debug_enable = 0;
uint16_t breakpoint = 0xFFFF;
int at_break = 0;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom.nes> [--pt] [--debug] [--break <addr>]\n", argv[0]);
        exit(1);
    }

    char *rom = NULL;
    int i = 1;

    // parse cli arguments
    while (i < argc) {
        // ROM file
        if (argv[i][0] != '-') {
            if (rom != NULL) {
                fprintf(stderr, "Error: multiple ROM files provided.\n");
                exit(1);
            }
            rom = argv[i];
            i++;
            continue;
        }

        // --pt flag for pattern table display
        if (strcmp(argv[i], "--pt") == 0) {
            pt_enable = 1;
            i++;
            continue;
        }

        // --debug flag
        if (strcmp(argv[i], "--debug") == 0) {
            debug_enable = 1;
            i++;
            continue;
        }

        // --break <address>
        if (strcmp(argv[i], "--break") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --break requires a hex address.\n");
                exit(1);
            }

            if (sscanf(argv[i + 1], "%hx", &breakpoint) != 1) {
                fprintf(stderr, "Invalid hex address for --break (example: 8000 or 0x8000).\n");
                exit(1);
            }

            i += 2;
            continue;
        }

        // Unknown flag
        fprintf(stderr, "Unknown option: %s\n", argv[i]);
        exit(1);
    }

    // Must have at least one ROM file
    if (rom == NULL) {
        fprintf(stderr, "Error: No ROM file specified.\n");
        exit(1);
    }

    printf("Booting up NES Emulator...\n");

    // get initial time
    last_time = clock();

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

        log_file = fopen("roms/tests/nestest_output.log", "w");
        if (!log_file) {
            perror("Failed to open log file");
            exit(1);
        }
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

        int cycles_to_run = 3;
        int cycles_executed = 0;
        // Run continuously
        if (!step){
            while (cycles_executed < cycles_to_run && running) {
                running = cycle();
                cycles_executed++;
            }
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
    // log cpu to file if debug
    if (debug_enable && log_file) {
        fprintf(log_file, "%04X A:%02X X:%02X Y:%02X P:%02X S:%02X\n",
                cpu->PC,
                cpu->A,
                cpu->X,
                cpu->Y,
                cpu->P,
                cpu->S);
    }

    // run cpu cycle
    cpu_run_cycle(cpu, memory, ppu);

    // Run PPU (3 * cycles completed by CPU)
    DEBUG_MSG_PPU("Running %i PPU cycles...", 3 * cpu->cycles);
    for (int i = 0; i < 3 * cpu->cycles; i++) {
        int frame_complete = ppu_run_cycle(ppu);
        if (frame_complete) {
            // calculate FPS
            clock_t curr_time = clock();
            if (ppu->frames > 10) {
                double elapsed = (double)(curr_time - last_time) / CLOCKS_PER_SEC;
                ppu->FPS = ppu->frames / elapsed;
                ppu->frames = 0;
                last_time = clock();
            }

            // render display
            render_display(renderer, ppu, cpu, game_texture, font, pt_enable);
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

void load_rom(char *filename, MEM *memory, PPU *ppu) {
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

    // Load PRG ROM into CPU memory 
    fread(memory->prg_rom, 1, prg_size, rom);

    // Mirror PRG ROM if it's only 16KB
    if (prg_size == 16384) {
        memcpy(memory->prg_rom + 0x4000, memory->prg_rom, 16384);
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
    window = window_init(pt_enable);
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

    if (log_file) {
        fclose(log_file);
    }
    
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