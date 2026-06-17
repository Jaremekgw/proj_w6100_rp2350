/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
drv_rd03d (LOW LEVEL)

Owns:
  UART init
  RX ring buffer
  Frame synchronization
  Checksum verification
  Raw frame extraction

Does NOT:
  Interpret “presence”
  Apply thresholds
  Apply debounce / hysteresis
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "hardware/uart.h"
#include "rd03d_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uart_inst_t *instance;
    uint8_t func;
    uint rx_pin;
    uint tx_pin;
    int baudrate;
} uart_cfg_t;

/* RD-03D reports exactly 3 objects per frame */
#define RD03D_OBJECT_SLOTS 3

typedef struct __attribute__((packed))
{
    /* Sign-bit + magnitude encoding (NOT two's complement), see decode helper */
    uint16_t x_raw;
    uint16_t y_raw;
    uint16_t v_raw;
    uint16_t dist_mm; /* already uint16 in mm */
} rd03d_object_raw_t;

// New proposal
typedef struct __attribute__((packed))
{
    /* Sign-bit + magnitude encoding (NOT two's complement), see decode helper */
    int16_t x_raw;
    int16_t y_raw;
    int16_t v_raw;
    uint16_t dist_mm; /* already uint16 in mm */
    bool active; /* true if this slot has a valid target, false if it's just empty data */
} rd03d_target_raw_t;
typedef struct
{
    uint32_t            rx_time_ms;
    rd03d_target_raw_t  target[RD03D_OBJECT_SLOTS];
    volatile bool       ready; /* set to true by driver when a new frame is ready, cleared by main loop after consuming */
} rd03d_data_t;

/* Hardware + driver init */
bool rd03d_drv_init(uart_cfg_t *cfg);

/* Poll UART and assemble frames */
void rd03d_drv_poll(void);

/* Non-blocking frame fetch */
bool rd03d_drv_get_ready(rd03d_data_t *out);

void rd03d_drv_set_debug(bool enable);
// void rd03d_drv_set_raw_debug(bool enable);

void rd03d_drv_send_multi_target_cmd(void);
void rd03d_drv_send_single_target_cmd(void);


// bool rd03d_drv_write_raw(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

