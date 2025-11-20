#include <SDL.h>
#include <stdint.h>
#include "../include/apu.h"
#include "../include/memory.h"

void audio_callback(void *userdata, Uint8 *stream, int len);
void pulse_channel(PulseChannel *ch);
void triangle_channel(TriangleChannel *ch);
void noise_channel(NoiseChannel *ch);

#define QUARTER_FRAME_CYCLES 7457
#define HALF_FRAME_CYCLES 14913

static int apu_quarter_frame = 0;
static int apu_half_frame = 0;

static const uint8_t pulse_length[32] = {
    10, 254, 20, 2, 40, 4, 80, 6,
    160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22,
    192, 24, 72, 26, 16, 28, 32, 30
};

static const uint8_t triangle_sequence[32] = {
    15, 14, 13, 12, 11, 10, 9, 8,
    7, 6, 5, 4, 3, 2, 1, 0,
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, 15
};

static const uint16_t noise_length[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 
    254, 380, 508, 762, 1016, 2034, 4068
};

static const uint8_t duty_patterns[4][8] = {
    {0,0,0,0,0,0,0,1}, // 12.5% duty cycle
    {0,0,0,0,0,1,1,1}, // 25% duty cycle
    {0,0,0,0,1,1,1,1}, // 50% duty cycle
    {1,1,1,1,1,1,0,0}  // 75% duty cycle
};

APU *apu_init() {
    APU *apu = (APU *)malloc(sizeof(APU));
    if (!apu) {
        fprintf(stderr, "Failed to allocate APU\n");
        exit(1);
    }

    // initialize all fields to zero
    memset(apu, 0, sizeof(APU));

    // initialize SDL audio
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "Failed to initialize SDL audio: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100; // sample rate
    want.format = AUDIO_S16SYS; // 16-bit signed audio
    want.channels = 1; 
    want.samples = 1024; // buffer size
    want.callback = audio_callback;
    want.userdata = apu;

    apu->audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!apu->audio_dev) {
        fprintf(stderr, "Failed to open audio device: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_PauseAudioDevice(apu->audio_dev, 0); // 0 = start playing

    return apu;
}

void audio_callback(void *userdata, Uint8 *stream, int len) {
    APU *apu = (APU *)userdata;
    if (!apu || len <= 0) {
        memset(stream, 0, len);
        return;
    }

    int16_t *buffer = (int16_t *)stream;
    int samples = len / sizeof(int16_t);

    double cycles_per_sample = (double)CPU_CLOCK / (double)APU_SAMPLE_RATE;
    static double cycle_accum = 0.0;

    for (int i = 0; i < samples; i++) {
        cycle_accum += cycles_per_sample;
        while (cycle_accum >= 1.0) {
            apu_run_cycle(apu);
            cycle_accum -= 1.0;
        }

        // simple linear mix of channels
        int32_t mixed = (int32_t)apu->pulse1.output / 2
                        + (int32_t)apu->pulse2.output / 2
                        + (int32_t)apu->triangle.output * 2
                        + (int32_t)apu->noise.output / 2;

        buffer[i] = (int16_t)mixed;
    }
}

void apu_run_cycle(APU *apu) {
    // update pulse channel 1
    PulseChannel *pulse1 = &apu->pulse1;
    pulse_channel(pulse1);

    // update pulse channel 2
    PulseChannel *pulse2 = &apu->pulse2;
    pulse_channel(pulse2);

    // update triangle channel
    TriangleChannel *triangle = &apu->triangle;
    triangle_channel(triangle);

    // update noise channel
    NoiseChannel *noise = &apu->noise;
    noise_channel(noise);

    apu_quarter_frame++;
    apu_half_frame++;
    int quarter_tick = 0;
    int half_tick = 0;
    if (apu_quarter_frame >= QUARTER_FRAME_CYCLES) {
        apu_quarter_frame -= QUARTER_FRAME_CYCLES;
        quarter_tick = 1;
    }
    if (apu_half_frame >= HALF_FRAME_CYCLES) {
        apu_half_frame -= HALF_FRAME_CYCLES;
        half_tick = 1;
    }

    // quarter frame updates (envelopes for pulse and noise channels)
    if (quarter_tick) {
        // pulse channels envelope
        for (int i = 0; i < 2; i++) {
            PulseChannel *ch = (i == 0) ? &apu->pulse1 : &apu->pulse2; // iterate over both pulse channels
            if (ch->envelope_start) {
                ch->envelope_counter = 15;
                ch->envelope_divider = ch->volume;
                ch->envelope_start = 0;
            } else {
                if (ch->envelope_divider == 0) {
                    if (ch->envelope_counter > 0) ch->envelope_counter--;
                    else if (ch->env_loop) ch->envelope_counter = 15;
                    ch->envelope_divider = ch->volume;
                } else {
                    ch->envelope_divider--;
                }
            }
        }

        // noise channel envelope
        NoiseChannel *nch = &apu->noise;
        if (nch->envelope_start) {
            nch->envelope_counter = 15;
            nch->envelope_divider = nch->volume;
            nch->envelope_start = 0;
        } else {
            if (nch->envelope_divider == 0) {
                if (nch->envelope_counter > 0) nch->envelope_counter--;
                else if (nch->env_loop) nch->envelope_counter = 15;
                nch->envelope_divider = nch->volume;
            } else {
                nch->envelope_divider--;
            }
        }
    }

    // half frame updates (length counters and sweep units for pulse, triangle, and noise channels)
    if (half_tick) {
        // pulse channels length counters and sweep units
        for (int i = 0; i < 2; i++) {
            PulseChannel *ch = (i == 0) ? &apu->pulse1 : &apu->pulse2;
            if (!ch->env_loop && ch->length_counter > 0) ch->length_counter--;
            // Sweep logic here, using ch->period as divider
            if (ch->sweep_reload) {
                ch->sweep_divider = ch->period;
                ch->sweep_reload = 0;
            } else if (ch->sweep_divider > 0) {
                ch->sweep_divider--;
            } else {
                ch->sweep_divider = ch->period;
                if (ch->sweep_en && ch->shift > 0 && ch->timer > 7) {
                    uint16_t delta = ch->timer >> ch->shift;
                    if (ch->negate) ch->timer -= delta;
                    else ch->timer += delta;
                }
            }
        }
        // noise channel length counter
        if (!apu->noise.env_loop && apu->noise.length_counter > 0)
            apu->noise.length_counter--;

        // triangle channel length counter
        if (!apu->triangle.counter_halt && apu->triangle.length_counter > 0)
            apu->triangle.length_counter--;
    }
}

void pulse_channel(PulseChannel *ch) {
    // timer and sequencer
    if (ch->timer_counter == 0) {
        ch->timer_counter = ch->timer + 1;
        ch->seq_pos = (ch->seq_pos + 1) & 0x07;
    } else {
        ch->timer_counter--;
    }

    // envelope (run at quarter-frame, every 7457 CPU cycles) 
    static int envelope_divider = 0;
    envelope_divider++;
    if (envelope_divider >= 7457) {
        envelope_divider = 0;
        if (ch->envelope_start) {
            ch->envelope_counter = 15;
            ch->envelope_divider = ch->volume;
            ch->envelope_start = 0;
        } else {
            if (ch->envelope_divider == 0) {
                if (ch->envelope_counter > 0) {
                    ch->envelope_counter--;
                } else if (ch->env_loop) {
                    ch->envelope_counter = 15;
                }
                ch->envelope_divider = ch->volume;
            } else {
                ch->envelope_divider--;
            }
        }
    }

    // length counter (run at half-frame, every 14913 CPU cycles)
    static int length_divider = 0;
    length_divider++;
    if (length_divider >= 14913) {
        length_divider = 0;
        if (!ch->env_loop && ch->length_counter > 0) {
            ch->length_counter--;
        }
    }

    // sweep unit (run at half-frame, every 14913 CPU cycles)
    static int sweep_divider = 0;
    sweep_divider++;
    if (sweep_divider >= 14913) {
        sweep_divider = 0;
        if (ch->sweep_reload) {
            ch->sweep_divider = ch->period;
            ch->sweep_reload = 0;
        } else if (ch->sweep_divider > 0) {
            ch->sweep_divider--;
        } else {
            ch->sweep_divider = ch->period;
            if (ch->sweep_en && ch->shift > 0 && ch->timer > 7) {
                uint16_t delta = ch->timer >> ch->shift;
                if (ch->negate) {
                    ch->timer -= delta;
                } else {
                    ch->timer += delta;
                }
            }
        }
    }

    // calculate output
    int duty = ch->duty & 0x03;
    int seq = ch->seq_pos & 0x07;
    int envelope = ch->constant_vol ? ch->volume : ch->envelope_counter;
    double amplitude = (envelope / 15.0) * 8000.0;
    if (apu->pulse1_en && ch->length_counter > 0 && ch->timer > 7) {
        ch->output = duty_patterns[duty][seq] ? amplitude : -amplitude;
    } else {
        ch->output = 0;
    }
}

void triangle_channel(TriangleChannel *ch) {
    // Timer and sequencer
    if (ch->timer_counter == 0) {
        ch->timer_counter = ch->timer + 1;
        // Only advance the 32-step sequencer if both linear and length counters permit output.
        // (Sequencer is clocked only when linear_counter > 0 AND length_counter > 0)
        if (ch->linear_counter > 0 && ch->length_counter > 0) {
            ch->seq_pos = (ch->seq_pos + 1) & 0x1F; // 32-step sequence
        }
    } else {
        ch->timer_counter--;
    }

    // Linear counter (run at quarter-frame, every 7457 CPU cycles)
    static int linear_divider = 0;
    linear_divider++;
    if (linear_divider >= 7457) {
        linear_divider = 0;
        // On linear-clock:
        if (ch->linear_reload) {
            ch->linear_counter = ch->counter_value;
        } else if (ch->linear_counter > 0) {
            ch->linear_counter--;
        }
        // If control flag (counter_halt) is clear -> clear reload flag
        if (!ch->counter_halt) ch->linear_reload = 0;
    }

    // Length counter (run at half-frame, every 14913 CPU cycles)
    static int length_divider = 0;
    length_divider++;
    if (length_divider >= 14913) {
        length_divider = 0;
        if (!ch->counter_halt && ch->length_counter > 0) {
            ch->length_counter--;
        }
    }

    // Calculate output
    if (apu->triangle_en && ch->length_counter > 0 && ch->linear_counter > 0 && ch->timer > 7) {
        ch->output = triangle_sequence[ch->seq_pos] * 256; // Scale to 16-bit range
    } else {
        ch->output = 0;
    }
}

void noise_channel(NoiseChannel *ch) {
    // Timer
    if (ch->timer_counter == 0) {
        ch->timer_counter = ch->period + 1;

        // LFSR shift
        uint16_t feedback;
        if (ch->mode) {
            feedback = ((ch->lfsr & 0x0001) ^ ((ch->lfsr >> 6) & 0x0001)) & 0x0001;
        } else {
            feedback = ((ch->lfsr & 0x0001) ^ ((ch->lfsr >> 1) & 0x0001)) & 0x0001;
        }
        ch->lfsr = (ch->lfsr >> 1) | (feedback << 14);
    } else {
        ch->timer_counter--;
    }

    // Envelope (run at quarter-frame, every 7457 CPU cycles)
    static int envelope_divider = 0;
    envelope_divider++;
    if (envelope_divider >= 7457) {
        envelope_divider = 0;
        if (ch->envelope_start) {
            ch->envelope_counter = 15;
            ch->envelope_divider = ch->volume;
            ch->envelope_start = 0;
        } else {
            if (ch->envelope_divider == 0) {
                if (ch->envelope_counter > 0) {
                    ch->envelope_counter--;
                } else if (ch->env_loop) {
                    ch->envelope_counter = 15;
                }
                ch->envelope_divider = ch->volume;
            } else {
                ch->envelope_divider--;
            }
        }
    }

    // Length counter (run at half-frame, every 14913 CPU cycles)
    static int length_divider = 0;
    length_divider++;
    if (length_divider >= 14913) {
        length_divider = 0;
        if (!ch->env_loop && ch->length_counter > 0) {
            ch->length_counter--;
        }
    }

    // Calculate output
    int envelope = ch->constant_vol ? ch->volume : ch->envelope_counter;
    double amplitude = (envelope / 15.0) * 8000.0;
    if (apu->noise_en && ch->length_counter > 0 && (ch->lfsr & 0x0001) == 0) {
        ch->output = amplitude;
    } else {
        ch->output = 0;
    }   
}

uint8_t apu_read(APU *apu, uint16_t reg) {
    switch (reg) {
        // 0x4015 is the only readable APU register
        case 0x4015: {
            uint8_t status = 0;
            status |= (apu->pulse1_en & 0x01) << 0;
            status |= (apu->pulse2_en & 0x01) << 1;
            status |= (apu->triangle_en & 0x01) << 2;
            status |= (apu->noise_en & 0x01) << 3;
            status |= (apu->DMC_en & 0x01) << 4;
            return status;
        }
        default:
            return 0;
    }
}

void apu_write(APU *apu, uint16_t reg, uint8_t value) {
    switch (reg) {
        // ======= PULSE 1 =======

        case 0x4000: {
            apu->pulse1.duty = (value >> 6) & 0x03;
            apu->pulse1.env_loop = (value >> 5) & 0x01;
            apu->pulse1.constant_vol = (value >> 4) & 0x01;
            apu->pulse1.volume = value & 0x0F;
            apu->pulse1.envelope_start = 1; // envelope restart on write
            break;
        }
        case 0x4001: {
            apu->pulse1.sweep_en = (value >> 7) & 0x01;
            apu->pulse1.period = (value >> 4) & 0x07;
            apu->pulse1.negate = (value >> 3) & 0x01;
            apu->pulse1.shift = value & 0x07;
            apu->pulse1.sweep_reload = 1; // sweep reload on write
            break;
        }
        case 0x4002: {
            apu->pulse1.timer = (apu->pulse1.timer & 0x0700) | value;
            break;
        }
        case 0x4003: {
            apu->pulse1.timer = ((value & 0x07) << 8) | (apu->pulse1.timer & 0x00FF);
            apu->pulse1.length_counter = pulse_length[(value >> 3) & 0x1F];
            apu->pulse1.seq_pos = 0;
            apu->pulse1.envelope_start = 1;
            apu->pulse1.timer_counter = apu->pulse1.timer + 1;
            break;
        }

        // ======= PULSE 2 =======

        case 0x4004: { 
            apu->pulse2.duty = (value >> 6) & 0x03;
            apu->pulse2.env_loop = (value >> 5) & 0x01;
            apu->pulse2.constant_vol = (value >> 4) & 0x01;
            apu->pulse2.volume = value & 0x0F;
            apu->pulse2.envelope_start = 1;
            break;
        }
        case 0x4005: { 
            apu->pulse2.sweep_en = (value >> 7) & 0x01;
            apu->pulse2.period = (value >> 4) & 0x07;
            apu->pulse2.negate = (value >> 3) & 0x01;
            apu->pulse2.shift = value & 0x07;
            apu->pulse2.sweep_reload = 1;
            break;
        }
        case 0x4006: { 
            apu->pulse2.timer = (apu->pulse2.timer & 0x0700) | value;
            break;
        }
        case 0x4007: { 
            apu->pulse2.timer = ((value & 0x07) << 8) | (apu->pulse2.timer & 0x00FF);
            apu->pulse2.length_counter = pulse_length[(value >> 3) & 0x1F];
            apu->pulse2.seq_pos = 0;
            apu->pulse2.envelope_start = 1;
            apu->pulse2.timer_counter = apu->pulse2.timer + 1;
            break;
        }

        // ======= TRIANGLE =======

        case 0x4008: {
            apu->triangle.counter_halt = (value >> 7) & 0x01;
            apu->triangle.counter_value = value & 0x7F;
            apu->triangle.linear_reload = 1; // Set reload flag
            break;
        }
        // 0x4009 is unused
        case 0x400A: {
            apu->triangle.timer = (apu->triangle.timer & 0x0700) | value; 
            break;
        }
        case 0x400B: {
            apu->triangle.timer = ((value & 0x07) << 8) | (apu->triangle.timer & 0x00FF);
            // load length counter from length table (same as pulses)
            apu->triangle.length_counter = pulse_length[(value >> 3) & 0x1F];
            apu->triangle.seq_pos = 0;
            // side-effect: set linear counter reload flag (actual reload handled on quarter-frame)
            apu->triangle.linear_reload = 1;
             break;
        }

        // ======= NOISE =======
        case 0x400C: {
            apu->noise.env_loop = (value >> 5) & 0x01;
            apu->noise.constant_vol = (value >> 4) & 0x01;
            apu->noise.volume = value & 0x0F;
            apu->noise.envelope_start = 1;
            break;
        }
        // 0x400D is unused
        case 0x400E: {
            apu->noise.mode = (value >> 7) & 0x01;
            apu->noise.period = noise_length[(value & 0x0F)];
            break;
        }
        case 0x400F: {
            apu->noise.length_counter = pulse_length[(value >> 3) & 0x1F]; // Set length counter
            apu->noise.envelope_start = 1; // Restart envelope
            apu->noise.lfsr = 1; // Reset LFSR
            apu->noise.timer_counter = apu->noise.period + 1; // Reset timer
            break;
        }

        // ======= APU STATUS =======
        
        case 0x4015: {
            apu->pulse1_en = value & 0x01;
            apu->pulse2_en = (value >> 1) & 0x01;
            apu->triangle_en = (value >> 2) & 0x01;
            apu->noise_en = (value >> 3) & 0x01;
            apu->DMC_en = (value >> 4) & 0x01;

            if (apu->DMC_en) {
                printf("DMC channel not implemented yet\n");
            }

            // clear length counters if channels are disabled
            if (!apu->pulse1_en) apu->pulse1.length_counter = 0;
            if (!apu->pulse2_en) apu->pulse2.length_counter = 0;
            if (!apu->triangle_en) apu->triangle.length_counter = 0;
            if (!apu->noise_en) apu->noise.length_counter = 0;
            break;
        }
        case 0x4017: {
            apu->mode = (value >> 7) & 0x01;
            apu->IRQ_inhibit = (value >> 6) & 0x01;
            if (apu->IRQ_inhibit) {
                // clear IRQ flag
                // TODO: Implement IRQ flag clearing logic
            }
            break;
        }
        default:
            break;
    }
}

void apu_free(APU *apu) {
    SDL_CloseAudioDevice(apu->audio_dev);
    free(apu);
}