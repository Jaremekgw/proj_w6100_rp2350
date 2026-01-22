/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/* =============================
 * RD-03D PROTOCOL DEFINITIONS
 * =============================
 *
 * YOU MUST CONFIRM THESE VALUES.
 * Replace placeholders once protocol is verified.
 * 
 * 
 * 
    Frame:
    AA FF 03 00
    [Target 1]
    [Target 2]
    [Target 3]
    55 CC


 */



#define RD03D_FRAME_HEADER_0   0xAA
#define RD03D_FRAME_HEADER_1   0x55

#define RD03D_MAX_FRAME_LEN    64

/* Example message IDs (PLACEHOLDERS) */
#define RD03D_MSG_PRESENCE     0x01
#define RD03D_MSG_DISTANCE     0x02

// /* Example payload layout (PLACEHOLDER) */
// typedef struct __attribute__((packed))
// {
//     uint8_t  msg_id;
//     uint8_t  len;
//     uint8_t  payload[64];   // previously was empty, only pointer
// } rd03d_frame_t;

//---- from documentation ----
// from: rd-03d_multi-target_trajectory_tracking_user_manual.pdf

// Each Target consists of 8 bytes:
typedef struct __attribute__((packed)) {
    int16_t x_mm;      // signed, bit15 = sign
    int16_t y_mm;      // signed, bit15 = sign
    int16_t speed_cms; // signed, cm/s
    uint16_t distance; // mm
} rd03d_target_t;
// Total payload size
// 3 targets × 8 bytes = 24 bytes

#define RD03D_FRAME_SIZE (4 + 24 + 2)

// static uint8_t frame_buf[RD03D_FRAME_SIZE];
// static uint8_t frame_pos;

// // Driver output structure
// typedef struct {
//     rd03d_target_t targets[3];
// } rd03d_frame_t;
//---- end from documentation ----
