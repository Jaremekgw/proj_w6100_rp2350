/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


// #include <stdio.h>
#include <string.h>
// #include "pico/stdlib.h"
// #include "hardware/uart.h"
// #include "hardware/gpio.h"
#include "pico/time.h"

#include "rd03d_drv.h"
#include "rd03d_api.h"

/* ---------- RD-03D sign encoding decoder ----------
 * Manual example indicates:
 * - MSB=1 => positive, magnitude = raw - 0x8000
 * - MSB=0 => negative, magnitude = raw & 0x7FFF, value = -magnitude
 * See example decode on page 19 :contentReference[oaicite:3]{index=3}
 */
static inline int16_t rd03d_decode_signmag(uint16_t raw)
{
    uint16_t mag = (uint16_t)(raw & 0x7FFF);
    if (raw & 0x8000)
        return (int16_t)mag;        /* positive */
    return (int16_t)(-(int16_t)mag); /* negative */
}

typedef struct
{
    bool     present;
    int16_t  x_mm;
    int16_t  y_mm;
    int16_t  v_cms;
    uint16_t dist_mm;
} det_t;

static rd03d_filter_cfg_t s_cfg;
static rd03d_state_t      s_state;
static bool               s_state_valid;

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

static bool det_is_zero(const rd03d_object_raw_t *o)
{
    return (o->x_raw == 0 && o->y_raw == 0 && o->v_raw == 0 && o->dist_mm == 0);
}

/* ---------- init ---------- */
bool rd03d_api_init(const rd03d_filter_cfg_t *cfg)
{
    /* Defaults chosen for 10 Hz report rate (manual typical) :contentReference[oaicite:4]{index=4} */
    rd03d_filter_cfg_t def = {
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

    return rd03d_drv_init();
}

/* ---------- tracking update ----------
 * Because RD-03D provides no stable IDs, object order may swap.
 * We do nearest-neighbour association into 3 persistent tracks.
 */
static void update_tracks(const det_t det[RD03D_OBJECT_SLOTS], uint32_t t_ms)
{
    bool det_used[RD03D_OBJECT_SLOTS] = {false, false, false};

    /* 1) Attempt to match existing active tracks */
    for (int ti = 0; ti < RD03D_TRACKS; ti++)
    {
        rd03d_track_t *tr = &s_state.track[ti];

        /* Decay if stale or not yet valid */
        bool active = tr->valid;

        int best_di = -1;
        uint32_t best_d2 = 0xFFFFFFFFu;

        if (active)
        {
            for (int di = 0; di < RD03D_OBJECT_SLOTS; di++)
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
    for (int di = 0; di < RD03D_OBJECT_SLOTS; di++)
    {
        if (!det[di].present || det_used[di])
            continue;

        int best_ti = -1;
        uint8_t best_conf = 255;

        for (int ti = 0; ti < RD03D_TRACKS; ti++)
        {
            rd03d_track_t *tr = &s_state.track[ti];
            if (tr->confidence < best_conf)
            {
                best_conf = tr->confidence;
                best_ti = ti;
            }
        }

        if (best_ti >= 0)
        {
            rd03d_track_t *tr = &s_state.track[best_ti];
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
    for (int ti = 0; ti < RD03D_TRACKS; ti++)
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
        for (int ti = 0; ti < RD03D_TRACKS; ti++)
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
void rd03d_api_poll(void)
{
    rd03d_drv_poll();

    rd03d_frame_t f;
    if (!rd03d_drv_get_frame(&f))
        return;

    det_t det[RD03D_OBJECT_SLOTS] = {0};

    for (int i = 0; i < RD03D_OBJECT_SLOTS; i++)
    {
        const rd03d_object_raw_t *o = &f.report.obj[i];

        if (det_is_zero(o))
        {
            det[i].present = false;
            continue;
        }

        det[i].present = true;
        det[i].x_mm  = rd03d_decode_signmag(o->x_raw);
        det[i].y_mm  = rd03d_decode_signmag(o->y_raw);
        det[i].v_cms = rd03d_decode_signmag(o->v_raw);
        det[i].dist_mm = o->dist_mm;
    }

    s_state.rx_time_ms = f.rx_time_ms;
    update_tracks(det, f.rx_time_ms);
    s_state_valid = true;
}

bool rd03d_api_get_state(rd03d_state_t *out)
{
    if (!out || !s_state_valid)
        return false;

    *out = s_state;
    return true;
}



