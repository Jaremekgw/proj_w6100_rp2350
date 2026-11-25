/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
drv_ms72sf1 (LOW LEVEL)

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
#include "ms72sf1_protocol.h"

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
#define MS72SF1_OBJECT_SLOTS 3

typedef struct __attribute__((packed))
{
    /* Sign-bit + magnitude encoding (NOT two's complement), see decode helper */
    uint16_t x_raw;
    uint16_t y_raw;
    uint16_t v_raw;
    uint16_t dist_mm; /* already uint16 in mm */
} ms72sf1_object_raw_t;

// New proposal
typedef struct __attribute__((packed))
{
    /* Sign-bit + magnitude encoding (NOT two's complement), see decode helper */
    int16_t x_raw;
    int16_t y_raw;
    int16_t v_raw;
    uint16_t dist_mm; /* already uint16 in mm */
    bool active; /* true if this slot has a valid target, false if it's just empty data */
} ms72sf1_target_raw_t;
typedef struct
{
    uint32_t            rx_time_ms;
    ms72sf1_target_raw_t  target[MS72SF1_OBJECT_SLOTS];
    volatile bool       ready; /* set to true by driver when a new frame is ready, cleared by main loop after consuming */
} ms72sf1_data_t;

/* Hardware + driver init */
bool ms72sf1_drv_init(uart_cfg_t *cfg);

/* Poll UART and assemble frames */
void ms72sf1_drv_poll(void);

/* Non-blocking frame fetch */
bool ms72sf1_drv_get_ready(ms72sf1_data_t *out);

void ms72sf1_drv_set_debug(bool enable);
// void ms72sf1_drv_set_raw_debug(bool enable);

void ms72sf1_drv_send_multi_target_cmd(void);
void ms72sf1_drv_send_single_target_cmd(void);


// bool ms72sf1_drv_write_raw(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

