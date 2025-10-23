/**
 * Copyright (c) 2024 Raspberry Pi Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef WS2815_CONTROL_DMA_PARALLEL_H
#define WS2815_CONTROL_DMA_PARALLEL_H

#include <stdint.h>

void ws2815_init(void);
void ws2815_loop(void);
void ws2815_show(uint8_t *fb);

#endif /* WS2815_CONTROL_DMA_PARALLEL_H */
