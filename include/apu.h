#ifndef APU_H
#define APU_H

#include <stdint.h>

#define APU_SAMPLE_RATE 88000
#define CPU_CLOCK 1789773

typedef struct MEM MEM;
typedef struct PPU PPU;

typedef struct PulseChannel {
    // 0x4000 | 0x4004
    uint8_t duty; // duty cycle 
    int env_loop;
    int constant_vol;
    uint8_t volume;
    
    // 0x4001 | 0x4005
    int sweep_en;
    uint8_t period;
    int negate; 
    uint8_t shift; 

    // 0x4002 & 0x4003 | 0x4006 & 0x4007
    uint16_t timer; // frequency timer
    uint8_t length; // duration timer

    // state variables
    uint16_t timer_counter;
    uint8_t seq_pos;         
    uint8_t envelope_divider;
    uint8_t envelope_counter;
    uint8_t envelope_start;
    uint8_t sweep_divider;
    uint8_t sweep_reload;
    uint8_t sweep_mute;
    uint8_t length_counter;
    int16_t output; // holds the most recent output value for audio_callback
} PulseChannel;

typedef struct TriangleChannel {
    // 0x4008
    int counter_halt;
    uint8_t counter_value;

    // 0x400A & 0x400B
    uint16_t timer; // frequency timer
    uint8_t length; // duration timer

    // state variables
    uint16_t timer_counter;
    uint8_t seq_pos;        
    uint8_t linear_counter;
    uint8_t linear_reload;
    uint8_t length_counter;
    int16_t output; // holds the most recent output value for audio_callback
} TriangleChannel;

typedef struct NoiseChannel {
    // 0x400C
    int env_loop;
    int constant_vol;
    uint8_t volume;

    // 0x400E
    int mode; // LFSR mode
    uint16_t period; // frequency timer

    // 0x400F
    uint8_t length; // duration timer

    // state variables
    uint16_t timer_counter;
    uint16_t lfsr; // shift register
    uint8_t envelope_divider;
    uint8_t envelope_counter;
    uint8_t envelope_start;
    uint8_t length_counter;
    int16_t output; // holds the most recent output value for audio_callback
} NoiseChannel;

typedef struct APU {
    SDL_AudioDeviceID audio_dev;

    // 0x4015
    int DMC_en;
    int noise_en;
    int triangle_en;
    int pulse1_en;
    int pulse2_en;

    // 0x4017
    int mode;
    int IRQ_inhibit;

    int frame_counter; 

    PulseChannel pulse1;
    PulseChannel pulse2;
    TriangleChannel triangle;
    NoiseChannel noise;
} APU;

APU *apu_init();
void apu_free(APU *apu);
void apu_run_cycle(APU *apu);
uint8_t apu_register_read(APU *apu, uint16_t reg);
void apu_register_write(APU *apu, uint16_t reg, uint8_t value);

#endif