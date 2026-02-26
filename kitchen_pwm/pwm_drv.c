/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <stdio.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "config.h"
#include "pwm_drv.h"

// /* Pin definitions */ see config_kitchen.h
// #define PWM_LED_W    4
// #define PWM_LED_B    5
// #define PWM_LED_R    6
// #define PWM_LED_G    7

static pwm_drv_hw_t hw[PWM_CH_COUNT];

// LUT definition
#define GAMMA_LUT_BITS   9                  // 512 entries
#define GAMMA_LUT_SIZE   (1u << GAMMA_LUT_BITS)
#define GAMMA_LUT_SHIFT  (LINEAR_BITS - GAMMA_LUT_BITS) // 12 - 9 = 3
static uint16_t gamma_lut[GAMMA_LUT_SIZE];

/**
 * Initialize the gamma lookup table
 *  Linear domain: 12-bit (0 … 4095)
 *  LUT size: 512 entries → one entry per 8 linear steps
 *  LUT value: PWM duty (0 … PWM_MAX)
 *  Gamma: typically 2.2
 */
void gamma_lut_init(void)
{
    const float gamma = 2.2f;

    for (uint32_t i = 0; i < GAMMA_LUT_SIZE; i++) {

        uint32_t linear = i << GAMMA_LUT_SHIFT;

        if (linear == 0) {
            gamma_lut[i] = 0;
            continue;
        }

        float x = (float)linear / (float)LINEAR_MAX;
        float y = powf(x, gamma);

        uint32_t pwm = (uint32_t)(y * (PWM_MAX - PWM_MIN_ON) + 0.5f)
                       + PWM_MIN_ON;

        if (pwm > PWM_MAX)
            pwm = PWM_MAX;

        gamma_lut[i] = (uint16_t)pwm;
    }
}

// static void gamma_lut_init(void)
// {
//     const float gamma = 2.2f;

//     for (uint32_t i = 0; i < GAMMA_LUT_SIZE; i++) {
//         /* Map LUT index to linear domain */
//         uint32_t linear = i << GAMMA_LUT_SHIFT;   // 0..4095

//         float x = (float)linear / (float)LINEAR_MAX;
//         float y = powf(x, gamma);

//         uint32_t pwm = (uint32_t)(y * PWM_MAX + 0.5f);
//         if (pwm > PWM_MAX) pwm = PWM_MAX;

//         gamma_lut[i] = (uint16_t)pwm;
//     }
// }

/**
 * Convert a linear light level to a PWM level using gamma correction LUT
 * @param linear  Linear light level in [0..LINEAR_MAX]
 * @return        PWM level in [0..PWM_MAX]
 */
static inline uint16_t linear_to_pwm(uint16_t linear)
{
    /* Split index and fractional part */
    uint32_t idx  = linear >> GAMMA_LUT_SHIFT;                 // top 9 bits
    uint32_t frac = linear & ((1u << GAMMA_LUT_SHIFT) - 1);    // lower 3 bits

    if (idx >= GAMMA_LUT_SIZE - 1)
        return gamma_lut[GAMMA_LUT_SIZE - 1];

    uint32_t a = gamma_lut[idx];
    uint32_t b = gamma_lut[idx + 1];

    /* Linear interpolation between LUT entries */
    return (uint16_t)(a + ((b - a) * frac >> GAMMA_LUT_SHIFT));
}


// /**
//  * Convert a linear light level to a PWM level using approximate gamma (2.2) correction
//  * If you want perfect gamma: use a LUT (I strongly recommend this).
//  * @param linear  Linear light level in [0..LINEAR_MAX]
//  * @return        PWM level in [0..PWM_MAX]
//  */
// static inline uint16_t linear_to_pwm(uint16_t linear)
// {
//     /* Normalize to 0..1 in Q15 */
//     uint32_t x = (uint32_t)linear;

//     /* x^2.2 ≈ x^2 * sqrt(x) */
//     uint64_t x2 = (uint64_t)x * x >> 16;
//     uint64_t x3 = (x2 * x) >> 16;

//     /* map to PWM range */
//     uint32_t pwm = (uint32_t)((x3 * PWM_MAX) >> 16);
//     if (pwm > PWM_MAX) pwm = PWM_MAX;

//     return (uint16_t)pwm;
// }

/**
 * Set the PWM level from a linear light level (after set brightness)
 * @param ch             Channel to set
 * @param linear_level   Linear light level in [0..LINEAR_MAX]
 */
void pwm_drv_ch_set(pwm_drv_channel_t ch, uint16_t linear_level)
{
    uint16_t pwm = linear_to_pwm(linear_level);

    uint16_t out = hw[ch].active_low
        ? (PWM_WRAP - pwm)
        : pwm;

    if (ch == 3) {
        printf("White channel linear in: %u -> pwm: %u -> out: %u\n", linear_level, pwm, out);

    // if (ch == 0) {
    //     printf("Red channel linear in: %u -> pwm: %u -> out: %u\n", linear_level, pwm, out);
    // } else if (ch == 1) {
    //     printf("Grn channel linear in: %u -> pwm: %u -> out: %u\n", linear_level, pwm, out);
    }

    pwm_set_chan_level(hw[ch].slice, hw[ch].channel, out);
}

// void pwm_drv_ch_set(pwm_drv_channel_t ch, uint16_t level)
// {
//     uint16_t out = hw[ch].active_low ? (PWM_WRAP - level) : level;

//     if (ch == 0) {
//         printf("R level: %u -> %u\n", level, out);
//     }
//     pwm_set_chan_level(hw[ch].slice, hw[ch].channel, out);
// }





/**
 * Initialize the PWM driver
 * @param pwm_freq_hz  Desired PWM frequency in Hz
 * @param pwm_wrap     PWM wrap value (defines resolution)
 * @return             true on success, false on failure (e.g. invalid parameters)
 */
bool pwm_drv_init(uint32_t pwm_freq_hz, uint16_t pwm_wrap)
{
    hw[PWM_CH_R] = (pwm_drv_hw_t){ 
        .gpio = PWM_LED_R, 
        .slice = pwm_gpio_to_slice_num(PWM_LED_R), 
        .channel = pwm_gpio_to_channel(PWM_LED_R), 
        .active_low = true
    };
    hw[PWM_CH_G] = (pwm_drv_hw_t){
        .gpio = PWM_LED_G, 
        .slice = pwm_gpio_to_slice_num(PWM_LED_G), 
        .channel = pwm_gpio_to_channel(PWM_LED_G), 
        .active_low = true
    };
    hw[PWM_CH_B] = (pwm_drv_hw_t){
        .gpio = PWM_LED_B, 
        .slice = pwm_gpio_to_slice_num(PWM_LED_B), 
        .channel = pwm_gpio_to_channel(PWM_LED_B), 
        .active_low = true
    };
    hw[PWM_CH_W] = (pwm_drv_hw_t){
        .gpio = PWM_LED_W, 
        .slice = pwm_gpio_to_slice_num(PWM_LED_W), 
        .channel = pwm_gpio_to_channel(PWM_LED_W), 
        .active_low = false      // false for 1 led, true for rgbw
    };
    gamma_lut_init();


    uint32_t sys_clk = clock_get_hz(clk_sys);
    // float clk_div = (float)sys_clk / (pwm_freq_hz * (pwm_wrap + 1));
    double clk_div = (double)sys_clk / ((double)pwm_freq_hz * ((double)pwm_wrap + 1));

    if (clk_div < 1.0f || clk_div > 256.0f) {
        return false;
    }

    // list the used slices
    bool slices_used[NUM_PWM_SLICES] = {false};

    for (int i = 0; i < PWM_CH_COUNT; i++) {
        slices_used[hw[i].slice] = true;
        gpio_init(hw[i].gpio);
        bool level = hw[i].active_low ? 1 : 0;
        gpio_put(hw[i].gpio, level);   // set value before setting as output
        gpio_set_dir(hw[i].gpio, GPIO_OUT);
        // gpio_put(hw[i].gpio, level);   // HIGH = LED OFF (active-low)
    }

    // prepare only the used slices, only 2 in this case
    for (uint slice = 0; slice < NUM_PWM_SLICES; slice++) {
        if (!slices_used[slice]) continue;

        pwm_config cfg = pwm_get_default_config();
        pwm_config_set_wrap(&cfg, pwm_wrap);
        pwm_config_set_clkdiv(&cfg, (float)clk_div);
        pwm_init(slice, &cfg, false);
    }

    // sleep_ms(500);
    // slices prepared, now prepare the channels and enable
    for (int i = 0; i < PWM_CH_COUNT; i++) {
        pwm_drv_ch_set((pwm_drv_channel_t)i, 0);
        pwm_set_enabled(hw[i].slice, true);
        // need time to count up before changing GPIO function
        sleep_ms(2);
        gpio_set_function(hw[i].gpio, GPIO_FUNC_PWM);
    }
    return true;
}

/**
 * Used only for debugging: change frequency on the fly.
 */
void pwm_drv_enable(bool enable)
{
    for (int i = 0; i < PWM_CH_COUNT; i++) {
        pwm_set_enabled(hw[i].slice, enable);
    }
}

