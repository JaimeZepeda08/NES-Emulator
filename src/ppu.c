#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/ppu.h"
#include "../include/log.h"
#include "../include/memory.h"
#include "../include/cpu.h"

uint32_t calculate_pixel_color(PPU *ppu, int x, int y);
uint32_t get_background_pixel(PPU *ppu, int x, int y);
uint32_t get_sprite_pixel(PPU *ppu, int x, int y, uint32_t bg_color, int *sprite_hit);
uint16_t mirror_nametable(PPU *ppu, uint16_t address);

struct PPU *ppu_init() {
    printf("Initializing PPU...");

    struct PPU *ppu = (struct PPU *)malloc(sizeof(struct PPU));
    if (ppu == NULL) {
        printf("\tFAILED\n");
        FATAL_ERROR("PPU", "PPU memory allocation failed");
    }

    // Initialize PPU memory 
    memset(ppu->vram, 0, VRAM_SIZE);
    memset(ppu->oam, 0, OAM_SIZE);
    memset(ppu->frame_buffer, 0, NES_WIDTH * NES_HEIGHT);

    // Set up register
    ppu->PPUCTRL = 0;
    ppu->PPUMASK = 0;
    ppu->PPUSTATUS = 0x80;  // Bit 7 set on reset (VBlank clear)
    ppu->OAMADDR = 0;
    ppu->OAMDATA = 0;
    ppu->PPUSCROLL = 0;
    ppu->PPUADDR = 0;
    ppu->PPUDATA = 0;

    ppu->v = 0;
    ppu->t = 0;
    ppu->w = 0;
    ppu->x = 0;
    ppu->data_buffer = 0;

    ppu->nmi = 0;

    // initialize frame counter
    ppu->frames = 0;
    ppu->FPS = 0;


    printf("\tDONE\n");
    return ppu;
}

void ppu_free(PPU *ppu) {
    free(ppu);
}

int ppu_run_cycle(PPU *ppu) {
    int frame_complete = 0;

    // Pre render scanline 
    if (ppu->scanline == -1) {
        if (ppu->cycle == 1) {
            ppu->PPUSTATUS &= ~PPUSTATUS_V; // Clear VBlank flag after the frame is complete
            ppu->PPUSTATUS &= ~PPUSTATUS_S; // Clear Sprite 0 Hit mask for next frame
        }

        // OAMADDR is set to 0 during each of ticks 257–320 of pre render scanline
        if (ppu->cycle >= 257 && ppu->cycle <= 320) {
            ppu->OAMADDR = 0;
        }

        // Handle pre render scanline even/odd frames
        if (ppu->frames % 1 == 0 && ppu->cycle == 339) {
            ppu->cycle = 0;
            ppu->scanline++;
            return frame_complete;
        }
    }  

    // Visible scanlines
    if (ppu->scanline >= 0 && ppu->scanline < NES_HEIGHT) {
        // OAMADDR is set to 0 during each of ticks 257–320 of visible scanlines
        if (ppu->cycle >= 257 && ppu->cycle <= 320) {
            ppu->OAMADDR = 0;
        }

        // cycle 0 is idle
        if (ppu->cycle >= 1 && ppu->cycle <= NES_WIDTH) {
            int x = ppu->cycle - 1;
            int y = ppu->scanline;

            // Draw pixel
            ppu->frame_buffer[y * NES_WIDTH + x] = calculate_pixel_color(ppu, x, y);
        }
    }

    // Post render scanline
    if (ppu->scanline == 240) {
        // idle scan line
    }

    // Vertical blanking lines, non-visible scanlines
    // game can modify screen
    if (ppu->scanline >= 241) {
        // VBlank flag is set at the second (cycle 1) tick of scanline 241
        if (ppu->scanline == 241 && ppu->cycle == 1) {
            ppu->PPUSTATUS |= PPUSTATUS_V; // Set VBlank flag
        }

        // if NMI is enabled, blanking VBlank NMI occurs here too 
        if ((ppu->PPUCTRL & PPUCNTRL_V) && ppu->nmi == 0) { // detects on rising edge of nmi
            // signal cpu's NMI handler
            ppu->nmi = 1;
        }
    }

    // Advance cycle
    ppu->cycle++;
    if (ppu->cycle > 340) { // reached last horizontal cycle --> go to next scanline
        ppu->cycle = 0;
        ppu->scanline++;
        if (ppu->scanline >= 261) {  // reached bottom scanline
            ppu->scanline = -1; // pre-render scanline
            frame_complete = 1;
            ppu->frames++;
        }
    }

    return frame_complete;
}

uint32_t calculate_pixel_color(PPU *ppu, int x, int y) {
    int sprite_hit = 0;
    uint32_t bg_color = get_background_pixel(ppu, x, y);
    uint32_t sprite_color = get_sprite_pixel(ppu, x, y, bg_color, &sprite_hit);
    if (sprite_hit) {
        ppu->PPUSTATUS |= PPUSTATUS_S;
    }
    return sprite_color != 0 ? sprite_color : bg_color;
}

uint32_t get_background_pixel(PPU *ppu, int x, int y) {
    uint32_t bg_color = 0x00000000; // black

    // Check if background rendering is enabled
    if ((ppu->PPUMASK & PPUMASK_b) && (x >= 8 || (ppu->PPUMASK & PPUMASK_m))) {
        int scroll_x = ((ppu->t & 0x001F) << 3) | ppu->x;  // coarse X * 8 + fine X
        int scroll_y = (((ppu->t >> 5) & 0x001F) << 3) | ((ppu->t >> 12) & 0x7); // coarse Y * 8 + fine Y

        int scrolled_x = (x + scroll_x) % 512; // horizontal wrap
        int scrolled_y = (y + scroll_y) % 480; // vertical wrap

        int tile_x = (scrolled_x / 8) % 32;
        int tile_y = (scrolled_y / 8) % 30;
        int fine_x = scrolled_x % 8;
        int fine_y = scrolled_y % 8;

        // Fetch nametable entry
        uint16_t base_nametable = 0x2000;
        if (scrolled_x >= 256 && scrolled_y < 240)
            base_nametable = 0x2400;
        else if (scrolled_x < 256 && scrolled_y >= 240)
            base_nametable = 0x2800;
        else if (scrolled_x >= 256 && scrolled_y >= 240)
            base_nametable = 0x2C00;
        uint16_t nametable_address = mirror_nametable(ppu, base_nametable) + tile_y * 32 + tile_x;
        uint8_t tile_index = ppu->vram[nametable_address & 0x3FFF];

        // Get corresponding pattern table
        uint16_t pattern_table_addr = (ppu->PPUCTRL & PPUCNTRL_B) ? 0x1000 : 0x0000;
        uint16_t tile_address = pattern_table_addr + tile_index * 16;
        uint8_t bitplane0 = ppu->vram[(tile_address + fine_y) & 0x1FFF];
        uint8_t bitplane1 = ppu->vram[(tile_address + fine_y + 8) & 0x1FFF];
        int bit = 7 - fine_x;

        // Get low byte
        uint8_t pixel_low  = (bitplane0 >> bit) & 1;

        // Get high byte
        uint8_t pixel_high = (bitplane1 >> bit) & 1;

        // Get palette index
        uint8_t color_id = (pixel_high << 1) | pixel_low;

        if (color_id % 4 != 0) {
            // Fetch corresponding attribute table entry
            uint16_t attr_address = mirror_nametable(ppu, base_nametable + 0x3C0 + (tile_y / 4) * 8 + (tile_x / 4));
            uint8_t attr_byte = ppu->vram[attr_address & 0x3FFF];

            int shift = ((tile_y % 4) / 2) * 4 + ((tile_x % 4) / 2) * 2;
            uint8_t palette_bits = (attr_byte >> shift) & 0x03;

            uint8_t palette_index = (palette_bits << 2) | color_id;
            uint16_t palette_address = PALETTE_BASE + (palette_index & 0x1F);
            SDL_Color color = nes_palette[ppu->vram[palette_address] & 0x3F];

            if (ppu->PPUMASK & PPUMASK_Gr) {
                uint8_t gray = (color.r + color.g + color.b) / 3;
                color.r = color.g = color.b = gray;
            }

            bg_color = (color.r << 24) | (color.g << 16) | (color.b << 8) | 0xFF;
        } else {
            // universal background color if color_id % 4 == 0
            SDL_Color sdl_bg_color = nes_palette[ppu->vram[PALETTE_BASE]];
            bg_color = (sdl_bg_color.r << 24) | (sdl_bg_color.g << 16) | (sdl_bg_color.b << 8) | 0xFF;
        }
    }

    return bg_color;
}

uint32_t get_sprite_pixel(PPU *ppu, int x, int y, uint32_t bg_color, int *sprite_hit) {
    uint32_t sprite_color = 0x00000000; // black 

    if (ppu->PPUMASK & PPUMASK_s) {
        for (int i = 0; i < 64; i++) {
            int base = i * 4;
            uint8_t sprite_y = ppu->oam[base];
            uint8_t tile_index = ppu->oam[base + 1];
            uint8_t attr = ppu->oam[base + 2];
            uint8_t sprite_x = ppu->oam[base + 3];

            int sprite_height = (ppu->PPUCTRL & 0x20) ? 16 : 8;

            if (x < sprite_x || x >= sprite_x + 8 || y < sprite_y || y >= sprite_y + sprite_height) {
                continue;
            }

            int sx = x - sprite_x;
            int sy = y - sprite_y;
            if (attr & 0x40) sx = 7 - sx;
            if (attr & 0x80) sy = sprite_height - 1 - sy;

            uint16_t pattern_table = (ppu->PPUCTRL & PPUCNTRL_S) ? 0x1000 : 0x0000;
            uint16_t tile_addr = pattern_table + tile_index * 16;
            if (sprite_height == 16) {
                tile_addr = (tile_index & 0xFE) * 16 + ((tile_index & 1) ? 0x1000 : 0x0000);
            }

            uint8_t plane0 = ppu->vram[(tile_addr + sy) & 0x1FFF];
            uint8_t plane1 = ppu->vram[(tile_addr + sy + 8) & 0x1FFF];

            int bit = 7 - sx;
            uint8_t p0 = (plane0 >> bit) & 1;
            uint8_t p1 = (plane1 >> bit) & 1;
            uint8_t color_id = (p1 << 1) | p0;

            if (color_id == 0) continue;

            uint8_t palette_index = 0x10 + ((attr & 0x03) << 2) + color_id;
            uint16_t palette_addr = PALETTE_BASE + (palette_index & 0x1F);
            SDL_Color color = nes_palette[ppu->vram[palette_addr] & 0x3F];

            if (ppu->PPUMASK & PPUMASK_Gr) {
                uint8_t gray = (color.r + color.g + color.b) / 3;
                color.r = color.g = color.b = gray;
            }

            // If sprite is in front of background OR background is transparent, draw sprite
            if ((attr & 0x20) == 0) {
                sprite_color = (color.r << 24) | (color.g << 16) | (color.b << 8) | 0xFF;
            }

            // Sprite 0 Hit
            if (i == 0 && color_id != 0 && ((bg_color >> 24) & 0xFF) != 0) {
                *sprite_hit = 1;
            }         
        }
    }

    return sprite_color;
}

uint16_t mirror_nametable(PPU *ppu, uint16_t address) {
    switch (ppu->mirroring) {
        case MIRROR_VERTICAL:
            if (address >= 0x2800 && address < 0x2C00)
                return address - 0x800;
            if (address >= 0x2C00 && address < 0x3000)
                return address - 0x800;
            break;
        case MIRROR_HORIZONTAL:
            if (address >= 0x2400 && address < 0x2800)
                return address - 0x400;
            if (address >= 0x2C00 && address < 0x3000)
                return address - 0x800;
            break;
    }
    return address;
}

uint8_t ppu_read(PPU *ppu, uint16_t reg) {
    DEBUG_MSG_PPU("Reading register 0x%04X", reg);
    switch (reg) {
        // Write
        case PPUCTRL_REG: {
           ERROR_MSG("PPU", "PPUCTRL (0x2000) register is Write Only");
           return 0xFF;
        }

        // Write
        case PPUMASK_REG: {
            ERROR_MSG("PPU", "PPUMASK (0x2001) register is Write Only");
            return 0xFF;
        }
        
        // Read
        case PPUSTATUS_REG: {
            uint8_t status = ppu->PPUSTATUS;
            // reading PPUSTATUS clears VBlank flag and resets w register 
            // this is done when the state of w may not be known 
            ppu->PPUSTATUS &= ~PPUSTATUS_V; // clear VBlank
            ppu->w = 0; // clear w register
            return status;
        }
        
        // Write 
        case OAMADDR_REG: {
            ERROR_MSG("PPU", "OAMADDR (0x2003) register is Write Only");
            return 0xFF;
        }

        // Read/Write
        case OAMDATA_REG: {
            return ppu->oam[ppu->OAMADDR]; // do not increase addr
        }

        // Write (2 writes: X, Y)
        case PPUSCROLL_REG: {
            ERROR_MSG("PPU", "PPUSCROLL (0x2005) register is Write Only");
            return 0xFF;
        }

        // Write (2 writes: MSB, LSB)
        case PPUADDR_REG: {
            ERROR_MSG("PPU", "PPUADDR (0x2006) register is Write Only");
            return 0xFF;
        }

        // Read/Write
        // Memory is byte-addressable, so each read/write accesses one byte
        case PPUDATA_REG: {
            // palette data can be read without buffering
            if (ppu->v >= PALETTE_BASE) {
                uint16_t mirrored_addr = PALETTE_BASE | (ppu->v & 0x1F);
                if ((mirrored_addr & 0x13) == 0x10) mirrored_addr &= ~0x10;
                ppu->PPUDATA = ppu->vram[mirrored_addr];

                // perform buffered read for next read
                uint16_t buffered_addr = ppu->v & 0x2FFF;
                ppu->data_buffer = ppu->vram[buffered_addr];
            } else { // Normal VRAM read
                // buffer data reads for one extra cycle
                ppu->PPUDATA = ppu->data_buffer;
                ppu->data_buffer = ppu->vram[ppu->v & 0x3FFF];
            }

            // Increase address after read
            if (ppu->PPUCTRL & PPUCNTRL_I) {
                ppu->v += 32;
            } else {
                ppu->v += 1;
            }

            return ppu->PPUDATA;
        }

        default:
            return 0xFF; // invalid
    }
}

void ppu_write(PPU *ppu, uint16_t reg, uint8_t value) {
    DEBUG_MSG_PPU("Writing 0x%02X to register 0x%04X", value, reg);
    switch (reg) {
        // Write 
        case PPUCTRL_REG: {
            ppu->PPUCTRL = value;

            // bits 0-1: base nametable address in t register
            ppu->t = (ppu->t & 0xF3FF) | ((value & (PPUCNTRL_N_LOW | PPUCNTRL_N_HIGH)) << 10); // set bits 10-11 of t
            
            break;
        }
        
        // Write
        case PPUMASK_REG: {
            ppu->PPUMASK = value;
            break;
        }

        // Read
        case PPUSTATUS_REG: {
            ERROR_MSG("PPU", "PPUSTATUS (0x2002) register is Read Only");
            break;
        }

        // Write
        case OAMADDR_REG: {
            ppu->OAMADDR = value;
            break;
        }

        // Read/Write
        case OAMDATA_REG: {
            ppu->OAMDATA = value;
            ppu->oam[ppu->OAMADDR] = ppu->OAMDATA;
            ppu->OAMADDR += 1; // increase address after write
            break;
        }

        // Write (2 writes: X, Y)
        case PPUSCROLL_REG: {
            ppu->PPUSCROLL = value; 

            // takes 2 writes to set scroll position
            // first write sets X scroll, second write sets Y scroll
            // whether it is the first or second write is determined by w register

            if (ppu->w == 0) {
                ppu->x = value & 0x07;                       // fine X scroll
                ppu->t = (ppu->t & 0x7FE0) | (value >> 3);   // coarse X scroll
                ppu->w = 1; // set w to 1 for second write
            } else {
                ppu->t = (ppu->t & 0x0C1F)
                        | ((value & 0x07) << 12)              // fine Y scroll
                        | ((value & 0xF8) << 2);              // coarse Y scroll
                ppu->w = 0; // reset w register
            }
            break;
        }
            
        // Write (2 writes: MSB, LSB)
        // VRAM has 14-bit address, so we need 2 writes to set full address
        case PPUADDR_REG: {
            ppu->PPUADDR = value;

            // Write 16-bit address one byte at a time (high byte first)
            // whether it is the first or second write is determined by w register

            // First write (high byte)
            if (ppu->w == 0) {
                ppu->t = (ppu->t & 0x00FF) | ((value & 0x3F) << 8); // write high byte, only 14 bits (0x3FFF) used
                ppu->w = 1; // set w to 1 for second write
            } else { // Second write (low byte)
                ppu->t = (ppu->t & 0xFF00) | value; // write low byte
                ppu->v = ppu->t; // copy t to v for actual address
                ppu->w = 0; // reset w register
            }
            break;
        }

        // Read/Write
        case PPUDATA_REG: {
            ppu->PPUDATA = value; // not an actual latch so not needed, only for debug

            // Mirror writes to palette
            // Uses v register set by PPUADDR writes
            if (ppu->v >= PALETTE_BASE) {
                uint16_t palette_offset = ppu->v & 0x1F;
                if ((palette_offset & 0x13) == 0x10) palette_offset &= ~0x10;
                uint16_t palette_addr = PALETTE_BASE | palette_offset;
                ppu->vram[palette_addr] = value;
            } else { // Write to VRAM
                ppu->vram[ppu->v & 0x3FFF] = value;
            }

            // Auto increment address after write based on PPUCTRL setting
            if (ppu->PPUCTRL & PPUCNTRL_I) {
                ppu->v += 32; // next row
            } else {
                ppu->v += 1; // next column
            } 
            break;
        }
    
        default:
            break;
    }
}

void ppu_dump_registers(PPU *ppu) {
    DEBUG_MSG_PPU("CTRL: %02X  MASK: %02X  STATUS: %02X  OAMADDR: %02X  OAMDATA: %02X  SCROLL: %02X  PPUADDR: %02X  PPUDATA: %02X",
            ppu->PPUCTRL, ppu->PPUMASK, ppu->PPUSTATUS, ppu->OAMADDR, ppu->OAMDATA, ppu->PPUSCROLL, ppu->PPUADDR, ppu->PPUDATA);
}

void ppu_memory_dump(FILE *output, PPU *ppu) {
    if (!ppu || !output) {
        fprintf(stderr, "[ERROR] [Memory] Invalid memory or output stream!\n");
        return;
    }

    for (size_t i = 0; i < VRAM_SIZE; i += 16) {
        fprintf(output, "%04lX: ", i);  // Print memory address (offset)
        
        // Print hex bytes
        for (size_t j = 0; j < 16; j++) {
            if (i + j < VRAM_SIZE) {
                fprintf(output, "%02X ", ppu->vram[i + j]); // Print hex value
            } else {
                fprintf(output, "   ");  // Padding for alignment
            }
        }

        fprintf(output, " | "); // Separator

        // Print ASCII representation
        for (size_t j = 0; j < 16; j++) {
            if (i + j < VRAM_SIZE) {
                uint8_t byte = ppu->vram[i + j];
                fprintf(output, "%c", (byte >= 32 && byte <= 126) ? byte : '.'); // Printable ASCII or '.'
            }
        }
        
        fprintf(output, " |\n"); // End of line
    }
}

SDL_Color nes_palette[64] = {
    {124, 124, 124, 255}, {0, 0, 252, 255}, {0, 0, 188, 255}, {68, 40, 188, 255},
    {148, 0, 132, 255}, {168, 0, 32, 255}, {168, 16, 0, 255}, {136, 20, 0, 255},
    {80, 48, 0, 255}, {0, 120, 0, 255}, {0, 104, 0, 255}, {0, 88, 0, 255},
    {0, 64, 88, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, {0, 0, 0, 255},

    {188, 188, 188, 255}, {0, 120, 248, 255}, {0, 88, 248, 255}, {104, 68, 252, 255},
    {216, 0, 204, 255}, {228, 0, 88, 255}, {248, 56, 0, 255}, {228, 92, 16, 255},
    {172, 124, 0, 255}, {0, 184, 0, 255}, {0, 168, 0, 255}, {0, 168, 68, 255},
    {0, 136, 136, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, {0, 0, 0, 255},
    
    {248, 248, 248, 255}, {60, 188, 252, 255}, {104, 136, 252, 255}, {152, 120, 248, 255},
    {248, 120, 248, 255}, {248, 88, 152, 255}, {248, 120, 88, 255}, {252, 160, 68, 255},
    {248, 184, 0, 255}, {184, 248, 24, 255}, {88, 216, 84, 255}, {88, 248, 152, 255},
    {0, 232, 216, 255}, {120, 120, 120, 255}, {0, 0, 0, 255}, {0, 0, 0, 255},
    
    {252, 252, 252, 255}, {164, 228, 252, 255}, {184, 184, 248, 255}, {216, 184, 248, 255},
    {248, 184, 248, 255}, {248, 164, 192, 255}, {240, 208, 176, 255}, {252, 224, 168, 255},
    {248, 216, 120, 255}, {216, 248, 120, 255}, {184, 248, 184, 255}, {184, 248, 216, 255},
    {0, 252, 252, 255}, {248, 216, 248, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}
};