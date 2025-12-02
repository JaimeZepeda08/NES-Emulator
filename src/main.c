#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include "../include/nes.h"
#include "../include/cpu.h"
#include "../include/log.h"
#include "../include/ppu.h"
#include "../include/input.h"
#include "../include/display.h"
#include "../include/apu.h"

void clean_up();
void handle_sigint(int sig);

uint32_t last_time;

int display = 0; // pattern table and register display

int debug_enable = 0;
uint16_t breakpoint = 0xFFFF;
int at_break = 0;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom.nes> [<save.nes>] [--display] [--debug] [--break <addr>]\n", argv[0]);
        exit(1);
    }

    char *rom = NULL;
    char *save = NULL;
    int i = 1;

    // parse cli arguments
    while (i < argc) {
        // ROM file or save file
        if (argv[i][0] != '-') {
            if (rom == NULL) {
                rom = argv[i];
            } else if (save == NULL) {
                save = argv[i];
            } else {
                fprintf(stderr, "Error: too many positional arguments provided.\n");
                exit(1);
            }
            i++;
            continue;
        }

        // --display flag for extra debug display
        if (strcmp(argv[i], "--display") == 0) {
            display = 1;
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
    last_time = SDL_GetTicks();

    // Register signal handler for SIGINT
    signal(SIGINT, handle_sigint);

    // Initialize NES
    nes_init(rom, save, display);

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
            cntrl1_handle_input(nes->controller1, &event);
            cntrl2_handle_input(nes->controller2, &event);

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
                                running = nes_cycle(&last_time, debug_enable); // Run next instruction
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
        }

        if (!step) { // run continuously
            int cycles_this_frame = 0;
            uint32_t frame_start = SDL_GetTicks();

            // run enough CPU cycles to simulate 1/60th of a second
            while (cycles_this_frame < CYCLES_PER_FRAME && running) {
                running = nes_cycle(&last_time, debug_enable); 
                cycles_this_frame += nes->cpu->cycles;    // get actual number of cycles run

                // handle infitite loop edge case
                if (nes->cpu->cycles == 0) {
                    cycles_this_frame++;
                }
            }

            uint32_t frame_end = SDL_GetTicks();
            double elapsed_ms = (double)(frame_end - frame_start);
            double target_ms = 1000.0 / 60.0; // 16.6667 ms
            if (elapsed_ms < target_ms) {
                // delay if its running too fast (limit to ~60 FPS)
                SDL_Delay((Uint32)(target_ms - elapsed_ms));
            }
        }

        // check for breakpoint
        if (debug_enable && nes->cpu->PC == breakpoint && !at_break) {
            printf("BREAKPOINT HIT at 0x%04X\nSTEP MODE Enabled [press `p` to run next instruction]\n", breakpoint);
            step = 1;
            at_break = 1;
        } else if (debug_enable && nes->cpu->PC != breakpoint && at_break) {
            at_break = 0;
        }
    }

    clean_up();
    return 0;
}

void clean_up() {
    printf("Cleaning up...\n");
    nes_free();
    printf("DONE\n");
}

void handle_sigint(int sig) {
    (void) sig;
    printf("\nCaught interrupt [SIGINT]\n");
    clean_up();
    exit(0);
}