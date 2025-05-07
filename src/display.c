#include <SDL.h>
#include <SDL_ttf.h>
#include "../include/display.h"
#include "../include/log.h"
#include "../include/ppu.h"
#include "../include/cpu.h"
#include "../include/input.h"

SDL_Window *window_init() {
    printf("Initializing NES Display...");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        printf("\tFAILED\n");
        exit(1);
    }

    SDL_Window *window = SDL_CreateWindow("NES Emulator", 
                                            SDL_WINDOWPOS_CENTERED, 
                                            SDL_WINDOWPOS_CENTERED, 
                                            WINDOW_WIDTH, 
                                            WINDOW_HEIGHT, 
                                            SDL_WINDOW_SHOWN);
    if (!window) {
        FATAL_ERROR("WIN", "Error initializing window");
    }

    printf("\tDONE\n");
    return window;
}

void render_display(SDL_Renderer *renderer, PPU *ppu, CPU *cpu, SDL_Texture *game_texture, TTF_Font *font) {
    // --- Clear the screen once before rendering ---
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); 
    SDL_RenderClear(renderer);

    // ======================= Game Window =======================
    SDL_UpdateTexture(game_texture, NULL, ppu->frame_buffer, NES_WIDTH * sizeof(uint32_t)); 
    SDL_Rect game_rect = {0, 0, GAME_WIDTH, GAME_HEIGHT};
    SDL_RenderCopy(renderer, game_texture, NULL, &game_rect);  
    // ======================= Game Window =======================

    // ======================= Pattern Tables =======================
    static SDL_Texture *pt_texture = NULL;
    if (!pt_texture) {
        pt_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, PT_WIDTH, PT_HEIGHT);
        SDL_SetRenderTarget(renderer, pt_texture);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        for (int table = 0; table < 2; table++) {
            uint16_t base = table * 0x1000;

            for (int tile_index = 0; tile_index < NES_WIDTH; tile_index++) {
                int tx = tile_index % 16;
                int ty = tile_index / 16;

                uint16_t addr = base + tile_index * 16;
                for (int row = 0; row < 8; row++) {
                    uint8_t plane0 = ppu->vram[addr + row];
                    uint8_t plane1 = ppu->vram[addr + row + 8];

                    for (int col = 0; col < 8; col++) {
                        uint8_t bit0 = (plane0 >> (7 - col)) & 1;
                        uint8_t bit1 = (plane1 >> (7 - col)) & 1;
                        uint8_t pixel = (bit1 << 1) | bit0;

                        uint8_t shade = 85 * pixel;
                        SDL_SetRenderDrawColor(renderer, shade, shade, shade, 255);

                        SDL_Rect pixel_rect = {
                            .x = (tx * 8 + col),
                            .y = ((ty + table * 16) * 8 + row),
                            .w = 1,
                            .h = 1
                        };
                        SDL_RenderFillRect(renderer, &pixel_rect);
                    }
                }
            }
        }

        SDL_SetRenderTarget(renderer, NULL);
    }
    SDL_Rect pt_dest = {
        .x = GAME_WIDTH,
        .y = 0,
        .w = PT_WIDTH * 2,
        .h = GAME_HEIGHT
    };
    SDL_RenderCopy(renderer, pt_texture, NULL, &pt_dest);
    // ======================= Pattern Tables =======================

    // ======================= Debug Text =======================
    SDL_Rect debug_rect = { 0, GAME_HEIGHT, WINDOW_WIDTH, DEBUG_HEIGHT };
    SDL_SetRenderDrawColor(renderer, 50, 60, 80, 255);
    SDL_RenderFillRect(renderer, &debug_rect);

    char reg_text[256];
    snprintf(reg_text, sizeof(reg_text),
             "PC: $%04X   A: $%02X   X: $%02X   Y: $%02X   SP: $%02X   P: %c%c-%c%c%c%c%c%c    FPS: %02i\n"
             "PPUCTRL: $%02X   PPUMASK: $%02X   PPUSTATUS: $%02X   OAMADDR: $%02X\n"
             "OAMDATA: $%02X   PPUSCROLL: $%02X   PPUADDR: $%02X   PPUDATA: $%02X",
             cpu->PC, cpu->A, cpu->X, cpu->Y, cpu->S,
             (cpu->P & FLAG_NEGATIVE) ? 'N' : 'n',
             (cpu->P & FLAG_OVERFLOW) ? 'V' : 'v',
             (cpu->P & FLAG_UNUSED) ? 'U' : 'u',
             (cpu->P & FLAG_BREAK) ? 'B' : 'b',
             (cpu->P & FLAG_DECIMAL) ? 'D' : 'd',
             (cpu->P & FLAG_INT) ? 'I' : 'i',
             (cpu->P & FLAG_ZERO) ? 'Z' : 'z',
             (cpu->P & FLAG_CARRY) ? 'C' : 'c',
             ppu->FPS,
             ppu->PPUCTRL, ppu->PPUMASK, ppu->PPUSTATUS, ppu->OAMADDR, ppu->OAMDATA, ppu->PPUSCROLL, ppu->PPUADDR, ppu->PPUDATA
    );

    // Split the text into lines using strtok
    char *line = strtok(reg_text, "\n");
    int line_height = 20;  // Adjust the height of each line for spacing

    SDL_Color white = {255, 255, 255, 255};
    
    // Render each line of text separately
    int y_offset = GAME_HEIGHT + (DEBUG_HEIGHT - line_height * 3) / 2;  // Adjust initial y_offset as needed
    while (line != NULL) {
        SDL_Surface *text_surface = TTF_RenderText_Solid(font, line, white);
        SDL_Texture *text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);

        SDL_Rect text_rect = {
            .x = (WINDOW_WIDTH - text_surface->w) / 2,
            .y = y_offset,
            .w = text_surface->w,
            .h = text_surface->h
        };
        SDL_RenderCopy(renderer, text_texture, NULL, &text_rect);
        SDL_FreeSurface(text_surface);
        SDL_DestroyTexture(text_texture);

        line = strtok(NULL, "\n");  // Move to the next line
        y_offset += line_height;    // Increment the y_offset to render the next line lower
    }
    // ======================= Debug Text =======================

    // Render
    SDL_RenderPresent(renderer);
}
