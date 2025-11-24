#include <stdio.h>
#include <stdlib.h>
#include "../include/input.h"
#include "../include/log.h"

CNTRL *cntrl_init() {
    printf("Initializing Controller...");

    CNTRL *cntrl = (CNTRL *)malloc(sizeof(CNTRL));
    if (cntrl == NULL) {
        printf("\tFAILED\n");
        FATAL_ERROR("CNTRL", "CNTRL memory allocation failed");
    }

    cntrl->button_state = 0;
    cntrl->shift_reg = 0;
    cntrl->strobe = 0;

    printf("\tDONE\n");
    return cntrl;
}

void cntrl_free(CNTRL *cntrl) {
    free(cntrl);
}

void cntrl1_handle_input(CNTRL *cntrl, SDL_Event *event) {
    if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) {
        uint8_t mask = 0;

        switch (event->key.keysym.sym) {
            case SDLK_x: 
                mask = NES_BUTTON_A; 
                break;
            case SDLK_z: 
                mask = NES_BUTTON_B; 
                break;
            case SDLK_RETURN: 
                mask = NES_BUTTON_START; 
                break;
            case SDLK_RSHIFT: 
                mask = NES_BUTTON_SELECT; 
                break;
            case SDLK_UP: 
                mask = NES_BUTTON_UP; 
                break;
            case SDLK_DOWN: 
                mask = NES_BUTTON_DOWN; 
                break;
            case SDLK_LEFT: 
                mask = NES_BUTTON_LEFT; 
                break;
            case SDLK_RIGHT: 
                mask = NES_BUTTON_RIGHT; 
                break;
            default:
                return; // Ignore other keys
        }

        if (event->type == SDL_KEYDOWN) {
            if (!(cntrl->button_state & mask)) {
                DEBUG_MSG_CNTRL("Key [%s] pressed", SDL_GetKeyName(event->key.keysym.sym));
                cntrl->button_state |= mask;  // Set bit (button pressed)
            }
        } else {
            cntrl->button_state &= ~mask; // Clear bit (button released)
        }
    }
}

void cntrl2_handle_input(CNTRL *cntrl, SDL_Event *event) {
    if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) {
        uint8_t mask = 0;

        switch (event->key.keysym.sym) {
            case SDLK_l: 
                mask = NES_BUTTON_A; 
                break;
            case SDLK_k: 
                mask = NES_BUTTON_B; 
                break;
            case SDLK_h: 
                mask = NES_BUTTON_START; 
                break;
            case SDLK_g: 
                mask = NES_BUTTON_SELECT; 
                break;
            case SDLK_w: 
                mask = NES_BUTTON_UP; 
                break;
            case SDLK_s: 
                mask = NES_BUTTON_DOWN; 
                break;
            case SDLK_a: 
                mask = NES_BUTTON_LEFT; 
                break;
            case SDLK_d: 
                mask = NES_BUTTON_RIGHT; 
                break;
            default:
                return; // Ignore other keys
        }

        if (event->type == SDL_KEYDOWN) {
            if (!(cntrl->button_state & mask)) {
                DEBUG_MSG_CNTRL("Key [%s] pressed", SDL_GetKeyName(event->key.keysym.sym));
                cntrl->button_state |= mask;  // Set bit (button pressed)
            }
        } else {
            cntrl->button_state &= ~mask; // Clear bit (button released)
        }
    }
}

void cntrl_write(CNTRL *cntrl, uint8_t value) {
    cntrl->strobe = value & 1;
    if (cntrl->strobe) {
        // Copy button state to shift register
        cntrl->shift_reg = cntrl->button_state;
    }
}

uint8_t cntrl_read(CNTRL *cntrl) {
    uint8_t bit = cntrl->shift_reg & 1; 
    cntrl->shift_reg >>= 1; 
    return bit;
}
