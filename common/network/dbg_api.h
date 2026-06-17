/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <stdint.h>

#define RD03D_MAX_TARGETS 3

typedef struct __attribute__((packed))
{
    uint32_t timestamp_ms;
    uint8_t presence;
    uint8_t targets;

    struct {
        int16_t x_mm;
        int16_t y_mm;
        int16_t velocity_cm_s;
        uint16_t distance_mm;
        uint8_t valid;
    } target[RD03D_MAX_TARGETS];

} dbg_rd03d_packet_t;

void dbg_server_init(uint8_t sn, uint16_t port, uint8_t *buf, uint16_t buf_size);
int dbg_server_poll(void);
int dbg_send_sensor_data(uint32_t now_ms, const void *data, uint16_t size);
