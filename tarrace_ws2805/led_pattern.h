/**
 * Copyright (c) 2024 Raspberry Pi Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef LED_PATTERN_H
#define LED_PATTERN_H
#include <stdint.h>
#include "config.h"

void init_start_strips(void);
// typedef void (*pattern_func_t)(uint len, uint t);
// typedef void (*pattern)(uint len, uint t);
typedef void (*pattern)(uint8_t *buffer, uint strips, uint pixels);

// Each call toggles one channel across all pixels, preserving other channels.
void pattern_red(uint8_t *buffer, uint strips, uint pixels);
void pattern_green(uint8_t *buffer, uint strips, uint pixels);
void pattern_blue(uint8_t *buffer, uint strips, uint pixels);
#if NUM_CHANNELS >= 4
void pattern_warm(uint8_t *buffer, uint strips, uint pixels);
#endif
#if NUM_CHANNELS >= 5
void pattern_cold(uint8_t *buffer, uint strips, uint pixels);
#endif

#if NUM_CHANNELS == 5
// Ambient animations assume the 20 ms pattern loop and write RGB/warm/cold.
void pattern_white_crossfade(uint8_t *buffer, uint strips, uint pixels);
void pattern_colour_tide(uint8_t *buffer, uint strips, uint pixels);
void pattern_soft_comet(uint8_t *buffer, uint strips, uint pixels);
void pattern_fireflies(uint8_t *buffer, uint strips, uint pixels);
void pattern_candlelight(uint8_t *buffer, uint strips, uint pixels);
void pattern_sunset(uint8_t *buffer, uint strips, uint pixels);
void pattern_slow_rain(uint8_t *buffer, uint strips, uint pixels);
#endif

void pattern_snakes(uint8_t *buffer, uint strips, uint pixels);
void pattern_random(uint8_t *buffer, uint strips, uint pixels);
void pattern_sparkle(uint8_t *buffer, uint strips, uint pixels);
void pattern_drop1(uint8_t *buffer, uint strips, uint pixels);
void pattern_solid(uint8_t *buffer, uint strips, uint pixels);
void pattern_jaremek(uint8_t *buffer, uint strips, uint pixels);

#endif /* LED_PATTERN_H */ 