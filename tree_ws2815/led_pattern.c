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

#include "config_tree.h"


// dir == 1 ? "(forward)" : dir ? "(backward)" : "(still)" dir = [-1, 0, 1]
static int t = 0;

// // horrible temporary hack to avoid changing pattern code
// static uint8_t *current_strip_out;
// // static bool current_strip_4color;

// static inline void put_pixel(uint32_t pixel_grb) {
//     *current_strip_out++ = (pixel_grb >> 16u) & 0xffu;
//     *current_strip_out++ = (pixel_grb >> 8u) & 0xffu;
//     *current_strip_out++ = pixel_grb & 0xffu;
// }


/**
 * Oryginally was in order GRB
 */
// static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
//     return 
//             ((uint32_t) (r) << 16) |
//             ((uint32_t) (g) << 8) |
//             (uint32_t) (b);
// }
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return 
            ((uint32_t) (r) << 24) |
            ((uint32_t) (g) << 16) |
            ((uint32_t) (b) << 8);
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
    uint16_t x = (uint16_t)p * 255 / 100;       // scale to 0..255
    return gamma2_u8((uint8_t)x);
}

// Best quality, still small: LUT (γ=2.2)
static uint8_t gamma22_lut[256];
static void build_gamma22_lut(void) {
    const float gamma = 2.2f;
    for (int i = 0; i < 256; ++i) {
        float lin = (float)i / 255.0f;
        int v = (int)(255.0f * powf(lin, gamma) + 0.5f);
        gamma22_lut[i] = (uint8_t)(v > 255 ? 255 : v);
    }
}

// typedef void (*pattern)(uint8_t *buffer, uint strips, uint pixels);

static uint32_t start_strip_pos[NUM_STRIPS];  // random start positions for patterns
static uint32_t start_strip_color[NUM_STRIPS];  // random color with random brightness
static uint32_t start_column_pos[NUM_PIXELS];  // random start positions for patterns
static uint8_t start_column_sel_color[NUM_PIXELS];  // random color selection for columns

static void init_ornaments(void);

void init_start_strips(void) {
    uint32_t y, x;
    uint8_t color, r, g, b, val;

    for (y = 0; y < NUM_STRIPS; ++y) {
        start_strip_pos[y] = (rand() % NUM_PIXELS);
    }
    for (y = 0; y < NUM_STRIPS; ++y) {
        color = rand() % 7;
        val = rand() % 256;
        r = (color & 0x4) ? val : 0;
        g = (color & 0x2) ? val : 0;
        b = (color & 0x1) ? val : 0;

        start_strip_color[y] = urgb_u32(r, g, b);
    }
    for (x = 0; x < NUM_PIXELS; ++x) {
        start_column_pos[x] = rand() % NUM_STRIPS;
    }
    for (x = 0; x < NUM_PIXELS; ++x) {
        start_column_sel_color[x] = rand() % 8;
    }

    for (y = 0; y < sizeof(linear_brightness_percent); y++) {
        linear_brightness_percent[y] = percent_to_u8(y);
    }
    for (y = 0; y < sizeof(linear_brightness_percept); y++) {
        linear_brightness_percept[y] = percept_to_u8(y);
    }

    init_ornaments();
}

// void pattern_simple(uint32_t *buffer, uint8_t *rgb, uint32_t pixels) {
//     static uint8_t hue = 0;
//     uint32_t color;


//     color = urgb_u32(rgb[0], rgb[1], rgb[2]);

//     for (uint i = 0; i < NUM_PIXELS; i++) {
//         if (i < pixels)
//             buffer[i] = color;  // ws2815_buf[i] = color;
//         else
//             buffer[i] = 0;      // ws2815_buf[i] = 0;
//     }

//     hue += 1; // Increment hue for next frame
// }

#define COLOR_BEGIN 1
#define COLOR_LAST 6
void pattern_snakes1(uint32_t *buffer, uint32_t pixels, int dir) {
    uint a, x, y;
    uint i, snake_len, snake_tail;
    uint8_t r, g, b, max_r, max_g, max_b;
    static uint32_t t;
    static int8_t color_first = 1;
    int8_t color;



    max_r = 248;
    max_g = 230;
    max_b = 254;
    snake_len = 14; // 13
    snake_tail = 8; // 7

    x = (t >> 1) % snake_len;

    if (!(t & 1) && (dir > 0) && (x == 0)) {
        if (++color_first > COLOR_LAST)
            color_first = COLOR_BEGIN;
    }
    else if ((t & 1) && (dir < 0) && (x == (snake_len - 1))) { // x == 0
        if (++color_first > COLOR_LAST)
            color_first = COLOR_BEGIN;
    }

    color = color_first;
    printf("Snake1 start color=%d  x=%d\r\n", color, x);
    for (i = 0; i < NUM_PIXELS; ++i) {
        if (i >= pixels) {
            *buffer++ = 0;
            continue;
        }

        a = (i + (t >> 1));
        if (y != (a / snake_len)) {
            y++;
        }
        x = a % snake_len;     // % 64;

        if (x == 0) {
            r = max_r;
            g = max_g;
            b = max_b;
        } else if (x < snake_tail) {
            r = max_r >> x;
            g = max_g >> x;
            b = max_b >> x;
            r = r ? r : 1;
            g = g ? g : 1;
            b = b ? b : 1;
        } else {
            r = 0; g = 0; b = 0;
        }

        if (i < 18) {
            printf("    color=%d  x=%d  y=%d  i=%d\r\n", color, x, y, i);
        }

        r = (color & 0x4)? 0:r;
        g = (color & 0x2)? 0:g;
        b = (color & 0x1)? 0:b;

        *buffer++ = urgb_u32(r, g, b);


        if ((dir > 0) && (x == (snake_len - 1))) {
            if (++color > COLOR_LAST)
                color = COLOR_BEGIN;
            if (i <= (2*snake_len))
                printf("         change1 color=%d  x=%d  i=%d\r\n", color, x, i);
        }
        else if ((dir < 0) && (x == (snake_len - 1))) {
            if (--color < COLOR_BEGIN)
                color = COLOR_LAST;
            if (i <= (2*snake_len))
                printf("         change2 color=%d  x=%d  i=%d\r\n", color, x, i);
        }
    }

    t += dir;
}

/**
 * kiepski, do wykasowania
 */
void pattern_snakes2(uint32_t *buffer, uint32_t pixels, int dir) {
    uint a, x, pos = 0;
    uint i, snake_len, snake_tail[5];
    uint8_t r, g, b, max_r, max_g, max_b;
    static uint32_t t;

    max_r = 255;
    max_g = 255;
    max_b = 255;
    snake_len = 50;
    snake_tail[0] = 10;
    snake_tail[1] = 15;
    snake_tail[2] = 25;
    snake_tail[3] = 30;
    snake_tail[4] = 40;

    for (i = 0; i < NUM_PIXELS; ++i) {
        if (i >= pixels) {
            *buffer++ = 0;
            continue;
        }
        r = 0; g = 0; b = 0;
        a = (i + pos + (t >> 1));
        x = a % snake_len;     // % 64;
        if (x < snake_tail[0]) {
            r = max_r;
        } else if (x >= snake_tail[1] && x < snake_tail[2]) {
            g = max_g;
        } else if (x >= snake_tail[3] && x < snake_tail[4]) {
            b = max_b;
        } else {
            r = max_r;
            g = max_g;
            b = max_b;
        }

        *buffer++ = urgb_u32(r, g, b);
    }

    t += dir;
}

void pattern_snakes3(uint32_t *buffer, uint32_t pixels, int dir) {
    uint a, x;
    uint i, snake_len, snake_tail;
    uint8_t r, g, b, max_r, max_g, max_b;
    static uint32_t t;

    static int8_t color_first = 1;
    int8_t color;

    max_r = 255;
    max_g = 255;
    max_b = 255;

    snake_len = 28;
    snake_tail = 20;


    x = t % snake_len;
    if ((dir > 0) && (x == 0)) {
        if (++color_first > COLOR_LAST)
            color_first = COLOR_BEGIN;
    }
    else if ((dir < 0) && (x == (snake_len - 1))) { // x == 0
        if (++color_first > COLOR_LAST)
            color_first = COLOR_BEGIN;
    }
    color = color_first;

    for (i = 0; i < NUM_PIXELS; ++i) {
        if (i >= pixels) {
            *buffer++ = 0;
            continue;
        }

        a = (i + t);

        // y = a / snake_len;
        x = a % snake_len;     // % 64;

        if (x == 0) {
            r = 1;
            g = 1;
            b = 1;
        }
        else if (x == 1) {
            r = max_r;
            g = max_g;
            b = max_b;
        } 
        else if (x < snake_tail) {
            r = (color & 1)? max_r / x : 2;
            g = (color & 2)? max_g / x : 2;
            b = (color & 4)? max_b / x : 2;
        } else {
            r = 0; g = 0; b = 0;
        }

        *buffer++ = urgb_u32(r, g, b);

        if ((dir > 0) && (x == (snake_len - 1))) {
            if (++color > COLOR_LAST)
                color = COLOR_BEGIN;
            if (i <= (2*snake_len))
                printf("         change1 color=%d  x=%d  i=%d\r\n", color, x, i);
        }
        else if ((dir < 0) && (x == (snake_len - 1))) {
            if (--color < COLOR_BEGIN)
                color = COLOR_LAST;
            if (i <= (2*snake_len))
                printf("         change2 color=%d  x=%d  i=%d\r\n", color, x, i);
        }
    }

    t += dir;
}

uint8_t get_val_perc(uint8_t level) {
    uint8_t size = sizeof(linear_brightness_percent);
    if (level >= size)
        level = size - 1;
    return linear_brightness_percent[level];
}

void pattern_snakes4(uint32_t *buffer, uint32_t pixels, int dir) {
    uint a, x;  // , y;
    uint i, snake_len, snake_tail;
    uint8_t r, g, b;
    uint8_t red, green, blue;
    static uint32_t t;
 
    static int8_t color_first = 1;
    int8_t color;

    // max = 99;

    snake_len = 50;
    snake_tail = 39;

    x = t % snake_len;
    if ((dir > 0) && (x == 0)) {
        if (++color_first > COLOR_LAST)
            color_first = COLOR_BEGIN;
    }
    else if ((dir < 0) && (x == (snake_len - 1))) { // x == 0
        if (++color_first > COLOR_LAST)
            color_first = COLOR_BEGIN;
    }
    color = color_first;

    for (i = 0; i < NUM_PIXELS; ++i) {
        if (i >= pixels) {
            *buffer++ = 0;
            continue;
        }

        a = (i + t);
        //y = a / snake_len;
        x = a % snake_len;
 
        if (x == 0) {
            r = (color & 1)? 5 : 10;
            g = (color & 2)? 5 : 10;
            b = (color & 4)? 5 : 10;
            red = get_val_perc(r);
            green = get_val_perc(g);
            blue = get_val_perc(b);
        } else if (x < snake_tail) {
            r += (color & 2)? 1 : 3;
            g += (color & 4)? 1 : 3;
            b += (color & 1)? 1 : 3;
            red = get_val_perc(r);
            green = get_val_perc(g);
            blue = get_val_perc(b);
        } else {
            red = 0; green = 0; blue = 0;
        }

        *buffer++ = urgb_u32(red, green, blue);

        if ((dir > 0) && (x == (snake_len - 1))) {
            if (++color > COLOR_LAST)
                color = COLOR_BEGIN;
            if (i <= (2*snake_len))
                printf("         change1 color=%d  x=%d  i=%d\r\n", color, x, i);
        }
        else if ((dir < 0) && (x == (snake_len - 1))) {
            if (--color < COLOR_BEGIN)
                color = COLOR_LAST;
            if (i <= (2*snake_len))
                printf("         change2 color=%d  x=%d  i=%d\r\n", color, x, i);
        }
    }

    t += dir;
}


void pattern_snakes5(uint32_t *buffer, uint32_t pixels, int dir) {
    uint a, x;
    uint i, snake_len; // , snake_tail;
    uint8_t r, g, b, max;
    uint8_t red, green, blue;
    static uint32_t t;
    static int8_t color_first = 1;
    int8_t color;

    max = 99;

    snake_len = max / 2;

    x = t % snake_len;
    if ((dir > 0) && (x == 0)) {
        if (++color_first > COLOR_LAST)
            color_first = COLOR_BEGIN;
    }
    else if ((dir < 0) && (x == (snake_len - 1))) { // x == 0
        if (++color_first > COLOR_LAST)
            color_first = COLOR_BEGIN;
    }
    color = color_first;

    for (i = 0; i < NUM_PIXELS; ++i) {
        if (i >= pixels) {
            *buffer++ = 0;
            continue;
        }

        // a = (i + pos + (t >> 1));
        a = (i + t);
        // b = t & 1;
        //y = a / snake_len;
        x = a % snake_len;     // % 64;

        if (x == 0) {
            switch (color) {
                case 1:
                    r = 0;
                    g = snake_len / 2;
                    b = snake_len / 2;
                    break;
                case 2:
                    r = 2;
                    g = snake_len;
                    b = 2;
                    break;
                case 3:
                    r = snake_len / 2;
                    g = snake_len / 2;
                    b = 0;
                    break;
                case 4:
                    r = 2;
                    g = 2;
                    b = snake_len;
                    break;
                case 5:
                    r = snake_len / 2;
                    g = 0;
                    b = snake_len / 2;
                    break;
                case 6:
                    r = snake_len;
                    g = 2;
                    b = 2;
                    break;
                default:
                    r = g = b = 20;
            }
            red = get_val_perc(r);
            green = get_val_perc(g);
            blue = get_val_perc(b);
        } else {
            r += 2;
            g += 2;
            b += 2;
            red = get_val_perc(r);
            green = get_val_perc(g);
            blue = get_val_perc(b);
        }

        *buffer++ = urgb_u32(red, green, blue);

        if ((dir > 0) && (x == (snake_len - 1))) {
            if (++color > COLOR_LAST)
                color = COLOR_BEGIN;
            if (i <= (2*snake_len))
                printf("         change1 color=%d  x=%d  i=%d\r\n", color, x, i);
        }
        else if ((dir < 0) && (x == (snake_len - 1))) {
            if (--color < COLOR_BEGIN)
                color = COLOR_LAST;
            if (i <= (2*snake_len))
                printf("         change2 color=%d  x=%d  i=%d\r\n", color, x, i);
        }
    }

    t += dir;
}

/**
 * Solid color with breathing (classic Christmas “soft glow”)
 * Variants:
 * (level, level, level) → white
 * (level, level / 4, 0) → warm white
 */
void pattern_breath(uint32_t *buffer, uint32_t pixels, int dir) {
    uint32_t i;
    uint g, b;
    uint8_t level = (t & 0xFF);
    if (level > 127) level = 255 - level;   // triangle wave
    if (level < 3) {
        level = 3;
        t += dir;
    }

    g = level * 2 / 3;
    b = level * 3 / 4;

    for (i = 0; i < pixels; ++i) {
        // *buffer++ = urgb_u32(level, 0, 0);  // red breathing
        *buffer++ = urgb_u32(level, g, b);  // warm white
    }

    t += dir;
}

/**
 * Moving rainbow (most popular LED effect)
 * Fast integer HSV → RGB approximation (no floats):
 * 
 */
static inline uint32_t hsv_wheel(uint8_t pos) {
    pos = 255 - pos;
    if (pos < 85)
        return urgb_u32(255 - pos * 3, 0, pos * 3);
    if (pos < 170) {
        pos -= 85;
        return urgb_u32(0, pos * 3, 255 - pos * 3);
    }
    pos -= 170;
    return urgb_u32(pos * 3, 255 - pos * 3, 0);
}

void pattern_rainbow(uint32_t *buffer, uint32_t pixels, int dir) {
    for (uint32_t i = 0; i < pixels; ++i) {
        uint8_t hue = (i * 256 / pixels + t) & 0xFF;
        *buffer++ = hsv_wheel(hue);
    }
    t += dir;
}


/**
 * Color wipe (holiday classic)
 * 
 * Variants:
 * red → green → blue (cycle t / pixels % 3)
 * warm white wipe
 */
void pattern_color_wipe(uint32_t *buffer, uint32_t pixels, int dir) {
    uint32_t pos = t % pixels;

    for (uint32_t i = 0; i < pixels; ++i) {
        if (i < pos)
            *buffer++ = urgb_u32(0, 255, 0);  // green
        else
            *buffer++ = 0;
    }

    t += dir;
}

/**
 * Twinkle (very Christmas-like)
 * 
 * No RNG required, deterministic pseudo-twinkle:
 * Very low CPU cost
 * ✔ Looks random but repeatable
 */
void pattern_twinkle(uint32_t *buffer, uint32_t pixels, int dir) {
    for (uint32_t i = 0; i < pixels; ++i) {
        uint32_t v = (i * 37 + t * 13) & 0xFF;
        if (v < 8)
            *buffer++ = urgb_u32(255, 255, 255);
        else
            *buffer++ = 0;
    }
    t += dir;
}

/**
 * Chase (Larson scanner / Knight Rider style)
 * 
 * Variants:
 * red → green → blue cycling
 * mirrored chase
 */
void pattern_chase(uint32_t *buffer, uint32_t pixels, int dir) {
    int pos = t % pixels;

    for (uint32_t i = 0; i < pixels; ++i) {
        int d = i - pos;
        if (d < 0) d = -d;

        uint8_t v = (d < 10) ? (255 - d * 25) : 0;
        *buffer++ = urgb_u32(v, 0, 0);
    }

    t += dir;
}

/**
 * Sparkle overlay (combine with any base pattern)
 * 
 * Call after another pattern, usage:
 * pattern_rainbow(buf, pixels, 1);
 * pattern_sparkle_overlay(buf, pixels);
 */
void pattern_sparkle_overlay(uint32_t *buffer, uint32_t pixels) {
    for (uint32_t i = 0; i < pixels; ++i) {
        if (((i + t) & 0x3F) == 0)
            buffer[i] = urgb_u32(255, 255, 255);
    }
}


/**
 * Fire / candle flicker (very popular on trees)
 */
void pattern_fire(uint32_t *buffer, uint32_t pixels, int dir) {
    for (uint32_t i = 0; i < pixels; ++i) {
        uint8_t flicker = (t + i * 13) & 0x3F;
        uint8_t r = 180 + flicker;
        uint8_t g = flicker >> 1;
        uint8_t b = g - 20;
        *buffer++ = urgb_u32(r, g, b);
    }
    t += dir;
}

/**
 * Snowfall (white drops falling down the tree)
 * 
 */
void pattern_snow(uint32_t *buffer, uint32_t pixels, int dir) {
    for (uint32_t i = 0; i < pixels; ++i) {
        if (((i + t) % 50) == 0)
            *buffer++ = urgb_u32(255, 255, 255);
        else
            *buffer++ = 0;
    }
    t += dir;
}

/**
 * Alternating Christmas colors (static but animated)
 */
void pattern_christmas(uint32_t *buffer, uint32_t pixels, int dir) {
    for (uint32_t i = 0; i < pixels; ++i) {
        if (((i + t / 10) & 1) == 0)
            *buffer++ = urgb_u32(255, 0, 0);
        else
            *buffer++ = urgb_u32(0, 255, 0);
    }
    t += dir;
}

/**
 * Smooth global fade (classic Christmas, calm)
 * 
 * All LEDs fade together between red ↔ green.
 * Effect
 * Very calm
 * Looks like slow breathing between red and green 
 * Ideal for a Christmas tree background mode
 */
void pattern_christmas_fade(uint32_t *buffer, uint32_t pixels, int dir) {
    // triangle wave 0..255..0
    uint8_t phase = t & 0xFF;
    if (phase > 127) phase = 255 - phase;
    phase <<= 1;  // scale to 0..254

    uint8_t red   = phase;
    uint8_t green = 255 - phase;

    for (uint32_t i = 0; i < pixels; ++i) {
        *buffer++ = urgb_u32(red, green, 0);
    }

    t += dir;
}

/**
 * Spatially distributed smooth fade (more dynamic)
 * Adjacent LEDs are out of phase → creates a flowing gradient.
 * 
 * Effect
 * Gentle wave of red ↔ green flowing along the tree
 * Still calm, but visually richer
 * Very popular on long vertical strings
 */
void pattern_christmas_fade_wave(uint32_t *buffer, uint32_t pixels, int dir) {
    for (uint32_t i = 0; i < pixels; ++i) {
        uint8_t phase = (t + i * 4) & 0xFF;
        if (phase > 127) phase = 255 - phase;
        phase <<= 1;

        uint8_t red   = phase;
        uint8_t green = 255 - phase;

        *buffer++ = urgb_u32(red, green, 0);
    }

    t += dir;
}

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

/**
 * Christmas palette: light blue / gold / light green
 */
static const rgb_t christmas_palette[] = {
    { 80, 120, 220 },  // light blue       { 120, 180, 255 }
    { 230, 170,  45 },  // gold             { 255, 190,  60 }
    { 90, 210, 90 },  // light green      { 120, 220, 120 }
};
// static const rgb_t warm_white_palette[] = {
//     {255, 180, 100},
//     {255, 210, 160},
// };
// static const rgb_t icy_palette[] = {
//     {180, 220, 255},
//     {220, 240, 255},
// };

#define CHRISTMAS_PAL_SIZE  (sizeof(christmas_palette)/sizeof(christmas_palette[0]))

/**
 * Linear interpolation between two colors
 */
static inline uint8_t lerp_u8(uint8_t a, uint8_t b, uint8_t t) {
    return a + (((int)(b - a) * t) >> 8);
}

static inline rgb_t palette_sample(const rgb_t *pal, uint8_t pal_size, uint16_t pos) {
    uint8_t idx = (pos >> 8) % pal_size;
    uint8_t next = (idx + 1) % pal_size;
    uint8_t frac = pos & 0xFF;

    rgb_t c0 = pal[idx];
    rgb_t c1 = pal[next];

    rgb_t out;
    out.r = lerp_u8(c0.r, c1.r, frac);
    out.g = lerp_u8(c0.g, c1.g, frac);
    out.b = lerp_u8(c0.b, c1.b, frac);
    return out;
}

/**
 * flowing Christmas palette wave
 */
void pattern_christmas_palette(uint32_t *buffer, uint32_t pixels, int dir) {
    for (uint32_t i = 0; i < pixels; ++i) {
        // spatial + temporal offset
        uint16_t pos = (uint16_t)(t * 4 + i * 256 / pixels);

        rgb_t c = palette_sample(christmas_palette,
                                 CHRISTMAS_PAL_SIZE,
                                 pos);

        *buffer++ = urgb_u32(c.r, c.g, c.b);
    }

    t += dir;
}



static uint8_t spark_level[700];   // >= max pixels
static uint32_t frame_div;


/**
 * Fast pseudo-random generator (deterministic, cheap)
 */
// static inline uint32_t xorshift32(void) {
//     static uint32_t x = 0x12345678;
//     x ^= x << 13;
//     x ^= x >> 17;
//     x ^= x << 5;
//     return x;
// }
static inline uint32_t xorshift32(void) {
    static uint32_t x = 0xA341316C;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

void pattern_warm_white_with_sparks(uint32_t *buffer,
                                    uint32_t pixels,
                                    int dir)
{
    // ---- base warm white (calm) ----
    const uint8_t base_r = 18, base_g = 14, base_b = 9;     // tested
    // const uint8_t base_r = 155, base_g = 120, base_b = 80;
    const uint8_t dimm_step = 16;
    
    

    // ---- time scaling ----
    // Run spark logic at ~25 Hz instead of 500 Hz
    frame_div++;
    // bool tick_25hz = (frame_div % 20) == 0;   // 500 / 20 = 25 Hz
    bool tick_25hz = (frame_div % 2) == 0;   // 

    // ---- create new spark (human-visible rate) ----
    if (tick_25hz) {
        // ~1 spark per second
        // if ((xorshift32() & 0xFF) < 10) {
        if ((xorshift32() & 0xF) < 10) {
            uint32_t idx = xorshift32() % pixels;
            // spark_level[idx] = 180 + (xorshift32() & 0x3F);
            spark_level[idx] = 220;
        }
    }

    // ---- render ----
    for (uint32_t i = 0; i < pixels; ++i) {
        uint8_t s = spark_level[i];

        if (s) {
            // cool icy spark (bluish, not harsh white)
            uint8_t r = base_r + (s - dimm_step);
            uint8_t g = base_g + s;
            uint8_t b = base_b + s;

            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;

            *buffer++ = urgb_u32(r, g, b);

            // decay tuned for 25 Hz
            spark_level[i] = (s > dimm_step) ? (s - dimm_step) : 0;
        } else {
            *buffer++ = urgb_u32(base_r, base_g, base_b);
        }
    }

    // to dziala, na cieplym tle zimne gwiazdki
    // for (uint32_t i = 0; i < pixels; ++i) {
    //     if (rand() % 232 == 0) {
    //         *buffer++ = urgb_u32(135, 185, 220);
    //     }
    //     else {
    //         *buffer++ = urgb_u32(base_r, base_g, base_b);
    //     }
    // }

    t += dir;
}



#define MAX_FALLING_SPARKS 12

typedef struct {
    int pos;        // fixed-point: pixel * 256
    int speed;      // pixels per frame * 256
    uint8_t life;   // decay counter
    uint8_t active;
} falling_spark_t;

static falling_spark_t falling[MAX_FALLING_SPARKS];


void pattern_falling_sparks(uint32_t *buffer,
                            uint32_t pixels,
                            int dir)
{
    // ---- dim cool base (night sky) ----
    const uint8_t base_r = 10;
    const uint8_t base_g = 20;
    const uint8_t base_b = 30;

    // clear buffer
    for (uint32_t i = 0; i < pixels; ++i)
        buffer[i] = urgb_u32(base_r, base_g, base_b);

    // ---- spawn new spark occasionally ----
    if ((xorshift32() & 0x1F) < 10) {  // ~1–2 per second
        for (int i = 0; i < MAX_FALLING_SPARKS; ++i) {
            if (!falling[i].active) {
                falling[i].active = 1;
                falling[i].pos   = 0;                 // top
                falling[i].speed = 300 + (xorshift32() & 0xFF); // ~1–2 px/frame
                falling[i].life  = 40;
                break;
            }
        }
    }

    // ---- update + draw sparks ----
    for (int i = 0; i < MAX_FALLING_SPARKS; ++i) {
        if (!falling[i].active)
            continue;

        int idx = falling[i].pos >> 8;
        if (idx >= 0 && idx < (int)pixels) {
            uint8_t l = falling[i].life;

            // icy spark color
            uint8_t r = 80  + (l << 1);
            uint8_t g = 120 + (l << 1);
            uint8_t b = 200 + (l << 1);

            if (b > 255) b = 255;

            buffer[idx] = urgb_u32(r, g, b);
        }

        falling[i].pos += falling[i].speed;
        if (falling[i].life > 0)
            falling[i].life--;

        if (falling[i].life == 0 || idx >= (int)pixels)
            falling[i].active = 0;
    }

    t += dir;
}

/**
 * Tuning cheatsheet
 * More ornaments   ORNAMENT_SPACING 30
 * Bigger bulbs     ORNAMENT_RADIUS 8
 * Faster falling sparks    falling[i].speed += 200;
 * Fewer sparks             (xorshift32() & 0xFF) < 5
 * 
 */
#define ORNAMENT_SPACING 40   // pixels between bulbs
#define ORNAMENT_RADIUS  6

void pattern_ornaments(uint32_t *buffer,
                       uint32_t pixels,
                       int dir)
{
    // ---- dark warm base ----
    const uint8_t base_r = 20;
    const uint8_t base_g = 10;
    const uint8_t base_b = 5;

    for (uint32_t i = 0; i < pixels; ++i)
        buffer[i] = urgb_u32(base_r, base_g, base_b);

    // ---- ornament bulbs ----
    for (uint32_t center = 0; center < pixels; center += ORNAMENT_SPACING) {

        // slow breathing phase per bulb
        uint8_t phase = (t + center) & 0xFF;
        if (phase > 127) phase = 255 - phase;
        phase <<= 1;  // 0..254

        // warm ornament color (gold/red mix)
        uint8_t br = 120 + (phase >> 1);
        uint8_t bg = 40  + (phase >> 2);
        uint8_t bb = 10;

        for (int d = -ORNAMENT_RADIUS; d <= ORNAMENT_RADIUS; ++d) {
            int idx = (int)center + d;
            if (idx < 0 || idx >= (int)pixels)
                continue;

            uint8_t falloff = 255 - (abs(d) * 255 / ORNAMENT_RADIUS);

            uint8_t r = (br * falloff) >> 8;
            uint8_t g = (bg * falloff) >> 8;
            uint8_t b = (bb * falloff) >> 8;

            buffer[idx] = urgb_u32(r, g, b);
        }
    }

    t += dir;
}


typedef struct {
    uint8_t r, g, b;
    uint8_t hue;        // 0..255 color wheel
    uint8_t phase;      // brightness breathing
} ornament_t;


static ornament_t ornaments[32];
static uint8_t ornaments_init;

// static inline uint32_t xorshift32(void) {    already exists
//     static uint32_t x = 0xA5A5A5A5;
//     x ^= x << 13;
//     x ^= x >> 17;
//     x ^= x << 5;
//     return x;
// }

static void init_ornament_clusters(uint32_t pixels);
static void init_fade_clusters(uint32_t pixels);

static void init_ornaments(void) {

    init_ornament_clusters(NUM_PIXELS);
    init_fade_clusters(NUM_PIXELS);


    for (int i = 0; i < 32; ++i) {
        ornaments[i].hue   = xorshift32() & 0xFF;
        ornaments[i].phase = xorshift32() & 0xFF;

        switch (xorshift32() % 3) {
            case 0: ornaments[i] = (ornament_t){220, 40,  20, xorshift32()}; break; // red
            case 1: ornaments[i] = (ornament_t){255, 180, 60, xorshift32()}; break; // gold
            case 2: ornaments[i] = (ornament_t){40,  180, 60, xorshift32()}; break; // green
        }
    }
    ornaments_init = 1;
}

void pattern_ornaments_multicolor(uint32_t *buffer,
                                  uint32_t pixels,
                                  int dir)
{
    if (!ornaments_init)
        init_ornaments();

    // dark warm background
    const uint8_t base_r = 12;
    const uint8_t base_g = 8;
    const uint8_t base_b = 4;

    for (uint32_t i = 0; i < pixels; ++i)
        buffer[i] = urgb_u32(base_r, base_g, base_b);

    uint32_t idx = 0;
    for (uint32_t center = 0;
         center < pixels && idx < 32;
         center += ORNAMENT_SPACING, ++idx)
    {
        ornament_t *o = &ornaments[idx];

        // very slow breathing (~5–6 seconds)
        uint8_t phase = o->phase;
        if (phase > 127) phase = 255 - phase;
        phase <<= 1;

        for (int d = -ORNAMENT_RADIUS; d <= ORNAMENT_RADIUS; ++d) {
            int p = (int)center + d;
            if (p < 0 || p >= (int)pixels)
                continue;

            uint8_t falloff = 255 - (abs(d) * 255 / ORNAMENT_RADIUS);

            uint8_t r = (o->r * phase >> 8) * falloff >> 8;
            uint8_t g = (o->g * phase >> 8) * falloff >> 8;
            uint8_t b = (o->b * phase >> 8) * falloff >> 8;

            buffer[p] = urgb_u32(r, g, b);
        }

        o->phase += dir;   // slow evolution
    }

    t += dir;
}

static inline void christmas_wheel(uint8_t h,
                                   uint8_t *r,
                                   uint8_t *g,
                                   uint8_t *b)
{
    if (h < 85) {              // red → gold
        *r = 255;
        *g = 60 + (h * 2);
        *b = 0;
    } else if (h < 170) {      // gold → green
        h -= 85;
        *r = 255 - (h * 3);
        *g = 255;
        *b = 0;
    } else {                   // green → red
        h -= 170;
        *r = h * 3;
        *g = 255 - (h * 3);
        *b = 0;
    }
}

void pattern_ornaments_cycling(uint32_t *buffer,
                               uint32_t pixels,
                               int dir)
{
    if (!ornaments_init)
        init_ornaments();

    // dark warm background
    const uint8_t base_r = 10;
    const uint8_t base_g = 6;
    const uint8_t base_b = 3;

    for (uint32_t i = 0; i < pixels; ++i)
        buffer[i] = urgb_u32(base_r, base_g, base_b);

    uint32_t idx = 0;
    for (uint32_t center = 0;
         center < pixels && idx < 32;
         center += ORNAMENT_SPACING, ++idx)
    {
        ornament_t *o = &ornaments[idx];

        // brightness breathing (~6 s cycle)
        uint8_t br = o->phase;
        if (br > 127) br = 255 - br;
        br <<= 1;

        // color cycling (~15–20 s per full loop)
        uint8_t r0, g0, b0;
        christmas_wheel(o->hue, &r0, &g0, &b0);

        for (int d = -ORNAMENT_RADIUS; d <= ORNAMENT_RADIUS; ++d) {
            int p = (int)center + d;
            if (p < 0 || p >= (int)pixels)
                continue;

            uint8_t falloff = 255 - (abs(d) * 255 / ORNAMENT_RADIUS);

            uint8_t r = (r0 * br >> 8) * falloff >> 8;
            uint8_t g = (g0 * br >> 8) * falloff >> 8;
            uint8_t b = (b0 * br >> 8) * falloff >> 8;

            buffer[p] = urgb_u32(r, g, b);
        }

        o->phase += 1;          // breathing speed
        o->hue   += 1;          // color cycle speed
    }

    t += dir;
}


#define MAX_CLUSTERS        10
#define CLUSTER_MIN_SIZE   2
#define CLUSTER_MAX_SIZE   5
#define CLUSTER_SPACING    60     // distance between clusters
#define BULB_SPACING       4      // distance between bulbs in cluster
// #define ORNAMENT_RADIUS    5    // 6 linie 994

typedef struct {
    uint16_t center;      // pixel index of cluster center
    uint8_t  size;        // number of bulbs
    uint8_t  hue;         // base hue (cycles slowly)
    uint8_t  phase;       // base brightness phase
} ornament_cluster_t;

static ornament_cluster_t clusters[MAX_CLUSTERS];
static uint8_t clusters_init;

static void init_ornament_clusters(uint32_t pixels) {
    uint32_t pos = CLUSTER_SPACING / 2;

    for (int i = 0; i < MAX_CLUSTERS && pos < pixels; ++i) {
        clusters[i].center = pos;
        clusters[i].size   = CLUSTER_MIN_SIZE +
                              (xorshift32() % (CLUSTER_MAX_SIZE - CLUSTER_MIN_SIZE + 1));
        clusters[i].hue    = xorshift32() & 0xFF;
        clusters[i].phase  = xorshift32() & 0xFF;

        pos += CLUSTER_SPACING;
    }

    clusters_init = 1;
}


/*
7. Quick tuning table
Effect	Change
Bigger clusters	CLUSTER_MAX_SIZE 6
Denser clusters	CLUSTER_SPACING 45
Larger bulbs	ORNAMENT_RADIUS 7
Slower color cycle	cl->hue += (t & 1)
Slower breathing	cl->phase += (t & 1)
*/
void pattern_ornament_clusters(uint32_t *buffer,
                               uint32_t pixels,
                               int dir)
{
    if (!clusters_init)
        init_ornament_clusters(pixels);

    // very dark warm background
    const uint8_t base_r = 8;
    const uint8_t base_g = 5;
    const uint8_t base_b = 3;

    for (uint32_t i = 0; i < pixels; ++i)
        buffer[i] = urgb_u32(base_r, base_g, base_b);

    for (int c = 0; c < MAX_CLUSTERS; ++c) {
        ornament_cluster_t *cl = &clusters[c];

        // cluster breathing (~6–8 s)
        uint8_t br = cl->phase;
        if (br > 127) br = 255 - br;
        br <<= 1;

        uint8_t r0, g0, b0;
        christmas_wheel(cl->hue, &r0, &g0, &b0);

        // bulbs inside cluster
        for (int b = 0; b < cl->size; ++b) {
            int bulb_center =
                (int)cl->center +
                (b - (cl->size - 1) / 2) * BULB_SPACING;

            if (bulb_center < 0 || bulb_center >= (int)pixels)
                continue;

            // slight per-bulb phase offset
            uint8_t bulb_br = br - (b * 12);

            for (int d = -ORNAMENT_RADIUS; d <= ORNAMENT_RADIUS; ++d) {
                int p = bulb_center + d;
                if (p < 0 || p >= (int)pixels)
                    continue;

                uint8_t falloff =
                    255 - (abs(d) * 255 / ORNAMENT_RADIUS);

                uint8_t r = (r0 * bulb_br >> 8) * falloff >> 8;
                uint8_t g = (g0 * bulb_br >> 8) * falloff >> 8;
                uint8_t bcol = (b0 * bulb_br >> 8) * falloff >> 8;

                buffer[p] = urgb_u32(r, g, bcol);
            }
        }

        cl->phase += 1;   // breathing speed
        cl->hue   += 1;   // color cycling speed
    }

    t += dir;
}


/**
 * Global slow fade: one random color → another (whole strip)
 * Characteristics
 *  - Entire tree smoothly fades between colors
 *  - Extremely calm
 *  - Perfect as “idle / night mode”
 */
// typedef struct {
//     uint8_t r, g, b;
// } rgb_t;

// static rgb_t color_from;
// static rgb_t color_to;
// static uint16_t fade_pos;   // 0..4096
// static uint8_t fade_init;

/**
 * Random color generator (pleasant range)
 */
static rgb_t random_soft_color(void) {
    rgb_t c;
    c.r = 80  + (xorshift32() & 0x7F);
    c.g = 80  + (xorshift32() & 0x7F);
    c.b = 80  + (xorshift32() & 0x7F);
    return c;
}

static rgb_t random_visible_color(void) {
    rgb_t c;
    uint8_t h = xorshift32() & 0xFF;

    if (h < 85) {                 // red → yellow
        c.r = 255;
        c.g = h * 3;
        c.b = 0;
    } else if (h < 170) {          // yellow → green
        h -= 85;
        c.r = 255 - h * 3;
        c.g = 255;
        c.b = 0;
    } else {                       // green → red
        h -= 170;
        c.r = 0;
        c.g = 255 - h * 3;
        c.b = h * 3;
    }

    // soften (avoid harsh saturation)
    c.r = (c.r >> 1) + 40;
    c.g = (c.g >> 1) + 40;
    c.b = (c.b >> 1) + 40;
    return c;
}




/**
 * Pattern: slow global fade (~8 seconds)
 * 
 */
void pattern_global_color_fade(uint32_t *buffer,
                               uint32_t pixels,
                               int dir)
{
    static rgb_t from, to;
    static uint16_t pos;
    static uint8_t init;

    if (!init) {
        from = random_visible_color();
        to   = random_visible_color();
        pos  = 0;
        init = 1;
    }

    // pos: 0..255
    uint8_t t8 = pos;

    uint8_t r = from.r + ((to.r - from.r) * t8 >> 8);
    uint8_t g = from.g + ((to.g - from.g) * t8 >> 8);
    uint8_t b = from.b + ((to.b - from.b) * t8 >> 8);

    for (uint32_t i = 0; i < pixels; ++i)
        buffer[i] = urgb_u32(r, g, b);

    pos += 1;   // 255 frames ≈ 5.1 s @ 50 FPS

    if (pos >= 255) {
        pos = 0;
        from = to;
        to   = random_visible_color();
    }
}


// void pattern_global_color_fade(uint32_t *buffer,
//                                uint32_t pixels,
//                                int dir)
// {
//     if (!fade_init) {
//         color_from = random_soft_color();
//         color_to   = random_soft_color();
//         fade_pos   = 0;
//         fade_init  = 1;
//     }

//     // fade_pos: 0..4096 (~8.2 s @ 50 FPS with step=10)
//     uint16_t t16 = fade_pos >> 4;  // 0..255

//     uint8_t r = color_from.r + ((color_to.r - color_from.r) * t16 >> 8);
//     uint8_t g = color_from.g + ((color_to.g - color_from.g) * t16 >> 8);
//     uint8_t b = color_from.b + ((color_to.b - color_from.b) * t16 >> 8);

//     for (uint32_t i = 0; i < pixels; ++i)
//         buffer[i] = urgb_u32(r, g, b);

//     fade_pos += 10;  // speed control

//     if (fade_pos >= 4096) {
//         fade_pos = 0;
//         color_from = color_to;
//         color_to   = random_soft_color();
//     }
// }

typedef struct {
    uint16_t center;
    uint8_t  size;
    rgb_t    from;
    rgb_t    to;
    uint16_t pos;
} color_cluster_t;

static color_cluster_t fade_clusters[MAX_CLUSTERS];
static uint8_t fade_clusters_init;

static void init_fade_clusters(uint32_t pixels) {
    uint32_t pos = CLUSTER_SPACING / 2;

    for (int i = 0; i < MAX_CLUSTERS && pos < pixels; ++i) {
        fade_clusters[i].center = pos;
        fade_clusters[i].size   = CLUSTER_MIN_SIZE +
                                  (xorshift32() %
                                   (CLUSTER_MAX_SIZE - CLUSTER_MIN_SIZE + 1));
        fade_clusters[i].from   = random_soft_color();
        fade_clusters[i].to     = random_soft_color();
        fade_clusters[i].pos    = xorshift32() & 0x0FFF;
        pos += CLUSTER_SPACING;
    }

    fade_clusters_init = 1;
}

/**
 * cluster color morph (~8 s per transition)
 */
void pattern_cluster_color_fade(uint32_t *buffer,
                                uint32_t pixels,
                                int dir)
{
    static uint8_t init;
    static struct {
        uint16_t center;
        uint8_t  size;
        rgb_t    from, to;
        uint8_t  pos;
        uint8_t  breath;
    } cl[MAX_CLUSTERS];

    if (!init) {
        uint32_t p = CLUSTER_SPACING / 2;
        for (int i = 0; i < MAX_CLUSTERS && p < pixels; ++i) {
            cl[i].center = p;
            cl[i].size   = 2 + (xorshift32() % 3);
            cl[i].from   = random_visible_color();
            cl[i].to     = random_visible_color();
            cl[i].pos    = xorshift32() & 0xFF;
            cl[i].breath = xorshift32() & 0xFF;
            p += CLUSTER_SPACING;
        }
        init = 1;
    }

    // dark background
    for (uint32_t i = 0; i < pixels; ++i)
        buffer[i] = urgb_u32(6, 4, 2);

    for (int i = 0; i < MAX_CLUSTERS; ++i) {
        uint8_t t8 = cl[i].pos;

        uint8_t r0 = cl[i].from.r + ((cl[i].to.r - cl[i].from.r) * t8 >> 8);
        uint8_t g0 = cl[i].from.g + ((cl[i].to.g - cl[i].from.g) * t8 >> 8);
        uint8_t b0 = cl[i].from.b + ((cl[i].to.b - cl[i].from.b) * t8 >> 8);

        // breathing envelope
        uint8_t br = cl[i].breath;
        if (br > 127) br = 255 - br;
        br <<= 1;  // 0..254

        for (int b = 0; b < cl[i].size; ++b) {
            int cpos = cl[i].center +
                       (b - (cl[i].size - 1) / 2) * BULB_SPACING;

            for (int d = -ORNAMENT_RADIUS; d <= ORNAMENT_RADIUS; ++d) {
                int p = cpos + d;
                if (p < 0 || p >= (int)pixels)
                    continue;

                uint8_t fall = 255 - (abs(d) * 255 / ORNAMENT_RADIUS);

                uint8_t r = (r0 * br >> 8) * fall >> 8;
                uint8_t g = (g0 * br >> 8) * fall >> 8;
                uint8_t bcol = (b0 * br >> 8) * fall >> 8;

                buffer[p] = urgb_u32(r, g, bcol);
            }
        }

        cl[i].pos += 1;       // color morph speed
        cl[i].breath += 1;   // brightness breathing

        if (cl[i].pos == 0) {
            cl[i].from = cl[i].to;
            cl[i].to   = random_visible_color();
        }
    }
}


// void pattern_cluster_color_fade(uint32_t *buffer,
//                                 uint32_t pixels,
//                                 int dir)
// {
//     if (!fade_clusters_init)
//         init_fade_clusters(pixels);

//     // dark neutral base
//     for (uint32_t i = 0; i < pixels; ++i)
//         buffer[i] = urgb_u32(5, 5, 5);

//     for (int c = 0; c < MAX_CLUSTERS; ++c) {
//         color_cluster_t *cl = &fade_clusters[c];

//         uint16_t t16 = cl->pos >> 4;  // 0..255

//         uint8_t cr = cl->from.r + ((cl->to.r - cl->from.r) * t16 >> 8);
//         uint8_t cg = cl->from.g + ((cl->to.g - cl->from.g) * t16 >> 8);
//         uint8_t cb = cl->from.b + ((cl->to.b - cl->from.b) * t16 >> 8);

//         for (int b = 0; b < cl->size; ++b) {
//             int bulb_center =
//                 (int)cl->center +
//                 (b - (cl->size - 1) / 2) * BULB_SPACING;

//             for (int d = -ORNAMENT_RADIUS; d <= ORNAMENT_RADIUS; ++d) {
//                 int p = bulb_center + d;
//                 if (p < 0 || p >= (int)pixels)
//                     continue;

//                 uint8_t falloff =
//                     255 - (abs(d) * 255 / ORNAMENT_RADIUS);

//                 uint8_t r = (cr * falloff) >> 8;
//                 uint8_t g = (cg * falloff) >> 8;
//                 uint8_t bcol = (cb * falloff) >> 8;

//                 buffer[p] = urgb_u32(r, g, bcol);
//             }
//         }

//         cl->pos += 8;  // ~8–9 s per morph

//         if (cl->pos >= 4096) {
//             cl->pos = 0;
//             cl->from = cl->to;
//             cl->to   = random_soft_color();
//         }
//     }
// }




void pattern_connection_show(uint32_t *buffer,
                               uint32_t pixels,
                               int dir)
{
    // very dark warm background
    const uint8_t base_r = 180;
    const uint8_t base_g = 80;
    const uint8_t base_b = 30;
    

#ifdef OUTDOOR_TREE_WS2815
    const uint32_t id[4] = {199, 200, 399, 400};
#else // OUTDOOR_TREE_WS2815
    const uint32_t id[4] = {197, 198, 394, 395};
#endif // OUTDOOR_TREE_WS2815

    for (uint32_t i = 0; i < pixels; ++i) {

        if (i == id[0] || i == id[1] || i == id[2] || i == id[3]) {
            buffer[i] = urgb_u32(base_r, base_g, base_b);
        }
        else {
            buffer[i] = 0; // urgb_u32(base_r, base_g, base_b);
        }
    }
}

void pattern_fade_show(uint32_t *buffer,
                               uint32_t pixels,
                               int dir)
{

    // uint32_t i, con1, con2;
    static rgb_t from, to;
    static uint16_t pos;
    // static uint8_t init;

    // con1 = 200;
    // con2 = 400;

    // pos: 0..255
    uint8_t t8 = pos;

    uint8_t r = from.r + ((to.r - from.r) * t8 >> 8);
    uint8_t g = from.g + ((to.g - from.g) * t8 >> 8);
    uint8_t b = from.b + ((to.b - from.b) * t8 >> 8);

    for (uint32_t i = 0; i < pixels; ++i)
        buffer[i] = urgb_u32(r, g, b);

    pos += 1;   // 255 frames ≈ 5.1 s @ 50 FPS

    if (pos >= 255) {
        pos = 0;
        from = to;
        to   = random_visible_color();
    }
}


void pattern_zero(uint32_t *buffer,
                               uint32_t pixels,
                               int dir)
{

    for (uint32_t i = 0; i < pixels; ++i) {
        buffer[i] = 0; // urgb_u32(base_r, base_g, base_b);
    }
}










/**
 * Side-to-side drifting snow (❄️)
 * 
 * Design goals
 *  - Snow falls downward
 *  - Each flake sways left/right
 *  - Cool white
 *  - Short lifetime
 *  - Sparse, elegant
 */
#define MAX_SNOWFLAKES 16

// typedef struct {
//     int pos;        // fixed point (pixel * 256)
//     int drift;      // horizontal sway phase
//     int speed;      // fall speed
//     uint8_t life;
//     uint8_t active;
// } snowflake_t;

typedef struct {
    int pos;        // fixed point
    int speed;
    uint8_t bright;
    uint8_t life;
    uint8_t active;
} snowflake_t;

// static snowflake_t snow[MAX_SNOWFLAKES];

/*
Quick tuning table
Effect	Change
Bigger ornaments	ORNAMENT_RADIUS 8
More ornaments	ORNAMENT_SPACING 30
Slower breathing	o->phase += (dir >> 1)
Fewer snowflakes	(xorshift32() & 0xFF) < 6
Stronger sway	sway -= 12
*/

// void pattern_drifting_snow(uint32_t *buffer,
//                            uint32_t pixels,
//                            int dir)
// {
//     // spawn rate tuned for 50 FPS
//     if ((xorshift32() & 0xFF) < 12) {
//         for (int i = 0; i < MAX_SNOWFLAKES; ++i) {
//             if (!snow[i].active) {
//                 snow[i].active = 1;
//                 snow[i].pos   = 0;
//                 snow[i].drift = xorshift32() & 0xFF;
//                 snow[i].speed = 120 + (xorshift32() & 0x7F); // ~0.5–1 px/frame
//                 snow[i].life  = 60;
//                 break;
//             }
//         }
//     }

//     for (int i = 0; i < MAX_SNOWFLAKES; ++i) {
//         if (!snow[i].active)
//             continue;

//         int base = snow[i].pos >> 8;

//         // side-to-side drift (sine-like via triangle wave)
//         int sway = snow[i].drift & 0x1F;
//         if (sway > 15) sway = 31 - sway;
//         sway -= 8;  // -8..+8

//         int idx = base + sway;

//         if (idx >= 0 && idx < (int)pixels) {
//             uint8_t l = snow[i].life;

//             uint8_t r = 160 + l;
//             uint8_t g = 180 + l;
//             uint8_t b = 220 + l;

//             if (b > 255) b = 255;

//             buffer[idx] = urgb_u32(r, g, b);
//         }

//         snow[i].pos   += snow[i].speed;
//         snow[i].drift += 3;
//         snow[i].life--;

//         if (snow[i].life == 0 || base >= (int)pixels)
//             snow[i].active = 0;
//     }

//     t += dir;
// }

// void pattern_falling_snow(uint32_t *buffer,
//                           uint32_t pixels,
//                           int dir)
// {
//     // spawn rate tuned for 50 FPS
//     if ((xorshift32() & 0xFF) < 10) {
//         for (int i = 0; i < MAX_SNOWFLAKES; ++i) {
//             if (!snow[i].active) {
//                 snow[i].active = 1;
//                 snow[i].pos    = 0;
//                 snow[i].speed  = 80 + (xorshift32() & 0x7F);  // slow fall
//                 snow[i].bright = 120 + (xorshift32() & 0x7F);
//                 snow[i].life   = 80;
//                 break;
//             }
//         }
//     }

//     for (int i = 0; i < MAX_SNOWFLAKES; ++i) {
//         if (!snow[i].active)
//             continue;

//         int idx = snow[i].pos >> 8;
//         if (idx >= 0 && idx < (int)pixels) {
//             uint8_t l = snow[i].life;

//             uint8_t r = snow[i].bright;
//             uint8_t g = snow[i].bright + (l >> 2);
//             uint8_t b = snow[i].bright + (l >> 1);

//             if (b > 255) b = 255;

//             buffer[idx] = urgb_u32(r, g, b);
//         }

//         snow[i].pos += snow[i].speed;
//         snow[i].life--;

//         if (snow[i].life == 0 || idx >= (int)pixels)
//             snow[i].active = 0;
//     }

//     t += dir;
// }








// /**
//  * Pattern selection dispatcher
//  */
// typedef void (*pattern_fn_t)(uint32_t *, uint32_t, int);

// pattern_fn_t patterns[] = {
//     pattern_breath,
//     pattern_rainbow,
//     pattern_color_wipe,
//     pattern_twinkle,
//     pattern_chase,
//     pattern_fire,
//     pattern_snow,
//     pattern_christmas,
// };

// void run_pattern(uint8_t idx, uint32_t *buf, uint32_t pixels, int dir) {
//     patterns[idx % (sizeof(patterns)/sizeof(patterns[0]))](buf, pixels, dir);
// }





// void pattern_random(uint8_t *buffer, uint strips, uint pixels) {
//     uint y;
//     uint8_t color, r, g, b, val;
//     // uint32_t px;

//     // t += dir;
//     if (++t % 8)    // 20ms * 8 = 160ms
//         return;

//      for ( y = 0; y < strips; ++y) {
//         current_strip_out = buffer + y * pixels * NUM_CHANNELS;
//         for (uint i = 0; i < pixels; ++i) {
//             color = rand() % 7;
//             val = rand() % 256;
//             r = (color & 0x4) ? val : 0;
//             g = (color & 0x2) ? val : 0;
//             b = (color & 0x1) ? val : 0;

//             //px = urgb_u32(r, g, b);
//             // x = rand() % 256;
//             put_pixel(urgb_u32(r, g, b));
//         }
//     }
//     //for (uint i = 0; i < pixels; ++i)
//     //    put_pixel(rand());
// }

// void pattern_sparkle(uint8_t *buffer, uint strips, uint pixels) {
//     static uint8_t flip = 0;
//     if (++t % 2)
//         return;

//     uint y;
//     for ( y = 0; y < strips; ++y) {
//         current_strip_out = buffer + y * pixels * NUM_CHANNELS;
//         for (uint i = 0; i < pixels; ++i) {
//             if (!flip && rand() % 32 == 0) {
//                 put_pixel(urgb_u32(0xff, 0xff, 0xff));
//             } else {
//                 put_pixel(0);
//             }
//         }
//     }
//     if (!flip)
//         flip = 3;
//     else
//         flip--;
//     // for (uint i = 0; i < pixels; ++i)
//     //     put_pixel(rand() % 16 ? 0 : 0xffffffff);
//     // t += dir;
// }



#ifdef USE_TABLE_JAREMEK
uint32_t jaremektable[] = {
        0x00F000, 0xF00000, 0x0000F0, 0xF0F0F0, 0x000000, 0x888800, 0x008888, 0x000000
};
    uint max = count_of(jaremektable);
    if (!(t % 6)) {
        for (y = 0; y < strips; y++) {
            current_strip_out = buffer + y * pixels * NUM_CHANNELS;
            for(x = 0; x < pixels; x++) {
                put_pixel(jaremektable[(t + x + (y < 2)) % max]);
            }
        }
    }
#endif

// #define STRIP_RAISE_PX 16
// // #define LINEAR_TABLE linear_brightness_percent
// #define LINEAR_TABLE linear_brightness_percept
// void pattern_jaremek(uint8_t *buffer, uint strips, uint pixels) {
//     uint y, x, mul, max, select;
//     uint8_t c, r, g, b;

//     max = count_of(LINEAR_TABLE);
//     mul = max / STRIP_RAISE_PX;
//     // if (!(t % 6)) {
//     for (y = 0; y < strips; y++) {
//         current_strip_out = buffer + y * pixels * NUM_CHANNELS;
//         for(x = 0; x < pixels; x++) {
//             // linear on every strip for one color
//             select = (x * mul + t) % max;
//             c = LINEAR_TABLE[select];

//             r = g = b = 0;
//             select = (y % 7) + 1;
//             if (select & 0x04)
//                 r = c;
//             if (select & 0x02)
//                 g = c;
//             if (select & 0x01)
//                 b = c;

//             put_pixel(urgb_u32(r, g, b));
//         }
//     }
//     // }
//     t += dir;
// }

