#include <SDL.h>
#include <SDL_ttf.h>
#include "../include/nes.h"
#include "../include/display.h"
#include "../include/log.h"
#include "../include/ppu.h"
#include "../include/cpu.h"
#include "../include/input.h"

DISPLAY *window_init(int debug_enable) {
    printf("Initializing NES Display...");

    DISPLAY *display = (DISPLAY *)malloc(sizeof(DISPLAY));
    if (!display) {
        FATAL_ERROR("DISP", "Memory allocation for DISPLAY struct failed");
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        printf("\tFAILED\n");
        exit(1);
    }

    display->debug_enable = debug_enable;

    int width = WINDOW_WIDTH;
    int height = WINDOW_HEIGHT; 

    if (!display->debug_enable) {
        width = GAME_WIDTH;
        height = GAME_HEIGHT;
    }

    display->window = SDL_CreateWindow("NES Emulator", 
                                            SDL_WINDOWPOS_CENTERED, 
                                            SDL_WINDOWPOS_CENTERED, 
                                            width, 
                                            height, 
                                            SDL_WINDOW_SHOWN);
    if (!display->window) {
        FATAL_ERROR("WIN", "Error initializing window");
    }

    display->renderer = SDL_CreateRenderer(display->window, -1, SDL_RENDERER_ACCELERATED);
    if (TTF_Init() < 0) {
        printf("TTF Init failed: %s\n", TTF_GetError());
        exit(1);
    }
    display->font = TTF_OpenFont("fonts/Ubuntu_Mono/UbuntuMono-Regular.ttf", 20);
    if (!display->font) {
        printf("Failed to load font: %s\n", TTF_GetError());
        exit(1);
    }
    display->game_texture = SDL_CreateTexture(display->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 256, 240);

    printf("\tDONE\n");
    return display;
}

void free_display(DISPLAY *display) {
    if (display) {
        if (display->font) {
            TTF_CloseFont(display->font);
        }
        if (display->game_texture) {
            SDL_DestroyTexture(display->game_texture);
        }
        if (display->renderer) {
            SDL_DestroyRenderer(display->renderer);
        }
        if (display->window) {
            SDL_DestroyWindow(display->window);
        }
        TTF_Quit();
        SDL_Quit();
        free(display);
    }
}

void render_display(DISPLAY *display) {
    // Safety check: ensure NES is initialized
    if (!nes || !nes->ppu || !nes->cpu || !nes->mapper) {
        return;
    }

    // clear the screen once before rendering
    SDL_SetRenderDrawColor(display->renderer, 0, 0, 0, 255); 
    SDL_RenderClear(display->renderer);

    int x_offset = 0;
    
    // If debug mode is enabled, draw nametables first (on the left)
    if (display->debug_enable) {
        x_offset = NT_DISPLAY_WIDTH;
        
        // ======================= Nametables =======================
        static SDL_Texture *nt_texture = NULL;
        static int frame_counter = 0;
        
        // Create texture if it doesn't exist
        if (!nt_texture) {
            nt_texture = SDL_CreateTexture(display->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, NT_WIDTH, NT_HEIGHT * 2);
            if (!nt_texture) {
                goto skip_nametables;
            }
        }
        
        // Only update nametables every 5 frames to improve performance
        if (frame_counter % 5 == 0) {
            // Render the 2 physical nametables (2KB VRAM)
            SDL_SetRenderTarget(display->renderer, nt_texture);
            SDL_SetRenderDrawColor(display->renderer, 0, 0, 0, 255);
            SDL_RenderClear(display->renderer);
            
            // Render each of the 2 nametables (actual VRAM at 0x2000-0x23FF and 0x2400-0x27FF)
            for (int nt = 0; nt < 2; nt++) {
                uint16_t nt_base = 0x2000 + (nt * 0x0400); // 0x2000, 0x2400
                int y_nt_offset = nt * NT_HEIGHT;
                
                // Iterate through all 30 rows and 32 columns of tiles
                for (int ty = 0; ty < 30; ty++) {
                    for (int tx = 0; tx < 32; tx++) {
                        // Read tile index from nametable
                        uint16_t tile_addr = nt_base + ty * 32 + tx;
                        uint8_t tile_id = nes_ppu_read(tile_addr);
                        
                        // Get pattern table base (from PPUCTRL bit 4)
                        uint16_t pattern_base = (nes->ppu->PPUCTRL & PPUCNTRL_B) ? 0x1000 : 0x0000;
                        uint16_t tile_pattern_addr = pattern_base + tile_id * 16;
                        
                        // Render the 8x8 tile
                        for (int py = 0; py < 8; py++) {
                            uint8_t plane0 = nes_ppu_read(tile_pattern_addr + py);
                            uint8_t plane1 = nes_ppu_read(tile_pattern_addr + py + 8);
                            
                            for (int px = 0; px < 8; px++) {
                                uint8_t bit0 = (plane0 >> (7 - px)) & 1;
                                uint8_t bit1 = (plane1 >> (7 - px)) & 1;
                                uint8_t pixel = (bit1 << 1) | bit0;
                                
                                // Use grayscale based on pixel value (0-3)
                                uint8_t shade = 85 * pixel;  // 0, 85, 170, 255
                                SDL_SetRenderDrawColor(display->renderer, shade, shade, shade, 255);
                                
                                SDL_Rect pixel_rect = {
                                    .x = tx * 8 + px,
                                    .y = y_nt_offset + ty * 8 + py,
                                    .w = 1,
                                    .h = 1
                                };
                                SDL_RenderFillRect(display->renderer, &pixel_rect);
                            }
                        }
                    }
                }
            }
            
            SDL_SetRenderTarget(display->renderer, NULL);
        }
        
        frame_counter++;
        
        // Copy nametables to screen (always display, even if not updated this frame)
        SDL_Rect nt_dest = {
            .x = 0,
            .y = 0,
            .w = NT_DISPLAY_WIDTH,
            .h = NT_DISPLAY_HEIGHT
        };
        SDL_RenderCopy(display->renderer, nt_texture, NULL, &nt_dest);
        
        skip_nametables:;
        // ======================= Nametables =======================
    }
    
    // ======================= Game Window =======================
    // Crop 8 pixels from each side of the frame buffer (lazy way to remove edge artifacts)
    SDL_Rect crop_rect = {
        .x = 8,
        .y = 8,
        .w = NES_WIDTH - 16,  // 256 - 16 = 240
        .h = NES_HEIGHT - 16  // 240 - 16 = 224
    };
    
    SDL_UpdateTexture(display->game_texture, NULL, nes->ppu->frame_buffer, NES_WIDTH * sizeof(uint32_t)); 
    SDL_Rect game_rect = {x_offset, 0, GAME_WIDTH, GAME_HEIGHT};
    SDL_RenderCopy(display->renderer, display->game_texture, &crop_rect, &game_rect);  
    // ======================= Game Window =======================

    // If pattern table display not enabled, present and return
    if (!display->debug_enable) {
        SDL_RenderPresent(display->renderer);
        return;
    }

    // ======================= Pattern Tables =======================
    static SDL_Texture *pt_texture = NULL;
    
    // create texture if it doesn't exist
    if (!pt_texture) {
        pt_texture = SDL_CreateTexture(display->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, PT_WIDTH, PT_HEIGHT);
        if (!pt_texture) {
            SDL_RenderPresent(display->renderer);
            return;
        }
    }

    // re-render the pattern table every frame 
    SDL_SetRenderTarget(display->renderer, pt_texture);
    SDL_SetRenderDrawColor(display->renderer, 0, 0, 0, 255);
    SDL_RenderClear(display->renderer);

    for (int table = 0; table < 2; table++) {
        uint16_t base = table * 0x1000;

        for (int tile_index = 0; tile_index < 256; tile_index++) {
            int tx = tile_index % 16;
            int ty = tile_index / 16;

            uint16_t addr = base + tile_index * 16;
            for (int row = 0; row < 8; row++) {
                uint8_t plane0 = nes->mapper->ppu_read(nes->mapper, (addr + row) & 0x1FFF);
                uint8_t plane1 = nes->mapper->ppu_read(nes->mapper, (addr + row + 8) & 0x1FFF);

                for (int col = 0; col < 8; col++) {
                    uint8_t bit0 = (plane0 >> (7 - col)) & 1;
                    uint8_t bit1 = (plane1 >> (7 - col)) & 1;
                    uint8_t pixel = (bit1 << 1) | bit0;

                    uint8_t shade = 85 * pixel;
                    SDL_SetRenderDrawColor(display->renderer, shade, shade, shade, 255);

                    SDL_Rect pixel_rect = {
                        .x = (tx * 8 + col),
                        .y = ((ty + table * 16) * 8 + row),
                        .w = 1,
                        .h = 1
                    };
                    SDL_RenderFillRect(display->renderer, &pixel_rect);
                }
            }
        }
    }

    SDL_SetRenderTarget(display->renderer, NULL);

    // Copy pattern table to screen (to the right of game window)
    SDL_Rect pt_dest = {
        .x = x_offset + GAME_WIDTH,
        .y = 0,
        .w = (int)(PT_WIDTH * SCALE_FACTOR),
        .h = (int)(PT_HEIGHT * SCALE_FACTOR)
    };
    SDL_RenderCopy(display->renderer, pt_texture, NULL, &pt_dest);

    // ======================= Pattern Tables =======================

    SDL_Rect debug_rect = { 0, GAME_HEIGHT, WINDOW_WIDTH, DEBUG_HEIGHT };
    SDL_SetRenderDrawColor(display->renderer, 50, 60, 80, 255);
    SDL_RenderFillRect(display->renderer, &debug_rect);

    // ======================= Debug Info =======================
    char reg_text[512];
    snprintf(reg_text, sizeof(reg_text),
             "PC: $%04X   A: $%02X   X: $%02X   Y: $%02X   SP: $%02X   P: %c%c-%c%c%c%c%c%c    FPS: %02i\n"
             "PPUCTRL: $%02X   PPUMASK: $%02X   PPUSTATUS: $%02X   OAMADDR: $%02X\n"
             "OAMDATA: $%02X   PPUSCROLL: $%02X   PPUADDR: $%02X   PPUDATA: $%02X",
             nes->cpu->PC, nes->cpu->A, nes->cpu->X, nes->cpu->Y, nes->cpu->S,
             (nes->cpu->P & FLAG_NEGATIVE) ? 'N' : 'n',
             (nes->cpu->P & FLAG_OVERFLOW) ? 'V' : 'v',
             (nes->cpu->P & FLAG_UNUSED) ? 'U' : 'u',
             (nes->cpu->P & FLAG_BREAK) ? 'B' : 'b',
             (nes->cpu->P & FLAG_DECIMAL) ? 'D' : 'd',
             (nes->cpu->P & FLAG_INT) ? 'I' : 'i',
             (nes->cpu->P & FLAG_ZERO) ? 'Z' : 'z',
             (nes->cpu->P & FLAG_CARRY) ? 'C' : 'c',
             nes->ppu->FPS,
             nes->ppu->PPUCTRL, nes->ppu->PPUMASK, nes->ppu->PPUSTATUS, nes->ppu->OAMADDR,
             nes->ppu->OAMDATA, nes->ppu->PPUSCROLL, nes->ppu->PPUADDR, nes->ppu->PPUDATA);

    // render each line of debug info
    char *line = strtok(reg_text, "\n");
    if (!display->font) {
        fprintf(stderr, "Font not loaded!\n");
        SDL_RenderPresent(display->renderer);
        return;
    }
    SDL_Color white = {255, 255, 255, 255};

    // consistent line height and y offset
    int line_height = FONT_SIZE + (int)(4 * SCALE_FACTOR);
    int y_offset = GAME_HEIGHT + (int)(4 * SCALE_FACTOR); 

    while (line != NULL) {
        SDL_Surface *txt_surf = TTF_RenderText_Solid(display->font, line, white);
        if (!txt_surf) {
            fprintf(stderr, "TTF_RenderText_Solid failed: %s\n", TTF_GetError());
            break;
        }
        SDL_Texture *txt_tex = SDL_CreateTextureFromSurface(display->renderer, txt_surf);

        // center text horizontally in total window width
        SDL_Rect txt_dst = {
            .x = (WINDOW_WIDTH - txt_surf->w) / 2,
            .y = y_offset,
            .w = txt_surf->w,
            .h = txt_surf->h
        };
        SDL_RenderCopy(display->renderer, txt_tex, NULL, &txt_dst);

        SDL_FreeSurface(txt_surf);
        SDL_DestroyTexture(txt_tex);

        line = strtok(NULL, "\n");
        y_offset += line_height;
    }
    // ======================= Debug Info =======================

    // Render
    SDL_RenderPresent(display->renderer);
}
