#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/ppu.h"
#include "../include/log.h"
#include "../include/memory.h"
#include "../include/cpu.h"

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
    ppu->x = 0;
    ppu->w = 0;
    ppu->data_buffer = 0;

    ppu->nmi = 0;

    // initialize frame counter
    ppu->frames = 0;
    ppu->FPS = 60;


    printf("\tDONE\n");
    return ppu;
}

void ppu_free(PPU *ppu) {
    free(ppu);
}

int ppu_run_cycle(PPU *ppu) {
    int frame_complete = 0;

    if (ppu->scanline == -1 && ppu->cycle == 1) {
        ppu->PPUSTATUS &= ~PPUSTATUS_V; // Clear VBlank flag after the frame is complete

        // Clear the frame buffer at the start of new frame
        for (int i = 0; i < NES_WIDTH * NES_HEIGHT; i++) {
            ppu->frame_buffer[i] = 0x000000FF; // black
        }
    }

    // Non-visible scanlines --> game can modify screen
    if (ppu->scanline == 241 && ppu->cycle == 1) {
        ppu->PPUSTATUS |= PPUSTATUS_V; // Set VBlank flag
        if ((ppu->PPUCTRL & PPUCNTRL_V) && ppu->nmi == 0) { // detects on rising edge of nmi
            // signal cpu's NMI handler
            ppu->nmi = 1;
        }
    }

    if (ppu->scanline >= 0 && ppu->scanline < NES_HEIGHT) {
        if (ppu->cycle >= 1 && ppu->cycle <= NES_WIDTH) {
            int x = ppu->cycle - 1;
            int y = ppu->scanline;

            // Draw pixel
            ppu->frame_buffer[y * NES_WIDTH + x] = calculate_pixel_color(ppu, x, y);
        }
    }

    // Advance cycle
    ppu->cycle++;
    if (ppu->cycle > 340) { // reached last horizontal cycle --> go to next scanline
        ppu->cycle = 0;
        ppu->scanline++;
        if (ppu->scanline > 261) {  // reached bottom scanline
            ppu->scanline = -1; // pre-render scanline
            frame_complete = 1;
            ppu->frames++;
        }
    }

    return frame_complete;
}

uint32_t calculate_pixel_color(PPU *ppu, int x, int y) {
    uint32_t bg_color = 0x00000000;

    // 1. Background Rendering
    if ((ppu->PPUMASK & PPUMASK_b) && (x >= 8 || (ppu->PPUMASK & PPUMASK_m))) {
        // Calculate tile indices and fine scroll within the tile
        int tile_x = x / 8;
        int tile_y = y / 8;
        int fine_x = x % 8;
        int fine_y = y % 8;

        // Determine the base nametable address from PPUCTRL
        uint16_t base_nametable = 0x2000;
        switch (((ppu->PPUCTRL & PPUCNTRL_N_HIGH) << 1) | (ppu->PPUCTRL & PPUCNTRL_N_LOW)) {
            case 0b00: base_nametable = 0x2000; break;
            case 0b01: base_nametable = 0x2400; break;
            case 0b10: base_nametable = 0x2800; break;
            case 0b11: base_nametable = 0x2C00; break;
        }

        uint16_t nametable_address = mirror_nametable(ppu, base_nametable) + tile_y * 32 + tile_x;
        uint8_t tile_index = ppu->vram[nametable_address & 0x3FFF];

        // Find tile graphics in pattern table
        uint16_t pattern_table_addr = (ppu->PPUCTRL & PPUCNTRL_B) ? 0x1000 : 0x0000;
        uint16_t tile_address = pattern_table_addr + tile_index * 16;

        uint8_t bitplane0 = ppu->vram[(tile_address + fine_y) & 0x1FFF];
        uint8_t bitplane1 = ppu->vram[(tile_address + fine_y + 8) & 0x1FFF];

        int bit = 7 - fine_x;
        uint8_t pixel_low  = (bitplane0 >> bit) & 1;
        uint8_t pixel_high = (bitplane1 >> bit) & 1;
        uint8_t color_id = (pixel_high << 1) | pixel_low;

        if (color_id != 0) {
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
        }
    }

    // 2. Sprite Rendering
    uint32_t sprite_color = 0x00000000;
    int sprite_drawn = 0;

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

            sprite_color = (color.r << 24) | (color.g << 16) | (color.b << 8) | 0xFF;

            // If sprite is in front of background OR background is transparent, draw sprite
            if ((attr & 0x20) == 0 || bg_color == 0x00000000) {
                sprite_drawn = 1;
                break;
            }
        }
    }

    // 3. Final pixel
    return sprite_drawn ? sprite_color : bg_color;
}

uint16_t mirror_nametable(PPU *ppu, uint16_t address) {
    switch (ppu->mirroring) {
        case MIRROR_VERTICAL:
            break;

        case MIRROR_HORIZONTAL:
            break;
    }
    return address;
}

uint8_t ppu_read(PPU *ppu, uint16_t reg) {
    DEBUG_MSG_PPU("Reading register 0x%04X", reg);
    switch (reg) {
        case PPUCTRL_REG:
           return ppu->PPUCTRL;
        
        case PPUMASK_REG:
            return ppu->PPUMASK;

        case PPUSTATUS_REG: {
            uint8_t status = ppu->PPUSTATUS;
            ppu->PPUSTATUS &= ~PPUSTATUS_V; // clear VBlank
            ppu->w = 0; // clear w register
            return status;
        }
        
        case OAMADDR_REG:
            return ppu->OAMADDR;

        case OAMDATA_REG:
            return ppu->OAMDATA;
        
        case PPUSCROLL_REG:
            return ppu->PPUSCROLL;

        case PPUADDR_REG:
            return ppu->PPUADDR;

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
        case PPUCTRL_REG:
            ppu->PPUCTRL = value;
            break;
        
        case PPUMASK_REG:
            ppu->PPUMASK = value;
            break;

        case PPUSTATUS_REG:
            ERROR_MSG("PPU", "PPUSTATUS (0x2002) register is Read Only");
            break;
        
        case OAMADDR_REG:
            ppu->OAMADDR = value;
            break;

        case OAMDATA_REG:
            ppu->OAMDATA = value;
            break;
        
        case PPUSCROLL_REG:
            ppu->PPUSCROLL = value;
            break;

        case PPUADDR_REG: {
            ppu->PPUADDR = value;
            if (ppu->w == 0) {
                ppu->t = (ppu->t & 0x00FF) | ((value & 0x3F) << 8);
                ppu->w = 1;
            } else {
                ppu->t = (ppu->t & 0xFF00) | value;
                ppu->v = ppu->t;
                ppu->w = 0;
            }
            break;
        }

        case PPUDATA_REG: {
            ppu->PPUDATA = value;

            // Mirror writes to pallete
            if (ppu->v >= PALETTE_BASE) {
                uint16_t palette_offset = ppu->v & 0x1F;
                if ((palette_offset & 0x13) == 0x10) palette_offset &= ~0x10;
                uint16_t palette_addr = PALETTE_BASE | palette_offset;
                ppu->vram[palette_addr] = value;
            } else { // Write to VRAM
                ppu->vram[ppu->v & 0x3FFF] = value;
            }

            // Increase address after write
            if (ppu->PPUCTRL & PPUCNTRL_I) {
                ppu->v += 32;
            } else {
                ppu->v += 1;
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