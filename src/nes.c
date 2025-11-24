#include "../include/nes.h"
#include <stdlib.h>
#include <stdio.h>

NES *nes = NULL; 

void nes_init(char *filename) {
    nes = (NES *)malloc(sizeof(NES)); 
    if (nes == NULL) {
        fprintf(stderr, "Memory allocation for NES instance failed!\n");
        exit(1);
    }
    printf("Initializing NES System...\n");

    // initialize Memory first (before CPU needs to read reset vector)
    memset(nes->ram, 0, RAM_SIZE);
    memset(nes->vram, 0, VRAM_SIZE);

    // load cartridge and mapper (before CPU init, since CPU reads reset vector from ROM)
    Cartridge *cart = cart_init(filename);
    nes->mapper = mapper_init(cart);

    // initialize CPU (now it can read the reset vector)
    nes->cpu = cpu_init();

    // initialize PPU
    nes->ppu = ppu_init();

    // initialize APU
    nes->apu = apu_init();

    // initialize controllers
    nes->controller1 = cntrl_init();
    nes->controller2 = cntrl_init();
}

void nes_free() {
    if (nes) {
        if (nes->cpu) {
            cpu_free(nes->cpu);
        }
        if (nes->ppu) {
            ppu_free(nes->ppu);
        }
        if (nes->apu) {
            apu_free(nes->apu);
        }
        if (nes->controller1) {
            cntrl_free(nes->controller1);
        }
        if (nes->controller2) {
            cntrl_free(nes->controller2);
        }
        if (nes->mapper && nes->mapper->cart) {
            cart_free(nes->mapper->cart);
        }
        if (nes->mapper) {
            mapper_free(nes->mapper);   
        }
        
        free(nes);
        
        nes = NULL;
    }
}

int nes_cycle(uint32_t *last_time, SDL_Renderer *renderer, SDL_Texture *game_texture, TTF_Font *font, int pt_enable) {
    // run cpu cycle (unless DMA in progress)
    if (nes->ppu->oam_dma_transfer == 0) {
        cpu_run_cycle(nes->cpu);
    } else {
        ppu_oam_dma_transfer(nes->ppu);
        nes->cpu->cycles = 2; // CPU is stalled for 514 cycles during OAM DMA (so 2 per transfer step)
        nes->ppu->oam_dma_cycle++;
        if (nes->ppu->oam_dma_cycle >= 256) {
            nes->ppu->oam_dma_transfer = 0; // DMA complete
            nes->ppu->oam_dma_cycle = 0;
            nes->ppu->oam_dma_page = 0x00;
        }
    }

    // run PPU (3 * cycles completed by CPU)
    for (int i = 0; i < 3 * nes->cpu->cycles; i++) {
        int frame_complete = ppu_run_cycle(nes->ppu);
        if (frame_complete) {
            // calculate FPS    
            uint32_t curr_time = SDL_GetTicks();
            if (nes->ppu->frames > 10) {
                double elapsed = (double)(curr_time - *last_time) / 1000.0;
                nes->ppu->FPS = nes->ppu->frames / elapsed;
                nes->ppu->frames = 0;
                *last_time = SDL_GetTicks();
            }

            // render display
            render_display(renderer, game_texture, font, pt_enable);
        }
    }

    // APU cycle is called by the audio callback function

    return 1;
}


uint8_t nes_cpu_read(uint16_t address) {
    // fixed address space
    if (address < 0x6000) {
        // internal RAM and Mirrors
        if (address < 0x2000) {
            address = address % RAM_SIZE; // mirror every 2KB
            return nes->ram[address];
        } 
        // PPU registers and mirrors
        else if (address >= 0x2000 && address < 0x4000) {
            uint16_t reg_addr = 0x2000 + (address % 8);
            // ppu register read
            return ppu_register_read(nes->ppu, reg_addr);
        } 
        // APU registers and IO
        else if (address >= 0x4015 && address <= 0x4017) {
            // apu register read
            if (address == 0x4015) { // only readable APU register
                return apu_register_read(nes->apu, address);
            }
            // controller 1
            else if (address == 0x4016) {  
                return cntrl_read(nes->controller1);
            } 
            // controller 2
            else if (address == 0x4017) {
                return cntrl_read(nes->controller2);
            } 

            return 0; // other APU registers are write-only
        } 
    } 
    // cartridge space (mapped by the mapper)
    else if (address >= 0x6000 && address <= CPU_MEMORY_SIZE) {
        // mapper cpu read
        return nes->mapper->cpu_read(nes->mapper, address);
    }
    // unmapped addresses
    else {
        return 0; // open bus (not addressable)
    }

    return 0;
}

void nes_cpu_write(uint16_t address, uint8_t value) {
    // fixed address space
    if (address < 0x6000) {
        // internal RAM and Mirrors
        if (address < 0x2000) {
            address = address % RAM_SIZE; // mirror every 2KB
            nes->ram[address] = value;
            return;
        } 
        // PPU registers and mirrors
        else if (address >= 0x2000 && address < 0x4000) {
            uint16_t reg_addr = 0x2000 + (address % 8);
            // ppu register write
            ppu_register_write(nes->ppu, reg_addr, value);
            return;
        } 
        // APU and IO registers
        else if ((address >= 0x4000 && address <= 0x4013) || (address >= 0x4015 && address <= 0x4017)) {
            // apu register write
            apu_register_write(nes->apu, address, value);
            // controller 1
            if (address == 0x4016) {  
                // controller write
                cntrl_write(nes->controller1, value);
                return;
            } 
            // controller 2
            else if (address == 0x4017) {
                cntrl_write(nes->controller2, value);
                return;
            }
            return;
        } 
        else if (address == 0x4014) {
            // begin OAM DMA transfer
            nes->ppu->oam_dma_transfer = 1;
            nes->ppu->oam_dma_page = value;
            nes->ppu->oam_dma_cycle = 0;
            return;
        }
    } 
    // cartridge space (mapped by the mapper)
    else if (address >= 0x6000 && address <= CPU_MEMORY_SIZE) {
        // mapper cpu write
        nes->mapper->cpu_write(nes->mapper, address, value);
        return;
    } 
    // unmapped addresses
    else {
        return; // open bus (not addressable)
    }
}

uint8_t nes_ppu_read(uint16_t address) {
    // pattern tables (handled by the mapper)
    if (address < 0x2000) {
        return nes->mapper->ppu_read(nes->mapper, address);
    }
    // nametables and mirrors 
    else if (address >= 0x2000 && address < 0x3F00) {
        return nes->vram[nes->mapper->mirror_nametable(nes->mapper, address) % 0x0FFF]; // mirror by 4KB (size of nametable space)
    }
    // pallette RAM and mirrors 
    else if (address >= 0x3F00 && address <= PPU_MEMORY_SIZE) {
        return 0; // handled internally by PPU 
    }
    // unmapped addresses
    else {
        return 0; // open bus (not addressable)
    }
}

void nes_ppu_write(uint16_t address, uint8_t value) {
    // pattern tables (handled by the mapper)
    if (address < 0x2000) {
        nes->mapper->ppu_write(nes->mapper, address, value);
        return;
    }
    // nametables and mirrors 
    else if (address >= 0x2000 && address < 0x3F00) {
        nes->vram[nes->mapper->mirror_nametable(nes->mapper, address) % 0x0FFF] = value; // mirror by 4KB (size of nametable space)
        return;
    }
    // pallette RAM and mirrors 
    else if (address >= 0x3F00 && address <= PPU_MEMORY_SIZE) {
        return; // handled internally by PPU 
    }
    // unmapped addresses
    else {
        return; // open bus (not addressable)
    }
}