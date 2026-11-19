/**
 * Copyright (c) 2024 Raspberry Pi Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef WS2815_CONTROL_DMA_PARALLEL_H
#define WS2815_CONTROL_DMA_PARALLEL_H

#include <stdint.h>

void ws2815_init(void);
void ws2815_pattern_loop(uint32_t period_ms);
void ws2815_loop(uint32_t period_ms);
void ws2815_show(uint8_t *fb);
uint8_t set_pattern_index(uint8_t index);
uint8_t get_pattern_index(void);

#endif /* WS2815_CONTROL_DMA_PARALLEL_H */
