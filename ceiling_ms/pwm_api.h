/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t r;
    uint16_t g;
    uint16_t b;
    uint16_t w;
} rgbw16_t;

/* Init + regular polling */
bool pwm_mod_init(void);
void pwm_api_poll(void);

/* Immediate set (updates target; if fade inactive, applies on next poll) */
void pwm_rgbw_set(rgbw16_t color);
void pwm_rgbw_set_brightness(uint16_t brightness);
void pwm_rgbw_set8(uint8_t r, uint8_t g, uint8_t b, uint8_t w);

/* -------- Fade engine (zero allocations) --------
 * Fade is computed in pwm_rgbw_poll() from absolute_time_t; no IRQ required.
 */
void pwm_rgbw_fade_to(rgbw16_t color, uint32_t duration_ms);
void pwm_rgbw_fade_stop(bool snap_to_target);   // stop fade; optionally snap immediately
bool pwm_rgbw_is_fading(void);

/* -------- DDP frame mapping --------
 * You call this from your ddp_loop() when a frame arrives.
 */
typedef enum {
    DDP_FMT_RGBW8 = 0,   // 4 bytes: R,G,B,W (8-bit)
    DDP_FMT_RGB8W0,      // 3 bytes: R,G,B (W forced 0)
    DDP_FMT_RGBW16LE,    // 8 bytes: Rlo,Rhi,Glo,Ghi,Blo,Bhi,Wlo,Whi (16-bit LE)
} ddp_rgbw_format_t;

typedef struct {
    ddp_rgbw_format_t fmt;
    uint16_t channel_offset;  // 0-based offset within payload where R starts
    bool     use_fade;        // if true: apply fade on each frame (duration_ms)
    uint32_t frame_fade_ms;   // used when use_fade=true
} pwm_rgbw_ddp_cfg_t;

void pwm_rgbw_ddp_config(const pwm_rgbw_ddp_cfg_t* cfg);
bool pwm_rgbw_ddp_ingest(const uint8_t* payload, uint16_t payload_len);

/* -------- CLI integration (commands) --------
 * Provide a line (NUL-terminated); returns true if handled.
 * This is pure parsing + calling API; no dynamic memory.
 */
bool pwm_rgbw_cli_handle_line(const char* line);

/* Optional: status snapshot for CLI printing */
typedef struct {
    rgbw16_t current;
    rgbw16_t target;
    uint16_t brightness;
    bool fading;
    uint32_t fade_remaining_ms;
} pwm_rgbw_status_t;

pwm_rgbw_status_t pwm_rgbw_get_status(void);
bool pwm_rgbw_reconfigure(uint32_t pwm_freq_hz);

void pwm_led_set(uint16_t w);

