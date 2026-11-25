/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>  // need for printf in debug
#include <string.h>

#include "hardware/gpio.h"  // for UART pin config
#include "pico/time.h"

#include "config.h"
#include "ms72sf1_drv.h"
#include "ms72sf1_cli.h"
#include "ms72sf1_api.h"
#include "utility.h"
#include "dbg_api.h"

// #ifndef MS72SF1_UART
// #error "MS72SF1_UART not defined"
// #endif

// #ifndef MS72SF1_TX_PIN
// #error "MS72SF1_UART_TX_PIN not defined"
// #endif

// #ifndef MS72SF1_RX_PIN
// #error "MS72SF1_UART_RX_PIN not defined"
// #endif

// #ifndef MS72SF1_BAUDRATE
// #error "MS72SF1_BAUDRATE not defined"
// #endif

#if !defined(MS72SF1_UART_INST) || \
    !defined(MS72SF1_UART_FUNC) || \
    !defined(MS72SF1_UART_RX_PIN) || \
    !defined(MS72SF1_UART_TX_PIN) || \
    !defined(MS72SF1_BAUDRATE)
#error "MS72SF1 UART configuration macros are not fully defined"
#endif


/* ---------- RD-03D sign encoding decoder ----------
 * Manual example indicates:
 * - MSB=1 => positive, magnitude = raw - 0x8000
 * - MSB=0 => negative, magnitude = raw & 0x7FFF, value = -magnitude
 * See example decode on page 19 :contentReference[oaicite:3]{index=3}
 */
// static inline int16_t ms72sf1_decode_signmag(uint16_t raw)
// {
//     uint16_t mag = (uint16_t)(raw & 0x7FFF);
//     if (raw & 0x8000)
//         return (int16_t)mag;        /* positive */
//     return (int16_t)(-(int16_t)mag); /* negative */
// }

typedef struct
{
    bool     present;
    int16_t  x_mm;
    int16_t  y_mm;
    int16_t  v_cms;
    uint16_t dist_mm;
} det_t;

static ms72sf1_filter_cfg_t s_cfg;
static ms72sf1_state_t      s_state;
static bool               s_state_valid;

void ms72sf1_print_raw_data(ms72sf1_data_t *data);


/* ---------- helpers ---------- */
static inline uint32_t now_ms(void)
{
    return (uint32_t)to_ms_since_boot(get_absolute_time());
}

static inline uint32_t u32_abs_diff(uint32_t a, uint32_t b)
{
    return (a > b) ? (a - b) : (b - a);
}

static inline uint32_t sq_u32(int32_t v)
{
    return (uint32_t)(v * v);
}

static uint32_t dist2_mm(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    int32_t dx = (int32_t)x1 - (int32_t)x2;
    int32_t dy = (int32_t)y1 - (int32_t)y2;
    return sq_u32(dx) + sq_u32(dy);
}

/* ---------- init ---------- */
bool ms72sf1_api_init(const ms72sf1_filter_cfg_t *cfg)
{
    uart_cfg_t uart_cfg = {
        .instance = MS72SF1_UART_INST,
        .func = MS72SF1_UART_FUNC,
        .rx_pin = MS72SF1_UART_RX_PIN,
        .tx_pin = MS72SF1_UART_TX_PIN,
        .baudrate = MS72SF1_BAUDRATE
    };
    /* Defaults chosen for 10 Hz report rate (manual typical) :contentReference[oaicite:4]{index=4} */
    ms72sf1_filter_cfg_t def = {
        .max_match_dist_mm = 600,  /* targets shouldn't jump farther than this frame-to-frame */
        .conf_inc          = 35,
        .conf_dec          = 15,
        .conf_on           = 120,
        .conf_off          = 60,
        .stale_ms          = 1500,
    };

    s_cfg = cfg ? *cfg : def;

    memset(&s_state, 0, sizeof(s_state));
    s_state_valid = false;


    return ms72sf1_drv_init(&uart_cfg);
    // sleep_ms(1); /* let UART settle */
    // ms72sf1_drv_write_raw(multi_target_cmd, sizeof(multi_target_cmd));
    // return true;
}

/* ---------- tracking update ----------
 * Because RD-03D provides no stable IDs, object order may swap.
 * We do nearest-neighbour association into 3 persistent tracks.
 */
static void update_tracks(const det_t det[MS72SF1_OBJECT_SLOTS], uint32_t t_ms)
{
    bool det_used[MS72SF1_OBJECT_SLOTS] = {false, false, false};

    /* 1) Attempt to match existing active tracks */
    for (int ti = 0; ti < MS72SF1_TRACKS; ti++)
    {
        ms72sf1_track_t *tr = &s_state.track[ti];

        /* Decay if stale or not yet valid */
        bool active = tr->valid;

        int best_di = -1;
        uint32_t best_d2 = 0xFFFFFFFFu;

        if (active)
        {
            for (int di = 0; di < MS72SF1_OBJECT_SLOTS; di++)
            {
                if (!det[di].present || det_used[di])
                    continue;

                uint32_t d2 = dist2_mm(tr->x_mm, tr->y_mm, det[di].x_mm, det[di].y_mm);
                uint32_t max_d2 = (uint32_t)s_cfg.max_match_dist_mm * (uint32_t)s_cfg.max_match_dist_mm;
                if (d2 <= max_d2 && d2 < best_d2)
                {
                    best_d2 = d2;
                    best_di = di;
                }
            }
        }

        if (best_di >= 0)
        {
            /* Matched: update state, increase confidence */
            const det_t *d = &det[best_di];
            det_used[best_di] = true;

            tr->x_mm = d->x_mm;
            tr->y_mm = d->y_mm;
            tr->speed_cms = d->v_cms;
            tr->distance_mm = d->dist_mm;
            tr->last_seen_ms = t_ms;

            uint16_t c = (uint16_t)tr->confidence + s_cfg.conf_inc;
            tr->confidence = (c > 255u) ? 255u : (uint8_t)c;
            tr->valid = (tr->confidence >= s_cfg.conf_off);
        }
        else
        {
            /* Not matched: decay confidence */
            if (tr->confidence > s_cfg.conf_dec)
                tr->confidence = (uint8_t)(tr->confidence - s_cfg.conf_dec);
            else
                tr->confidence = 0;

            /* Invalidate if too old */
            if (tr->last_seen_ms == 0 || u32_abs_diff(t_ms, tr->last_seen_ms) > s_cfg.stale_ms)
                tr->confidence = 0;

            tr->valid = (tr->confidence >= s_cfg.conf_off);
        }
    }

    /* 2) Assign remaining detections to empty/low-confidence tracks */
    for (int di = 0; di < MS72SF1_OBJECT_SLOTS; di++)
    {
        if (!det[di].present || det_used[di])
            continue;

        int best_ti = -1;
        uint8_t best_conf = 255;

        for (int ti = 0; ti < MS72SF1_TRACKS; ti++)
        {
            ms72sf1_track_t *tr = &s_state.track[ti];
            if (tr->confidence < best_conf)
            {
                best_conf = tr->confidence;
                best_ti = ti;
            }
        }

        if (best_ti >= 0)
        {
            ms72sf1_track_t *tr = &s_state.track[best_ti];
            tr->x_mm = det[di].x_mm;
            tr->y_mm = det[di].y_mm;
            tr->speed_cms = det[di].v_cms;
            tr->distance_mm = det[di].dist_mm;
            tr->last_seen_ms = t_ms;

            /* bootstrap confidence */
            uint16_t c = (uint16_t)tr->confidence + s_cfg.conf_inc;
            tr->confidence = (c > 255u) ? 255u : (uint8_t)c;
            tr->valid = (tr->confidence >= s_cfg.conf_off);

            det_used[di] = true;
        }
    }

    /* 3) Presence derived with hysteresis */
    bool any_on = false;
    for (int ti = 0; ti < MS72SF1_TRACKS; ti++)
    {
        if (s_state.track[ti].confidence >= s_cfg.conf_on)
        {
            any_on = true;
            break;
        }
    }

    if (any_on)
        s_state.presence = true;
    else
    {
        /* turn off only when all below conf_off */
        bool any_off_hold = false;
        for (int ti = 0; ti < MS72SF1_TRACKS; ti++)
        {
            if (s_state.track[ti].confidence >= s_cfg.conf_off)
            {
                any_off_hold = true;
                break;
            }
        }
        s_state.presence = any_off_hold ? s_state.presence : false;
    }
}

/* ---------- poll ---------- */
void ms72sf1_api_poll(void)
{
    // ms72sf1_drv_poll(); - call it from main loop, not here, to keep latency low and avoid starving main loop

    ms72sf1_data_t frame;
    if (!ms72sf1_drv_get_ready(&frame))
        return;

    // For debugging: print raw data on every received frame
    // ms72sf1_drv_debug_print_raw_data();
    ms72sf1_print_raw_data(&frame);
    dbg_send_sensor_data(now_ms(), &frame, sizeof(frame));

    det_t det[MS72SF1_OBJECT_SLOTS] = {0};

    for (int i = 0; i < MS72SF1_OBJECT_SLOTS; i++)
    {
        // const ms72sf1_object_raw_t *o = &f.report.obj[i];
        const ms72sf1_target_raw_t *target = &frame.target[i];

        if (target->active == false) /* no target in this slot */
        {
            det[i].present = false;
            continue;
        }

        det[i].present = true;
        det[i].x_mm  = target->x_raw; // ms72sf1_decode_signmag(o->x_raw);
        det[i].y_mm  = target->y_raw; // ms72sf1_decode_signmag(o->y_raw);
        det[i].v_cms = target->v_raw; // ms72sf1_decode_signmag(o->v_raw);
        det[i].dist_mm = target->dist_mm; // o->dist_mm;
    }

    s_state.rx_time_ms = frame.rx_time_ms;
    update_tracks(det, frame.rx_time_ms);
    s_state_valid = true;
}

bool ms72sf1_api_get_state(ms72sf1_state_t *out)
{
    if (!out || !s_state_valid)
        return false;

    *out = s_state;
    return true;
}

// --- Additional helper for debugging : print current state to console ---
int ms72sf1_api_print_state(char *msg, size_t msg_max_sz)
{
    if (msg == NULL || msg_max_sz == 0) {
        return -1;
    }

    char *cursor = msg;
    size_t remaining = msg_max_sz;

    ms72sf1_state_t *state = &s_state;

    printf("---- MS72SF1 STATE ----\r\n");
    printf("state_valid: %d\r\n", s_state_valid);
    printf("presence: %u\r\n", state->presence);
    printf("rx_time_ms: %u\r\n", state->rx_time_ms);
    // next will be s_state.track[0..2]
    printf("----------------------\r\n");

    msg_printf(&cursor, &remaining,
               "MS72SF1 state:\n");
    msg_printf(&cursor, &remaining,
               "  presence      : %s\n",
               state->presence ? "YES" : "NO");
    msg_printf(&cursor, &remaining,
               "  rx_time_ms    : %lu\n",
               (unsigned long)state->rx_time_ms);

    for (int i = 0; i < MS72SF1_TRACKS; i++) {
        ms72sf1_track_t *t = &state->track[i];

        msg_printf(&cursor, &remaining,
                   "  Track %d:\n", i);

        msg_printf(&cursor, &remaining,
                   "    valid       : %s\n",
                   t->valid ? "YES" : "NO");

        if (!t->valid) {
            continue;
        }

        msg_printf(&cursor, &remaining,
                   "    confidence  : %u\n",
                   (unsigned)t->confidence);

        msg_printf(&cursor, &remaining,
                   "    x_mm        : %d\n",
                   (int)t->x_mm);

        msg_printf(&cursor, &remaining,
                   "    y_mm        : %d\n",
                   (int)t->y_mm);

        msg_printf(&cursor, &remaining,
                   "    speed_cms   : %d\n",
                   (int)t->speed_cms);

        msg_printf(&cursor, &remaining,
                   "    distance_mm : %u\n",
                   (unsigned)t->distance_mm);

        msg_printf(&cursor, &remaining,
                   "    last_seen_ms: %lu\n",
                   (unsigned long)t->last_seen_ms);
    }

    return (int)(msg_max_sz - remaining);
}

void ms72sf1_print_raw_data(ms72sf1_data_t *data)
{
    if (!data || !data->ready) {
        printf("[MS72SF1] no raw data available\n");
        return;
    }

    printf("\r\n[MS72SF1][RAW DATA] t=%08lu ms", (unsigned long)data->rx_time_ms);
    for (int i = 0; i < MS72SF1_OBJECT_SLOTS; i++) {
        const ms72sf1_target_raw_t *t = &data->target[i];
        if (t->active) {
            printf("  t_%d x=%dmm y=%dmm v=%dcm/s d=%umm",
                i,
                (int)t->x_raw,
                (int)t->y_raw,
                (int)t->v_raw,
                (unsigned)t->dist_mm);
        // } else {
        //     printf("  t_%d <empty>", i);
        //     // continue;
        }
    }
}
