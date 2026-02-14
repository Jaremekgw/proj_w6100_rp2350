/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "pwm_api.h"
#include "pwm_drv.h"
#include "config_kitchen.h"

#include "pico/time.h"     // absolute_time_t, get_absolute_time, absolute_time_diff_us
#include <string.h>
#include <ctype.h>

// see config_kitchen.h
// #define PWM_WRAP  4095    // 12-bit resolution
// #define PWM_FREQ  20000   // 20 kHz (no audible noise)


/* ---------- Internal state (static, zero allocations) ---------- */
// mnimal value for each color is 9, what is maximum value?
static rgbw16_t s_current;      // what is currently being output (logical, before brightness)
static rgbw16_t s_target;       // target value (logical, before brightness)

// static uint16_t s_brightness = PWM_WRAP;
static uint16_t g_brightness; // global brightness for all channels

/* Fade state */
static bool s_fade_active = false;
static rgbw16_t s_fade_start;
static rgbw16_t s_fade_end;
static absolute_time_t s_fade_t0;
static uint32_t s_fade_dur_ms = 0;

/* DDP config */
static pwm_rgbw_ddp_cfg_t s_ddp_cfg = {
    .fmt = DDP_FMT_RGBW8,
    .channel_offset = 0,
    .use_fade = false,
    .frame_fade_ms = 0
};


/* ---------- Helpers ---------- */
// static inline uint16_t clamp_u16(uint32_t v, uint16_t lo, uint16_t hi) {
//     if (v < lo) return lo;
//     if (v > hi) return hi;
//     return (uint16_t)v;
// }

static inline uint16_t scale8_to_wrap(uint8_t v) {
    /* exact-ish integer scaling to [0..PWM_WRAP] */
    return (uint16_t)((((uint32_t)v) * PWM_WRAP + 127u) / 255u);
}

static inline uint16_t scale16_to_wrap(uint16_t v16) {
    /* map 0..65535 -> 0..PWM_WRAP */
    return (uint16_t)((((uint32_t)v16) * PWM_WRAP + 32767u) / 65535u);
}

    /* brightness in range 0..LINEAR_MAX */
    // extern uint16_t g_brightness;
static inline uint16_t apply_brightness(uint16_t linear)
{
    return (uint16_t)(((uint32_t)linear * (g_brightness+1)) >> LINEAR_BITS);
}

static void apply_to_hw(const rgbw16_t* logical)
{
    pwm_drv_ch_set(PWM_CH_R, apply_brightness(logical->r));
    pwm_drv_ch_set(PWM_CH_G, apply_brightness(logical->g));
    pwm_drv_ch_set(PWM_CH_B, apply_brightness(logical->b));
    pwm_drv_ch_set(PWM_CH_W, apply_brightness(logical->w));
}

static rgbw16_t lerp_rgbw(rgbw16_t a, rgbw16_t b, uint32_t t_ms, uint32_t dur_ms)
{
    if (dur_ms == 0) return b;
    if (t_ms >= dur_ms) return b;

    /* Linear interpolation with integer math: a + (b-a)*t/dur */
    rgbw16_t out;
    #define LERP_ONE(_field) do { \
        int32_t da = (int32_t)b._field - (int32_t)a._field; \
        out._field = (uint16_t)((int32_t)a._field + (int32_t)((da * (int32_t)t_ms) / (int32_t)dur_ms)); \
    } while (0)

    LERP_ONE(r);
    LERP_ONE(g);
    LERP_ONE(b);
    LERP_ONE(w);
    #undef LERP_ONE

    return out;
}

/* ---------- Public API ---------- */
bool pwm_mod_init(void)
{
    s_current = (rgbw16_t){0};
    s_target  = (rgbw16_t){0};
    // s_brightness = PWM_WRAP;
    g_brightness = LINEAR_MAX;
    s_fade_active = false;

    if (!pwm_drv_init(PWM_FREQ, PWM_WRAP)) {
        return false;
    }

    apply_to_hw(&s_current);
    return true;
}


void pwm_rgbw_set(rgbw16_t color)
{
    s_target = color;
    if (!s_fade_active) {
        s_current = s_target;
        printf("Set s_current RGBW to: %u %u %u %u\n",
               s_current.r, s_current.g, s_current.b, s_current.w);

    } else {
        printf("Set s_target RGBW to: %u %u %u %u\n",
               s_target.r, s_target.g, s_target.b, s_target.w);

    }
}


void pwm_led_set(uint16_t w)
{
    rgbw16_t color = {
        .r = 0,
        .g = 0,
        .b = 0,
        .w = w,
    };
    s_target = color;
    if (!s_fade_active) {
        s_current = s_target;
        printf("Set s_current W to: %u\n", s_current.w);

    } else {
        printf("Set s_target W to: %u\n", s_target.w);

    }
}

// void pwm_rgbw_set8(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
// {
//     pwm_rgbw_set((rgbw16_t){
//         .r = scale8_to_wrap(r),
//         .g = scale8_to_wrap(g),
//         .b = scale8_to_wrap(b),
//         .w = scale8_to_wrap(w),
//     });
//         // pwm_rgbw_set((rgbw16_t){
//         //     .r = scale8_to_wrap((uint8_t)clamp_u16(r,0,255)),
//         //     .g = scale8_to_wrap((uint8_t)clamp_u16(g,0,255)),
//         //     .b = scale8_to_wrap((uint8_t)clamp_u16(b,0,255)),
//         //     .w = scale8_to_wrap((uint8_t)clamp_u16(w,0,255)),
//         // });
// }

void pwm_rgbw_set_brightness(uint16_t brightness)
{
    g_brightness = (brightness > LINEAR_MAX) ? LINEAR_MAX : brightness;
}

void pwm_rgbw_fade_to(rgbw16_t color, uint32_t duration_ms)
{
    /* Start fade from current output state (logical) */
    s_fade_start = s_current;
    s_fade_end = color;
    s_fade_dur_ms = duration_ms;
    s_fade_t0 = get_absolute_time();
    s_fade_active = (duration_ms != 0);

    s_target = color;

    printf("Set fade to s_target RGBW to: %u %u %u %u\n",
               s_target.r, s_target.g, s_target.b, s_target.w);

    if (!s_fade_active) {
        s_current = s_target;
    }
}

void pwm_rgbw_fade_stop(bool snap_to_target)
{
    s_fade_active = false;
    if (snap_to_target) {
        s_current = s_target;
    }
}

bool pwm_rgbw_is_fading(void)
{
    return s_fade_active;
}

void pwm_api_poll(void)
{
    static rgbw16_t last_target = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};

    if (s_fade_active) {
        // calculate time from t0
        int64_t us = absolute_time_diff_us(s_fade_t0, get_absolute_time());
        uint32_t t_ms = (us <= 0) ? 0u : (uint32_t)(us / 1000);

        s_current = lerp_rgbw(s_fade_start, s_fade_end, t_ms, s_fade_dur_ms);

        if (t_ms >= s_fade_dur_ms) {
            s_current = s_fade_end;
            s_fade_active = false;
        }
    } else {
        /* If target changed via pwm_rgbw_set while not fading, keep in sync */
        s_current = s_target;
    }

    if (memcmp(&s_current, &last_target, sizeof(rgbw16_t)) != 0) {
        last_target = s_current;
        printf("PWM target RGBW changed to: %u %u %u %u\n",
               s_current.r, s_current.g, s_current.b, s_current.w);
        apply_to_hw(&s_current);
    }
}

/**
 * Only to diagnose current status; no state changes.
 */
pwm_rgbw_status_t pwm_rgbw_get_status(void)
{
    pwm_rgbw_status_t st = {
        .current = s_current,
        .target = s_target,
        .brightness = g_brightness,
        .fading = s_fade_active,
        .fade_remaining_ms = 0
    };

    if (s_fade_active) {
        int64_t us = absolute_time_diff_us(s_fade_t0, get_absolute_time());
        uint32_t t_ms = (us <= 0) ? 0u : (uint32_t)(us / 1000);
        st.fade_remaining_ms = (t_ms >= s_fade_dur_ms) ? 0u : (s_fade_dur_ms - t_ms);
    }
    return st;
}

/**
 * Used only for debugging, check different PWM frequencies from telnet.
 */
bool pwm_rgbw_reconfigure(uint32_t pwm_freq_hz)
{
    /* stop PWM, re-init slices, restart.
       This is safe if you do it when you are not outputting critical patterns. */
    pwm_drv_enable(false);

    /* NOTE: pwm_wrap must match your internal scaling assumptions.
       If you change it, update PWM_WRAP constants or store wrap in state. */
    bool ok = pwm_drv_init(pwm_freq_hz, PWM_WRAP);
    pwm_drv_enable(true);

    return ok;
}

/* ---------- DDP mapping ---------- */
void pwm_rgbw_ddp_config(const pwm_rgbw_ddp_cfg_t* cfg)
{
    if (!cfg) return;
    s_ddp_cfg = *cfg;  // POD copy, no allocations
}

static bool ddp_extract_rgbw(const uint8_t* payload, uint16_t payload_len, rgbw16_t* out)
{
    if (!payload || !out) return false;
    if (s_ddp_cfg.channel_offset >= payload_len) return false;

    const uint16_t off = s_ddp_cfg.channel_offset;

    switch (s_ddp_cfg.fmt) {
        case DDP_FMT_RGBW8: {
            if ((uint32_t)off + 4u > payload_len) return false;
            out->r = scale8_to_wrap(payload[off + 0]);
            out->g = scale8_to_wrap(payload[off + 1]);
            out->b = scale8_to_wrap(payload[off + 2]);
            out->w = scale8_to_wrap(payload[off + 3]);
            return true;
        }
        case DDP_FMT_RGB8W0: {
            if ((uint32_t)off + 3u > payload_len) return false;
            out->r = scale8_to_wrap(payload[off + 0]);
            out->g = scale8_to_wrap(payload[off + 1]);
            out->b = scale8_to_wrap(payload[off + 2]);
            out->w = 0;
            return true;
        }
        case DDP_FMT_RGBW16LE: {
            if ((uint32_t)off + 8u > payload_len) return false;
            uint16_t r16 = (uint16_t)payload[off + 0] | ((uint16_t)payload[off + 1] << 8);
            uint16_t g16 = (uint16_t)payload[off + 2] | ((uint16_t)payload[off + 3] << 8);
            uint16_t b16 = (uint16_t)payload[off + 4] | ((uint16_t)payload[off + 5] << 8);
            uint16_t w16 = (uint16_t)payload[off + 6] | ((uint16_t)payload[off + 7] << 8);
            out->r = scale16_to_wrap(r16);
            out->g = scale16_to_wrap(g16);
            out->b = scale16_to_wrap(b16);
            out->w = scale16_to_wrap(w16);
            return true;
        }
        default:
            return false;
    }
}

bool pwm_rgbw_ddp_ingest(const uint8_t* payload, uint16_t payload_len)
{
    rgbw16_t c;
    if (!ddp_extract_rgbw(payload, payload_len, &c)) {
        return false;
    }

    if (s_ddp_cfg.use_fade && s_ddp_cfg.frame_fade_ms > 0) {
        pwm_rgbw_fade_to(c, s_ddp_cfg.frame_fade_ms);
    } else {
        pwm_rgbw_set(c);
        pwm_rgbw_fade_stop(false);
    }
    return true;
}

/* ---------- CLI (no allocations) ----------
 * Commands:
 *   pwm set <r> <g> <b> <w>            (0..4095)
 *   pwm set8 <r> <g> <b> <w>           (0..255)
 *   pwm fade <ms> <r> <g> <b> <w>      (0..4095)
 *   pwm fade8 <ms> <r> <g> <b> <w>     (0..255)
 *   pwm bright <val>                   (0..4095)
 *   pwm stop [snap|nosnap]
 *   pwm ddp fmt <rgbw8|rgb8|rgbw16le>
 *   pwm ddp off <n>
 *   pwm ddp fade <ms|0>
 *   pwm status
 *
 * This file only parses and calls the API; printing is up to caller.
 */

static const char* skip_ws(const char* s) { while (s && *s && isspace((unsigned char)*s)) s++; return s; }

static bool __attribute__((unused)) read_u32(const char** ps, uint32_t* out)
{
    const char* s = skip_ws(*ps);
    if (!s || !*s || !isdigit((unsigned char)*s)) return false;

    uint32_t v = 0;
    while (*s && isdigit((unsigned char)*s)) {
        v = v * 10u + (uint32_t)(*s - '0');
        s++;
    }
    *ps = s;
    *out = v;
    return true;
}

// static bool match_tok(const char** ps, const char* tok)
// {
//     const char* s = skip_ws(*ps);
//     size_t n = strlen(tok);
//     if (strncmp(s, tok, n) != 0) return false;
//     if (s[n] && !isspace((unsigned char)s[n])) return false;
//     *ps = s + n;
//     return true;
// }

// bool pwm_rgbw_cli_handle_line(const char* line)
// {
//     if (!line) return false;

//     const char* p = line;
//     if (!match_tok(&p, "pwm")) return false;

//     if (match_tok(&p, "set")) {
//         uint32_t r,g,b,w;
//         if (!read_u32(&p,&r) || !read_u32(&p,&g) || !read_u32(&p,&b) || !read_u32(&p,&w)) return true;
//         pwm_rgbw_set((rgbw16_t){
//             .r = clamp_u16(r,0,PWM_WRAP),
//             .g = clamp_u16(g,0,PWM_WRAP),
//             .b = clamp_u16(b,0,PWM_WRAP),
//             .w = clamp_u16(w,0,PWM_WRAP),
//         });
//         pwm_rgbw_fade_stop(false);
//         return true;
//     }

//     if (match_tok(&p, "set8")) {
//         uint32_t r,g,b,w;
//         if (!read_u32(&p,&r) || !read_u32(&p,&g) || !read_u32(&p,&b) || !read_u32(&p,&w)) return true;
//         pwm_rgbw_set((rgbw16_t){
//             .r = scale8_to_wrap((uint8_t)clamp_u16(r,0,255)),
//             .g = scale8_to_wrap((uint8_t)clamp_u16(g,0,255)),
//             .b = scale8_to_wrap((uint8_t)clamp_u16(b,0,255)),
//             .w = scale8_to_wrap((uint8_t)clamp_u16(w,0,255)),
//         });
//         pwm_rgbw_fade_stop(false);
//         return true;
//     }

//     if (match_tok(&p, "fade")) {
//         uint32_t ms,r,g,b,w;
//         if (!read_u32(&p,&ms) || !read_u32(&p,&r) || !read_u32(&p,&g) || !read_u32(&p,&b) || !read_u32(&p,&w)) return true;
//         pwm_rgbw_fade_to((rgbw16_t){
//             .r = clamp_u16(r,0,PWM_WRAP),
//             .g = clamp_u16(g,0,PWM_WRAP),
//             .b = clamp_u16(b,0,PWM_WRAP),
//             .w = clamp_u16(w,0,PWM_WRAP),
//         }, ms);
//         return true;
//     }

//     if (match_tok(&p, "fade8")) {
//         uint32_t ms,r,g,b,w;
//         if (!read_u32(&p,&ms) || !read_u32(&p,&r) || !read_u32(&p,&g) || !read_u32(&p,&b) || !read_u32(&p,&w)) return true;
//         pwm_rgbw_fade_to((rgbw16_t){
//             .r = scale8_to_wrap((uint8_t)clamp_u16(r,0,255)),
//             .g = scale8_to_wrap((uint8_t)clamp_u16(g,0,255)),
//             .b = scale8_to_wrap((uint8_t)clamp_u16(b,0,255)),
//             .w = scale8_to_wrap((uint8_t)clamp_u16(w,0,255)),
//         }, ms);
//         return true;
//     }

//     if (match_tok(&p, "bright")) {
//         uint32_t v;
//         if (!read_u32(&p,&v)) return true;
//         pwm_rgbw_set_brightness(clamp_u16(v, 0, PWM_WRAP));
//         return true;
//     }

//     if (match_tok(&p, "stop")) {
//         bool snap = false;
//         if (match_tok(&p, "snap")) snap = true;
//         if (match_tok(&p, "nosnap")) snap = false;
//         pwm_rgbw_fade_stop(snap);
//         return true;
//     }

//     if (match_tok(&p, "ddp")) {
//         if (match_tok(&p, "fmt")) {
//             if (match_tok(&p, "rgbw8"))      s_ddp_cfg.fmt = DDP_FMT_RGBW8;
//             else if (match_tok(&p, "rgb8"))  s_ddp_cfg.fmt = DDP_FMT_RGB8W0;
//             else if (match_tok(&p, "rgbw16le")) s_ddp_cfg.fmt = DDP_FMT_RGBW16LE;
//             pwm_rgbw_ddp_config(&s_ddp_cfg);
//             return true;
//         }
//         if (match_tok(&p, "off")) {
//             uint32_t v;
//             if (!read_u32(&p,&v)) return true;
//             s_ddp_cfg.channel_offset = (uint16_t)v;
//             pwm_rgbw_ddp_config(&s_ddp_cfg);
//             return true;
//         }
//         if (match_tok(&p, "fade")) {
//             uint32_t ms;
//             if (!read_u32(&p,&ms)) return true;
//             s_ddp_cfg.use_fade = (ms != 0);
//             s_ddp_cfg.frame_fade_ms = ms;
//             pwm_rgbw_ddp_config(&s_ddp_cfg);
//             return true;
//         }
//         return true;
//     }

//     if (match_tok(&p, "status")) {
//         /* handled, caller can print pwm_rgbw_get_status() */
//         return true;
//     }

//     return true; /* "pwm" unknown subcommand: treated as handled for CLI UX */
// }



















// static rgbw16_t target;
// static uint16_t brightness = PWM_WRAP;

// bool pwm_rgbw_init(void)
// {
//     target = (rgbw16_t){0};
//     brightness = PWM_WRAP;
//     return pwm_rgbw_drv_init(PWM_FREQ, PWM_WRAP);
// }

// void pwm_rgbw_set(rgbw16_t color)
// {
//     target = color;
// }

// void pwm_rgbw_set_brightness(uint16_t b)
// {
//     brightness = b > PWM_WRAP ? PWM_WRAP : b;
// }

// void pwm_rgbw_poll(void)
// {
//     pwm_rgbw_drv_set_raw(PWM_CH_R, (target.r * brightness) >> 12);
//     pwm_rgbw_drv_set_raw(PWM_CH_G, (target.g * brightness) >> 12);
//     pwm_rgbw_drv_set_raw(PWM_CH_B, (target.b * brightness) >> 12);
//     pwm_rgbw_drv_set_raw(PWM_CH_W, (target.w * brightness) >> 12);
// }
