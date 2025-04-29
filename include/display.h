#include <SDL.h>
#include <SDL_ttf.h>
#include "ppu.h"
#include "cpu.h"

#define GAME_WIDTH      (NES_WIDTH * 2)
#define GAME_HEIGHT     (NES_HEIGHT * 2)
#define PT_WIDTH        128
#define PT_HEIGHT       256
#define DEBUG_HEIGHT    80

#define WINDOW_WIDTH    (GAME_WIDTH + PT_WIDTH * 2)
#define WINDOW_HEIGHT   (GAME_HEIGHT + DEBUG_HEIGHT)

SDL_Window *window_init();
void render_display(SDL_Renderer *renderer, PPU *ppu, CPU *cpu, SDL_Texture *game_texture, TTF_Font *font);