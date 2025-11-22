#include <SDL.h>
#include <SDL_ttf.h>
#include "ppu.h"
#include "cpu.h"

#define SCALE_FACTOR 2.5

#define GAME_WIDTH  ((int)(NES_WIDTH * SCALE_FACTOR))
#define GAME_HEIGHT ((int)(NES_HEIGHT * SCALE_FACTOR))

#define PT_WIDTH    128
#define PT_HEIGHT   256

#define DEBUG_BASE_HEIGHT 40
#define DEBUG_HEIGHT ((int)(DEBUG_BASE_HEIGHT * SCALE_FACTOR))

#define FONT_BASE_SIZE 6
#define FONT_SIZE ((int)(FONT_BASE_SIZE * SCALE_FACTOR))

#define WINDOW_WIDTH  (GAME_WIDTH + (int)(PT_WIDTH * SCALE_FACTOR))
#define WINDOW_HEIGHT (GAME_HEIGHT + DEBUG_HEIGHT)


SDL_Window *window_init(int pt_enable);
void render_display(SDL_Renderer *renderer, SDL_Texture *game_texture, TTF_Font *font, int pt_enable);