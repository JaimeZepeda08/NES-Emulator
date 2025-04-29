#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <SDL.h>

#define VRAM_SIZE           0x4000

////////////////////////////////////////////////////
//                PPU MEMORY MAP                  //
//================================================//
// 0x0000-0x0FFF    |   Pattern Table 0           //
// 0x1000-0x1FFF    |   Pattern Table 1           //
//================================================//
// 0x2000-0x23FF    |   Nametable 0               //
// 0x2400-0x27FF    |   Nametable 1               //
// 0x2800-0x2BFF    |   Nametable 2               //
// 0x2C00-0x2FFF    |   Nametable 3               //
// 0x3000-0x3EFF    |   Mirror of 0x2000-0x2FFF   //
//================================================//
// 0x3F00-0x3F1F    |   Pallete Ram               //
// 0x3F20-0x3FFF    |   Mirror of 0x3F00-0x3F1F   //
////////////////////////////////////////////////////

#define OAM_SIZE            256
#define NES_WIDTH           256
#define NES_HEIGHT          240

#define MIRROR_VERTICAL     0
#define MIRROR_HORIZONTAL   1

// ====================== Memory-Mapped Registers ======================

#define PPUCTRL_REG         0x2000 // Sets up rendering settings
#define PPUCNTRL_V          0x80 // Vblank NMI enable (0: off, 1: on)
#define PPUCNTRL_P          0x40 // PPU master/slave select (0: read backdrop from EXT pins; 1: output color on EXT pins)
#define PPUCNTRL_H          0x20 // Sprite size (0: 8x8 pixels; 1: 8x16 pixels)
#define PPUCNTRL_B          0x10 // Background pattern table address (0: $0000; 1: $1000)
#define PPUCNTRL_S          0x08 // Sprite pattern table address for 8x8 sprites (0: $0000; 1: $1000; ignored in 8x16 mode)
#define PPUCNTRL_I          0x04 // VRAM address increment per CPU read/write of PPUDATA (0: add 1, going across; 1: add 32, going down)
// Base nametable address (0 = $2000; 1 = $2400; 2 = $2800; 3 = $2C00)
// They are also used as MSB of scroll coordinates
#define PPUCNTRL_N_HIGH     0x02 // X scroll position bit 8 (i.e. add 256 to X)
#define PPUCNTRL_N_LOW      0x01 // Y scroll position bit 8 (i.e. add 240 to Y)

#define PPUMASK_REG        0x2001 // Controls rendering of background a sprites as well as color effects
#define PPUMASK_B          0x80 // Emphasize blue
#define PPUMASK_G          0x40 // Emphasize green
#define PPUMASK_R          0x20 // Emphasize red 
#define PPUMASK_s          0x10 // 1: Enable sprite rendering
#define PPUMASK_b          0x08 // 1: Enable background rendering
#define PPUMASK_M          0x04 // 1: Show sprites in leftmost 8 pixels of screen, 0: Hide
#define PPUMASK_m          0x02 // 1: Show background in leftmost 8 pixels of screen, 0: Hide
#define PPUMASK_Gr         0x01 // Greyscale (0: normal color, 1: greyscale)

#define PPUSTATUS_REG       0x2002 // Read only
#define PPUSTATUS_V         0x80 // Vblank flag, cleared on read
#define PPUSTATUS_S         0x40 // Sprite 0 hit flag
#define PPUSTATUS_O         0x20 // Sprite overflow flag

#define OAMADDR_REG         0x2003 // OAM memory address (Not commonly used)
#define OAMDATA_REG         0x2004 // OAM memory data (Not commonly used)

#define PPUSCROLL_REG       0x2005 
#define PPUADDR_REG         0x2006
#define PPUDATA_REG         0x2007

// ====================== Memory-Mapped Registers ======================

// NES master palette
#define PALETTE_BASE        0x3F00
extern SDL_Color nes_palette[64];

typedef struct PPU {
    uint8_t vram[VRAM_SIZE];
    uint8_t oam[OAM_SIZE];

    // Memory-Mapped Registers
    uint8_t PPUCTRL;
    uint8_t PPUMASK;
    uint8_t PPUSTATUS;
    uint8_t OAMADDR;
    uint8_t OAMDATA;
    uint8_t PPUSCROLL;
    uint8_t PPUADDR;
    uint8_t PPUDATA;

    // Internal Registers;
    uint16_t v; // Current VRAM address (15 bits)
    uint16_t t; // Temporary VRAM address (15 bits)
    uint8_t x;  // Fine X scroll (3 bits)
    uint8_t w;  // First or second write toggle (1 bit)
    uint8_t data_buffer; // buffers data read from PPU

    int nmi;

    int cycle;      // [0, 340]
    int scanline;   // [-1, 260], where -1 is the pre-render line, 0–239 are visible, 240 is post-render, 241–260 is VBlank
    int mirroring;  // Nametable mirroring mode

    uint32_t frame_buffer[NES_WIDTH * NES_HEIGHT]; // PPU writes to framebuffer and display renders to screen

    int frames; // keeps track of total frames to calculate FPS
    int FPS;
} PPU;

PPU *ppu_init();
void ppu_free(PPU *ppu);
int ppu_run_cycle(PPU *ppu);
uint32_t calculate_pixel_color(PPU *ppu, int x, int y);
uint16_t mirror_nametable(PPU *ppu, uint16_t address);
uint8_t ppu_read(PPU *ppu, uint16_t reg);
void ppu_write(PPU *ppu, uint16_t reg, uint8_t value);
void ppu_dump_registers(PPU *ppu);
void ppu_memory_dump(FILE *output, PPU *ppu);

#endif