#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL.h>
#include <SDL_ttf.h>
#include "ppu.h"
#include "cpu.h"

#define SCALE_FACTOR 2.5

#define GAME_WIDTH  ((int)(NES_WIDTH * SCALE_FACTOR))
#define GAME_HEIGHT ((int)(NES_HEIGHT * SCALE_FACTOR))

#define PT_WIDTH    128
#define PT_HEIGHT   256

#define NT_WIDTH    256  // Nametable width (32 tiles * 8 pixels)
#define NT_HEIGHT   240  // Nametable height (30 tiles * 8 pixels)
// Scale nametables to fill the game height (2 nametables = 480px, scale to GAME_HEIGHT)
#define NT_SCALE    (GAME_HEIGHT / (NT_HEIGHT * 2.0))
#define NT_DISPLAY_WIDTH  ((int)(NT_WIDTH * NT_SCALE))
#define NT_DISPLAY_HEIGHT GAME_HEIGHT  // Fill entire game height with 2 nametables

#define DEBUG_BASE_HEIGHT 40
#define DEBUG_HEIGHT ((int)(DEBUG_BASE_HEIGHT * SCALE_FACTOR))

#define FONT_BASE_SIZE 6
#define FONT_SIZE ((int)(FONT_BASE_SIZE * SCALE_FACTOR))

#define WINDOW_WIDTH  (NT_DISPLAY_WIDTH + GAME_WIDTH + (int)(PT_WIDTH * SCALE_FACTOR))
#define WINDOW_HEIGHT (GAME_HEIGHT + DEBUG_HEIGHT)

typedef struct DISPLAY {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *game_texture;
    TTF_Font *font;
    int debug_enable; // shows pattern tables, name tables and CPU/PPU info when enabled
} DISPLAY;

DISPLAY *window_init(int debug_enable);
void free_display(DISPLAY *display);
void render_display(DISPLAY *display);

#endif