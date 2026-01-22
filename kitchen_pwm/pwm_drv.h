/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

// #include "stdio.h"
// #include "stdlib.h"
//#include "string.h"
#include "pico/stdio.h"

typedef enum {
    PWM_CH_R = 0,
    PWM_CH_G,
    PWM_CH_B,
    PWM_CH_W,
    PWM_CH_COUNT
} pwm_drv_channel_t;

typedef struct {
    uint gpio;
    uint slice;
    uint channel;
    bool active_low;
} pwm_drv_hw_t;

bool pwm_drv_init(uint32_t pwm_freq_hz, uint16_t pwm_wrap);

void pwm_drv_ch_set(pwm_drv_channel_t ch, uint16_t linear_level);
void pwm_drv_enable(bool enable);
