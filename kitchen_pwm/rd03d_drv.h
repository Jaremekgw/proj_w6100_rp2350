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

#include "rd03d_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// see: protocol definitions in rd03d_protocol.h
// typedef struct
// {
//     uint8_t  msg_id;
//     uint8_t  len;
//     uint8_t  payload[64];
// } rd03d_frame_t;

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

typedef struct __attribute__((packed))
{
    rd03d_object_raw_t obj[RD03D_OBJECT_SLOTS];
} rd03d_report_raw_t;

typedef struct
{
    rd03d_report_raw_t report;
    uint32_t           rx_time_ms;
} rd03d_frame_t;


/* Hardware + driver init */
bool rd03d_drv_init(void);

/* Poll UART and assemble frames */
void rd03d_drv_poll(void);

/* Non-blocking frame fetch */
bool rd03d_drv_get_frame(rd03d_frame_t *out);

#ifdef __cplusplus
}
#endif



// #pragma once

// #include <stdint.h>
// #include <stdbool.h>

// #ifdef __cplusplus
// extern "C" {
// #endif

// typedef struct
// {
//     bool     presence;
//     uint16_t distance_mm;
//     uint8_t  confidence;
// } rd03d_data_t;

// /* Initialize UART + internal state */
// bool rd03d_init(void);

// /* Poll UART, parse frames, update internal state */
// void rd03d_poll(void);

// /* Get latest parsed data (thread-safe for main loop usage) */
// bool rd03d_get_data(rd03d_data_t *out);

// #ifdef __cplusplus
// }
// #endif
