#ifndef CNTRL_H
#define CNTRL_H

#include <SDL.h>
#include <stdint.h>

// Memory-Mapped register locations
#define PLAYER_1_INPUT_REG 0x4016
#define PLAYER_2_INPUT_REG 0x4017

#define NES_BUTTON_A        0x01
#define NES_BUTTON_B        0x02
#define NES_BUTTON_SELECT   0x04
#define NES_BUTTON_START    0x08
#define NES_BUTTON_UP       0x10
#define NES_BUTTON_DOWN     0x20
#define NES_BUTTON_LEFT     0x40
#define NES_BUTTON_RIGHT    0x80

typedef struct CNTRL {
    uint8_t button_state;  // Stores which buttons are currently pressed
    uint8_t shift_reg;     // Shift register for reading input
    uint8_t strobe;        // Latch signal (0 or 1)
} CNTRL;

CNTRL *cntrl_init();
void cntrl_free(CNTRL *cntrl);
void cntrl1_handle_input(CNTRL *cntrl, SDL_Event *event);
void cntrl2_handle_input(CNTRL *cntrl, SDL_Event *event);
uint8_t cntrl_read(CNTRL *cntrl);
void cntrl_write(CNTRL *cntrl, uint8_t value);

#endif