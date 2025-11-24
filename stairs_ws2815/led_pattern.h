/**
 * Copyright (c) 2024 Raspberry Pi Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef LED_PATTERN_H
#define LED_PATTERN_H
#include <stdint.h>

void init_start_strips(void);
// typedef void (*pattern_func_t)(uint len, uint t);
// typedef void (*pattern)(uint len, uint t);
typedef void (*pattern)(uint8_t *buffer, uint strips, uint pixels);

void pattern_snakes(uint8_t *buffer, uint strips, uint pixels);
void pattern_random(uint8_t *buffer, uint strips, uint pixels);
void pattern_sparkle(uint8_t *buffer, uint strips, uint pixels);
void pattern_drop1(uint8_t *buffer, uint strips, uint pixels);
void pattern_solid(uint8_t *buffer, uint strips, uint pixels);
void pattern_jaremek(uint8_t *buffer, uint strips, uint pixels);

#endif /* LED_PATTERN_H */ 