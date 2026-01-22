/**
 * Copyright (c) 2024 Raspberry Pi Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef LED_PATTERN_H
#define LED_PATTERN_H
#include <stdint.h>

void init_start_strips(void);
typedef void (*pattern)(uint32_t *buffer, uint32_t pixels, int dir);

void pattern_zero(uint32_t *buffer, uint32_t pixels, int dir);

// void pattern_simple(uint32_t *buffer, uint8_t *rgb, uint32_t pixels);
void pattern_snakes1(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_snakes2(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_snakes3(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_snakes4(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_snakes5(uint32_t *buffer, uint32_t pixels, int dir);


void pattern_breath(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_rainbow(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_color_wipe(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_twinkle(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_chase(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_fire(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_snow(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_christmas_fade(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_christmas_fade_wave(uint32_t *buffer, uint32_t pixels, int dir);

void pattern_christmas_palette(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_warm_white_with_sparks(uint32_t *buffer, uint32_t pixels, int dir);

void pattern_falling_sparks(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_ornaments(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_ornaments_multicolor(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_ornaments_cycling(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_ornament_clusters(uint32_t *buffer, uint32_t pixels, int dir);

void pattern_global_color_fade(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_cluster_color_fade(uint32_t *buffer, uint32_t pixels, int dir);

void pattern_connection_show(uint32_t *buffer, uint32_t pixels, int dir);
void pattern_fade_show(uint32_t *buffer, uint32_t pixels, int dir);

// void pattern_random(uint8_t *buffer, uint strips, uint pixels);
// void pattern_sparkle(uint8_t *buffer, uint strips, uint pixels);
// void pattern_drop1(uint8_t *buffer, uint strips, uint pixels);
// void pattern_solid(uint8_t *buffer, uint strips, uint pixels);
// void pattern_jaremek(uint8_t *buffer, uint strips, uint pixels);

#endif /* LED_PATTERN_H */ 