/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "pico/stdio.h"
#include "pico/sem.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include <math.h>

#include "config.h"


// dir == 1 ? "(forward)" : dir ? "(backward)" : "(still)" dir = [-1, 0, 1]
static uint32_t t = 0;
static int dir = 1;

// horrible temporary hack to avoid changing pattern code
static uint8_t *current_strip_out;
// static bool current_strip_4color;

// Write framebuffer channels in R, G, B, warm white, cold white order.
// The output driver applies CHANNEL_WIRE_ORDER when transmitting.
static inline void put_pixel(uint8_t r, uint8_t g, uint8_t b,
                             uint8_t warm, uint8_t cold) {
    *current_strip_out++ = r;
    *current_strip_out++ = g;
    *current_strip_out++ = b;
#if NUM_CHANNELS >= 4
    *current_strip_out++ = warm;
#else
    (void)warm;
#endif
#if NUM_CHANNELS >= 5
    *current_strip_out++ = cold;
#else
    (void)cold;
#endif
}

/**
 * Oryginally was in order GRB
 */
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return 
            ((uint32_t) (r) << 16) |
            ((uint32_t) (g) << 8) |
            (uint32_t) (b);
}

// --- Brightness conversion tables for subpixel ---
static uint8_t linear_brightness_percent[100];
static uint8_t linear_brightness_percept[256];

// Simple (float) formulas for input as percentage 0–100%
static inline uint8_t percent_to_u8(uint8_t p) {
    if (!p) return 0;
    const float gamma = 2.2f;              // try 2.0..2.4; tune per project/LEDs
    float lin = (float)p / 100.0f;
    int v = (int)(255.0f * powf(lin, gamma) + 0.5f);
    return (uint8_t)(v > 255 ? 255 : v);
}

// When input is 0–255 “perceptual” units (e.g., 0–256 scaled)
static inline uint8_t percept_to_u8(uint8_t x) {
    if (!x) return 0;
    const float gamma = 2.2f;
    float lin = (float)x / 255.0f;
    int v = (int)(255.0f * powf(lin, gamma) + 0.5f);
    return (uint8_t)(v > 255 ? 255 : v);
}

// No-float, super-cheap (γ≈2)
// Map 0..255 perceptual -> 0..255 LED
static inline uint8_t gamma2_u8(uint8_t x) {
    uint16_t y = (uint16_t)x * (uint16_t)x;     // 0..65025
    return (uint8_t)((y + 127) / 255);          // round to 0..255
}
// If you start from 0..100%:
static inline uint8_t gamma2_percent(uint8_t p) {
    uint16_t x = (uint16_t)(p * 255 / 100);       // scale to 0..255
    return gamma2_u8((uint8_t)x);
}

// // Best quality, still small: LUT (γ=2.2)
// static uint8_t gamma22_lut[256];
// static void build_gamma22_lut(void) {
//     const float gamma = 2.2f;
//     for (int i = 0; i < 256; ++i) {
//         float lin = (float)i / 255.0f;
//         int v = (int)(255.0f * powf(lin, gamma) + 0.5f);
//         gamma22_lut[i] = (uint8_t)(v > 255 ? 255 : v);
//     }
// }

// typedef void (*pattern)(uint8_t *buffer, uint strips, uint pixels);

static uint32_t start_strip_pos[NUM_STRIPS];  // random start positions for patterns
static uint32_t start_strip_color[NUM_STRIPS];  // random color with random brightness
static uint32_t start_column_pos[NUM_PIXELS];  // random start positions for patterns
static uint8_t start_column_sel_color[NUM_PIXELS];  // random color selection for columns


void init_start_strips(void) {
    uint16_t y, x;
    uint8_t color, r, g, b, val;

    for (y = 0; y < NUM_STRIPS; ++y) {
        start_strip_pos[y] = (uint32_t)(rand() % NUM_PIXELS);
    }
    for (y = 0; y < NUM_STRIPS; ++y) {
        color = (typeof(color))(rand() % 7);
        val = (typeof(val))(rand() % 256);
        r = (color & 0x4) ? val : 0;
        g = (color & 0x2) ? val : 0;
        b = (color & 0x1) ? val : 0;

        start_strip_color[y] = urgb_u32(r, g, b);
    }
    for (x = 0; x < NUM_PIXELS; ++x) {
        start_column_pos[x] = (uint32_t)(rand() % NUM_STRIPS);
    }
    for (x = 0; x < NUM_PIXELS; ++x) {
        start_column_sel_color[x] = (uint8_t)(rand() % 8);
    }

    for (y = 0; y < sizeof(linear_brightness_percent); y++) {
        linear_brightness_percent[y] = percent_to_u8((uint8_t)y);
    }
    for (y = 0; y < sizeof(linear_brightness_percept); y++) {
        linear_brightness_percept[y] = percept_to_u8((uint8_t)y);
    }
}  

// Toggle one channel uniformly across the frame, preserving the other channels.
// The first pixel determines whether the selected channel is currently on.
static void toggle_channel(uint8_t *buffer, uint strips, uint pixels, uint channel) {
    if (strips == 0 || pixels == 0)
        return;

    uint8_t value = buffer[channel] ? 0 : 0xff;
    for (uint y = 0; y < strips; ++y) {
        for (uint x = 0; x < pixels; ++x)
            buffer[(y * pixels + x) * NUM_CHANNELS + channel] = value;
    }
}

void pattern_red(uint8_t *buffer, uint strips, uint pixels) {
    toggle_channel(buffer, strips, pixels, 0);
}

void pattern_green(uint8_t *buffer, uint strips, uint pixels) {
    toggle_channel(buffer, strips, pixels, 1);
}

void pattern_blue(uint8_t *buffer, uint strips, uint pixels) {
    toggle_channel(buffer, strips, pixels, 2);
}

#if NUM_CHANNELS >= 4
void pattern_warm(uint8_t *buffer, uint strips, uint pixels) {
    toggle_channel(buffer, strips, pixels, 3);
}
#endif

#if NUM_CHANNELS >= 5
void pattern_cold(uint8_t *buffer, uint strips, uint pixels) {
    toggle_channel(buffer, strips, pixels, 4);
}
#endif

void pattern_snakes(uint8_t *buffer, uint strips, uint pixels) {
    uint16_t y, x;
    uint pos;

    for ( y = 0; y < strips; ++y) {
        pos = start_strip_pos[y];
        current_strip_out = buffer + y * pixels * NUM_CHANNELS;
        for (uint i = 0; i < pixels; ++i) {
            uint tmp = (uint)(t >> 1);
            x = (typeof(x))((i + pos + tmp) % 64);
            if (x < 10)
                put_pixel(0xff, 0, 0, 0, 0);
            else if (x >= 15 && x < 25)
                put_pixel(0, 0xff, 0, 0, 0);
            else if (x >= 30 && x < 40)
                put_pixel(0, 0, 0xff, 0, 0);
            else
                put_pixel(0, 0, 0, 0, 0);
        }
    }
    int64_t tmp = (int64_t)t + (int64_t)dir;
    t = (typeof(t))tmp;
}


void pattern_random(uint8_t *buffer, uint strips, uint pixels) {
    uint16_t y;
    uint8_t color, r, g, b, warm, cold, val;

    // t += dir;
    if (++t % 8)    // 20ms * 8 = 160ms
        return;

     for ( y = 0; y < strips; ++y) {
        current_strip_out = buffer + y * pixels * NUM_CHANNELS;
        for (uint i = 0; i < pixels; ++i) {
            color = (typeof(color))(rand() % 0x20);
            val = (typeof(val))(rand() % 256);
            r = (color & 0x4) ? val : 0;
            g = (color & 0x2) ? val : 0;
            b = (color & 0x1) ? val : 0;
            warm = (color & 0x8) ? val : 0;
            cold = (color & 0x10) ? val : 0;

            put_pixel(r, g, b, warm, cold);
        }
    }
}

void pattern_sparkle(uint8_t *buffer, uint strips, uint pixels) {
    static uint8_t flip = 0;
    if (++t % 2)
        return;

    uint y;
    for ( y = 0; y < strips; ++y) {
        current_strip_out = buffer + y * pixels * NUM_CHANNELS;
        for (uint i = 0; i < pixels; ++i) {
            if (!flip && rand() % 32 == 0) {
                put_pixel(0xff, 0xff, 0xff, 0xff, 0xff);
            } else {
                put_pixel(0, 0, 0, 0, 0);
            }
        }
    }
    if (!flip)
        flip = 3;
    else
        flip--;
    // t += dir;
}

void pattern_drop1(uint8_t *buffer, uint strips, uint pixels) {
    //uint max = 100; // let's not draw too much current!
    //t %= max;
    //start_column_pos[NUM_PIXELS]
    uint16_t x, y;
    uint8_t sel, c, r, g, b;
    uint32_t pos;
    uint a;

    for ( x = 0; x < pixels; ++x) {
        pos = start_column_pos[x];
        sel = start_column_sel_color[x];

        for ( y = 0; y < strips; ++y) {
            current_strip_out = buffer + (y * pixels * NUM_CHANNELS) + (x * NUM_CHANNELS);
            // c = (typeof(c))((y + pos + (t >> 1)) % 26);    // changes position every 2 frames (40ms)
            a = y + pos + (t >> 1);
            c = (typeof(c))(a % 26);
            
            if (c > 3)
                c -= 3;
            else
                c = 0;
            c *= 10;
            r = g = b = 0;
            if (sel & 0x04)
                r = c;
            if (sel & 0x02)
                g = c;
            if (sel & 0x01)
                b = c;
            put_pixel(r, g, b, 0, 0);
        }
    }

    // t += dir;
    int64_t tmp = (int64_t)t + (int64_t)dir;
    t = (typeof(t))tmp;
}

#define STRIP_LED_RANGE 15
#define STRIP_LED_FACTOR (240 / STRIP_LED_RANGE)
void pattern_solid(uint8_t *buffer, uint strips, uint pixels) {
    uint16_t y, x, len;
    uint8_t c, r, g, b, select;
    
    for (y = 0; y < strips; y++) {
        current_strip_out = buffer + y * pixels * NUM_CHANNELS;
        if (y < 14)
            len = (typeof(len))(pixels - 2);   // 55 leds
        else
            len = (typeof(len))pixels;

        for (x = 0; x < pixels; ++x) {
            if (x < STRIP_LED_RANGE) {
                // c = (STRIP_LED_RANGE - x) << 3;
                c = (typeof(c))((STRIP_LED_RANGE - x) * STRIP_LED_FACTOR);
            } else if ((x <= len) && ((len - x) < STRIP_LED_RANGE)) {
                // c = (STRIP_LED_RANGE + x - len) << 3;
                c = (typeof(c))((STRIP_LED_RANGE + x - len) * STRIP_LED_FACTOR);
            } else {
                c = 0;
            }
            r = g = b = 0;
            select = (typeof(select))((y % 7) + 1);
            if (select & 0x04)
                r = c;
            if (select & 0x02)
                g = c;
            if (select & 0x01)
                b = c;
            put_pixel(r, g, b, 0, 0);
        }
    }
    // t += dir;
    int64_t tmp = (int64_t)t + (int64_t)dir;
    t = (typeof(t))tmp;
}

#ifdef USE_TABLE_JAREMEK
uint32_t jaremektable[] = {
        0x00F000, 0xF00000, 0x0000F0, 0xF0F0F0, 0x000000, 0x888800, 0x008888, 0x000000
};
    uint max = count_of(jaremektable);
    if (!(t % 6)) {
        for (y = 0; y < strips; y++) {
            current_strip_out = buffer + y * pixels * NUM_CHANNELS;
            for(x = 0; x < pixels; x++) {
                uint32_t rgb = jaremektable[(t + x + (y < 2)) % max];
                put_pixel((uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8),
                          (uint8_t)rgb, 0, 0);
            }
        }
    }
#endif

#define STRIP_RAISE_PX 16
// #define LINEAR_TABLE linear_brightness_percent
#define LINEAR_TABLE linear_brightness_percept
void pattern_jaremek(uint8_t *buffer, uint strips, uint pixels) {
    uint16_t y, x, mul, max, select;
    uint8_t c, r, g, b;

    max = count_of(LINEAR_TABLE);
    mul = max / STRIP_RAISE_PX;
    // if (!(t % 6)) {
    for (y = 0; y < strips; y++) {
        current_strip_out = buffer + y * pixels * NUM_CHANNELS;
        for(x = 0; x < pixels; x++) {
            // linear on every strip for one color
            select = (typeof(select))((x * mul + t) % max);
            c = LINEAR_TABLE[select];

            r = g = b = 0;
            select = (typeof(select))((y % 7) + 1);
            if (select & 0x04)
                r = c;
            if (select & 0x02)
                g = c;
            if (select & 0x01)
                b = c;

            put_pixel(r, g, b, 0, 0);
        }
    }
    // }
    // t += dir;
    int64_t tmp = (int64_t)t + (int64_t)dir;
    t = (typeof(t))tmp;
}

