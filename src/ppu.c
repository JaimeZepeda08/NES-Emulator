#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/ppu.h"
#include "../include/log.h"
#include "../include/memory.h"
#include "../include/cpu.h"

uint32_t calculate_pixel_color(PPU *ppu);
uint32_t get_background_pixel(PPU *ppu);
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
    ppu->x = 0;

    ppu->bg_next_tile_id = 0x00;
    ppu->bg_next_tile_attrib = 0x00;
    ppu->bg_next_tile_lsb = 0x00;
    ppu->bg_next_tile_msb = 0x00;
    ppu->bg_shifter_pattern_lo = 0x0000;
    ppu->bg_shifter_pattern_hi = 0x0000;
    ppu->bg_shifter_attrib_lo = 0x0000;
    ppu->bg_shifter_attrib_hi = 0x0000;

    ppu->w = 0;
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

    // ============ Pre render scanline ============
    // scan line -1 (261)
    if (ppu->scanline == -1) {
        if (ppu->cycle == 1) {
            ppu->PPUSTATUS &= ~PPUSTATUS_V; // Clear VBlank flag after the frame is complete
            ppu->PPUSTATUS &= ~PPUSTATUS_S; // Clear Sprite 0 Hit mask for next frame
            ppu->PPUSTATUS &= ~PPUSTATUS_O; // Clear Sprite Overflow flag for next frame
        }
    }  

    if (ppu->scanline == 0 && ppu->cycle == 0) {
        // skip cycle
        ppu->cycle += 1;
    }

    // ============ Visible scanlines ============
    if (ppu->scanline >= -1 && ppu->scanline < 240) {
        // idle cycle at cycle 0
        if (ppu->cycle == 0) {}

        // cycles 1-256 & 321-336: background fetching and shifter updates
        if ((ppu->cycle >= 2 && ppu->cycle <= 257) || (ppu->cycle >= 321 && ppu->cycle <= 338)) {

            // update shifters if rendering is enabled
            if (ppu->PPUMASK & (PPUMASK_b | PPUMASK_s)) {
                ppu->bg_shifter_pattern_lo <<= 1;
                ppu->bg_shifter_pattern_hi <<= 1;
                ppu->bg_shifter_attrib_lo <<= 1;
                ppu->bg_shifter_attrib_hi <<= 1; 
            }

            switch ((ppu->cycle - 1) % 8) {
                case 0: {
                    // load shifters
                    ppu->bg_shifter_pattern_lo = (ppu->bg_shifter_pattern_lo & 0xFF00) | ppu->bg_next_tile_lsb;
                    ppu->bg_shifter_pattern_hi = (ppu->bg_shifter_pattern_hi & 0xFF00) | ppu->bg_next_tile_msb;
                    ppu->bg_shifter_attrib_lo  = (ppu->bg_shifter_attrib_lo & 0xFF00) | ((ppu->bg_next_tile_attrib & 0b01) ? 0xFF : 0x00);
		            ppu->bg_shifter_attrib_hi  = (ppu->bg_shifter_attrib_hi & 0xFF00) | ((ppu->bg_next_tile_attrib & 0b10) ? 0xFF : 0x00);

                    // fetch next tile id
                    uint16_t v_addr = 0x2000 | (ppu->v & 0x0FFF);
                    uint16_t mirrored_addr = mirror_nametable(ppu, v_addr);
                    ppu->bg_next_tile_id = ppu->vram[mirrored_addr];
                    break; 
                }
                case 2: {
                    // fetch attribute byte
                    uint16_t v_addr = 0x23C0 | (ppu->v & 0x0C00) | ((ppu->v >> 4) & 0x38) | ((ppu->v >> 2) & 0x07);
                    uint16_t mirrored_addr = mirror_nametable(ppu, v_addr);
                    uint8_t attribute_byte = ppu->vram[mirrored_addr];

                    // extract palette bits based on coarse X and Y
                    uint8_t shift = ((ppu->v >> 4) & 4) | (ppu->v & 2);
                    ppu->bg_next_tile_attrib = (attribute_byte >> shift) & 0b11;
                    break;
                }
                case 4: {
                    // fetch low byte of tile bitmap
                    uint16_t fine_y = (ppu->v >> 12) & 0x7;
                    uint16_t base_table_addr = (ppu->PPUCTRL & PPUCNTRL_B) ? 0x1000 : 0x0000;
                    uint16_t tile_addr = base_table_addr + (ppu->bg_next_tile_id * 16) + fine_y;
                    ppu->bg_next_tile_lsb = ppu->vram[tile_addr];
                    break;
                }
                case 6: {
                    // fetch high byte of tile bitmap
                    uint16_t fine_y = (ppu->v >> 12) & 0x7;
                    uint16_t base_table_addr = (ppu->PPUCTRL & PPUCNTRL_B) ? 0x1000 : 0x0000;
                    uint16_t tile_addr = base_table_addr + (ppu->bg_next_tile_id * 16) + fine_y + 8;
                    ppu->bg_next_tile_msb = ppu->vram[tile_addr];
                    break;
                }
                case 7: {
                    // increment horizontal position in v
                    if (ppu->PPUMASK & (PPUMASK_b | PPUMASK_s)) {
                        if ((ppu->v & 0x001F) == 31) {
                            ppu->v &= ~0x001F;          // coarse X = 0
                            ppu->v ^= 0x0400;           // switch horizontal nametable
                        } else {
                            ppu->v += 1;                 // increment coarse X
                        }
                    }
                }
            }
        }

        if (ppu->cycle == 256) {
            // increment vertical position in v
            if (ppu->PPUMASK & (PPUMASK_b | PPUMASK_s)) {
                if ((ppu->v & 0x7000) != 0x7000) {
                    ppu->v += 0x1000; // increment fine Y
                } else {
                    ppu->v &= ~0x7000; // fine Y = 0
                    uint16_t coarse_y = (ppu->v & 0x03E0) >> 5;
                    if (coarse_y == 29) {
                        coarse_y = 0;
                        ppu->v ^= 0x0800; // switch vertical nametable
                    } else if (coarse_y == 31) {
                        coarse_y = 0; // wrap around
                    } else {
                        coarse_y += 1; // increment coarse Y
                    }
                    ppu->v = (ppu->v & ~0x03E0) | (coarse_y << 5);
                }
            }
        }

        if (ppu->cycle == 257) {
            // copy horizontal bits from t to v
            if (ppu->PPUMASK & (PPUMASK_b | PPUMASK_s)) {
                ppu->v = (ppu->v & 0xFBE0) | (ppu->t & 0x041F);
            }
        }

        // OAMADDR reset
        if (ppu->cycle >= 257 && ppu->cycle <= 320) {
            ppu->OAMADDR = 0;
        }

        if (ppu->cycle == 338 || ppu->cycle == 340) {
            // fetch next tile id (odd frame skip)
            uint16_t v_addr = 0x2000 | (ppu->v & 0x0FFF);
            uint16_t mirrored_addr = mirror_nametable(ppu, v_addr);
            ppu->bg_next_tile_id = ppu->vram[mirrored_addr];
        }

        if (ppu->scanline == -1 && ppu->cycle >= 280 && ppu->cycle < 305)
		{
            // copy vertical bits from t to v during pre-render scanline
            if (ppu->PPUMASK & (PPUMASK_b | PPUMASK_s)) {
                ppu->v = (ppu->v & 0x041F) | (ppu->t & 0xFBE0);
            }
		}

        // Render pixel during cycles 1-256
        if (ppu->scanline >= 0 && ppu->cycle >= 1 && ppu->cycle <= 256) {
            int x = ppu->cycle - 1;
            int y = ppu->scanline;
            ppu->frame_buffer[y * 256 + x] = calculate_pixel_color(ppu);
        }
    }

    // ============ Post render scanline ============
    // scan line 240
    if (ppu->scanline == 240) {
        // idle scan line
    }

    // ============  Vertical Blanking Period ============
    // scan lines 241-260
    // game can modify screen during this period
    if (ppu->scanline >= 241) {
        // VBlank flag is set at the second (cycle 1) tick of scanline 241
        if (ppu->scanline == 241 && ppu->cycle == 1) {
            ppu->PPUSTATUS |= PPUSTATUS_V; // Set VBlank flag

            // if NMI is enabled, blanking VBlank NMI occurs here too 
            if ((ppu->PPUCTRL & PPUCNTRL_V) && ppu->nmi == 0) { // detects on rising edge of nmi
                // signal cpu's NMI handler
                ppu->nmi = 1;
            }
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

uint32_t calculate_pixel_color(PPU *ppu) {
    return get_background_pixel(ppu);
}

uint32_t get_background_pixel(PPU *ppu) {
    if (!(ppu->PPUMASK & PPUMASK_b)) {
        // background rendering is disabled
        return 0x000000FF;
    }

    // get the bit corresponding to the current fine x scroll
    uint16_t bit_mux = 0x8000 >> ppu->x;

    // extract the two bits for the pixel from the pattern shifters
    uint8_t p0_pixel = (ppu->bg_shifter_pattern_lo & bit_mux) ? 1 : 0;
    uint8_t p1_pixel = (ppu->bg_shifter_pattern_hi & bit_mux) ? 1 : 0;
    uint8_t bg_pixel = (p1_pixel << 1) | p0_pixel;

    // extract the two bits for the palette from the attribute shifters
    uint8_t bg_pal0 = (ppu->bg_shifter_attrib_lo & bit_mux) ? 1 : 0;
    uint8_t bg_pal1 = (ppu->bg_shifter_attrib_hi & bit_mux) ? 1 : 0;
    uint8_t bg_palette = (bg_pal1 << 1) | bg_pal0;

    // get final color from palette
    uint8_t color_id = 0;
    if (bg_pixel != 0) {
        color_id = ppu->vram[PALETTE_BASE + (bg_palette << 2) + bg_pixel] & 0x3F;
    } else {
        color_id = ppu->vram[PALETTE_BASE]; // background color
    }

    SDL_Color sdl_bg_color = nes_palette[color_id];
    return (sdl_bg_color.r << 24) | (sdl_bg_color.g << 16) | (sdl_bg_color.b << 8) | 0xFF;
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