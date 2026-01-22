/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
api_rd03d (HIGH LEVEL)

Owns:
  Distance validity
  Presence state machine
  Confidence filtering
  User-facing data model
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


#define RD03D_TRACKS 3

typedef struct
{
    bool     valid;        /* track considered active */
    uint8_t  confidence;   /* 0..255 */
    int16_t  x_mm;
    int16_t  y_mm;
    int16_t  speed_cms;
    uint16_t distance_mm;
    uint32_t last_seen_ms;
} rd03d_track_t;

typedef struct
{
    rd03d_track_t track[RD03D_TRACKS];
    bool          presence;       /* any track above threshold */
    uint32_t      rx_time_ms;
} rd03d_state_t;

typedef struct
{
    /* Tuning knobs (reasonable defaults recommended below) */
    uint16_t max_match_dist_mm;    /* association radius */
    uint8_t  conf_inc;             /* per matched frame */
    uint8_t  conf_dec;             /* per missed frame */
    uint8_t  conf_on;              /* presence threshold */
    uint8_t  conf_off;             /* drop threshold (hysteresis) */
    uint32_t stale_ms;             /* invalidate if not seen */
} rd03d_filter_cfg_t;

bool rd03d_api_init(const rd03d_filter_cfg_t *cfg);
void rd03d_api_poll(void);
bool rd03d_api_get_state(rd03d_state_t *out);

// typedef struct
// {
//     bool     presence;
//     uint16_t distance_mm;
//     uint8_t  confidence;
// } rd03d_state_t;

// bool rd03d_api_init(void);
// void rd03d_api_poll(void);
// bool rd03d_api_get_state(rd03d_state_t *out);

#ifdef __cplusplus
}
#endif

            // //---- from documentation ----
            // // from: rd-03d_multi-target_trajectory_tracking_user_manual.pdf

            // /*
            // Your API layer should:
            // Interpret (0,0,0,0) as “no target”
            // Maintain track persistence
            // Apply motion thresholds
            // Decide what “presence” means for your product
            // */
            // bool target_valid(const rd03d_target_t *t)
            // {
            //     return !(t->x_mm == 0 &&
            //             t->y_mm == 0 &&
            //             t->speed_cms == 0 &&
            //             t->distance == 0);
            // }


            // // 5. Recommended API abstraction (clean and future-proof)
            // typedef struct {
            //     bool    valid;
            //     int16_t x_mm;
            //     int16_t y_mm;
            //     int16_t speed_cms;
            //     uint16_t distance_mm;
            // } rd03d_object_t;

            // typedef struct {
            //     rd03d_object_t objects[3];
            //     uint32_t       timestamp_ms;
            // } rd03d_state_t;



