/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "ms72sf1_drv.h"
// #include "ms72sf1_cli.h"

/* Inherited RD-03D protocol and commands; not yet compatible with MS72SF1. */


// #define _MS72SF1_DEBUG_

#ifdef _MS72SF1_DEBUG_
bool debug_flag_ms72sf1_raw = false; // set to true to enable debug prints in driver
#endif
bool debug_flag_ms72sf1_print = false;

uart_inst_t *ms72sf1_uart = NULL;

/**
 *  RX_BUF: 0xAA 0xFF 0x03 0x00                   Header
 *  0x05 0x01 0x19 0x82 0x00 0x00 0x68 0x01      target1
 *  0xE3 0x81 0x33 0x88 0x20 0x80 0x68 0x01      target2
 *  0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00      target3
 *  0x55 0xCC                                    Tail
 */

/*

https://www.electroniclinic.com/rd-03d-mmwave-radar-multi-human-tracking-with-distance-speed-positioning/#google_vignette


        target1_x = (RX_BUF[4] | (RX_BUF[5] << 8)) - 0x200;
        target1_y = (RX_BUF[6] | (RX_BUF[7] << 8)) - 0x8000;
        target1_speed = (RX_BUF[8] | (RX_BUF[9] << 8)) - 0x10;
        target1_distance_res = (RX_BUF[10] | (RX_BUF[11] << 8));
        target1_distance = sqrt(pow(target1_x, 2) + pow(target1_y, 2));
        target1_angle = atan2(target1_y, target1_x) * 180.0 / PI;

	version 2
        // Extract data for Target 1
        target1_x = (RX_BUF[4] | (RX_BUF[5] << 8)) - 0x200;
        target1_y = (RX_BUF[6] | (RX_BUF[7] << 8)) - 0x8000;
        target1_speed = (RX_BUF[8] | (RX_BUF[9] << 8)) - 0x10;
        target1_distance_res = (RX_BUF[10] | (RX_BUF[11] << 8));
        target1_distance = sqrt(pow(target1_x, 2) + pow(target1_y, 2)) / 10.0;
        target1_angle = atan2(target1_y, target1_x) * 180.0 / PI;


// Multi-Target Detection Command
uint8_t Multi_Target_Detection_CMD[12] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0x90, 0x00, 0x04, 0x03, 0x02, 0x01};

    // Send multi-target detection command
    Serial1.write(Multi_Target_Detection_CMD, sizeof(Multi_Target_Detection_CMD));
    delay(200);

        // Extract data for Target 1
        target1_x = (RX_BUF[4] | (RX_BUF[5] << 8)) - 0x200;
        target1_y = (RX_BUF[6] | (RX_BUF[7] << 8)) - 0x8000;
        target1_speed = (RX_BUF[8] | (RX_BUF[9] << 8)) - 0x10;
        target1_distance_res = (RX_BUF[10] | (RX_BUF[11] << 8));
        float target1_distance = sqrt(pow(target1_x, 2) + pow(target1_y, 2));
        float target1_angle = atan2(target1_y, target1_x) * 180.0 / PI;

        // Extract data for Target 2
        target2_x = (RX_BUF[12] | (RX_BUF[13] << 8)) - 0x200;
        target2_y = (RX_BUF[14] | (RX_BUF[15] << 8)) - 0x8000;
        target2_speed = (RX_BUF[16] | (RX_BUF[17] << 8)) - 0x10;
        target2_distance_res = (RX_BUF[18] | (RX_BUF[19] << 8));
        float target2_distance = sqrt(pow(target2_x, 2) + pow(target2_y, 2));
        float target2_angle = atan2(target2_y, target2_x) * 180.0 / PI;

        // Extract data for Target 3
        target3_x = (RX_BUF[20] | (RX_BUF[21] << 8)) - 0x200;
        target3_y = (RX_BUF[22] | (RX_BUF[23] << 8)) - 0x8000;
        target3_speed = (RX_BUF[24] | (RX_BUF[25] << 8)) - 0x10;
        target3_distance_res = (RX_BUF[26] | (RX_BUF[27] << 8));
        float target3_distance = sqrt(pow(target3_x, 2) + pow(target3_y, 2));
        float target3_angle = atan2(target3_y, target3_x) * 180.0 / PI;

*/

/*  LD2450
	https://github.com/RBEGamer/HLK-LD2450
*/

#define MS72SF1_HDR0 0xAA
#define MS72SF1_HDR1 0xFF
#define MS72SF1_HDR2 0x03
#define MS72SF1_HDR3 0x00
#define MS72SF1_TAIL0 0x55
#define MS72SF1_TAIL1 0xCC

uint8_t single_target_cmd[12] = {
    0xFD, 0xFC, 0xFB, 0xFA,
    0x02, 0x00,
    0x80, 0x00,
    0x04, 0x03, 0x02, 0x01
};

uint8_t multi_target_cmd[12] = {
    0xFD, 0xFC, 0xFB, 0xFA,
    0x02, 0x00,
    0x90, 0x00,
    0x04, 0x03, 0x02, 0x01
};

#define MS72SF1_PAYLOAD_BYTES  (MS72SF1_OBJECT_SLOTS * sizeof(ms72sf1_object_raw_t))
#define MS72SF1_FRAME_BYTES    (4u + MS72SF1_PAYLOAD_BYTES + 2u)

#ifndef MS72SF1_RX_RING_SIZE
/* Must be power-of-two for mask indexing, basic frame is 32 bytes */
#define MS72SF1_RX_RING_SIZE 64
#endif

#if (MS72SF1_RX_RING_SIZE & (MS72SF1_RX_RING_SIZE - 1)) != 0
#error "MS72SF1_RX_RING_SIZE must be a power-of-two"
#endif

static uint8_t  s_rx[MS72SF1_RX_RING_SIZE];
static uint16_t s_head, s_tail;

// static ms72sf1_frame_t s_last_frame;

ms72sf1_data_t s_last_data;

#ifdef _MS72SF1_DEBUG_
static bool rx_stream_started = false;
#endif
/* ---------- ring buffer helpers ---------- */
static inline uint16_t rb_count(void)
{
    return (uint16_t)((s_head - s_tail) & (MS72SF1_RX_RING_SIZE - 1));
}

static inline void rb_push(uint8_t b)
{
#ifdef _MS72SF1_DEBUG_
    if (debug_flag_ms72sf1_raw) {
        if (rx_stream_started) {
            printf(", %02X", b); 
        } else {
            printf("\r\n[MS72SF1 RX] %02X", b);
            rx_stream_started = true;
        }
    }
#endif

    s_rx[s_head] = b;
    s_head = (uint16_t)((s_head + 1) & (MS72SF1_RX_RING_SIZE - 1));
    /* Overrun policy: drop oldest */
    if (s_head == s_tail)
        s_tail = (uint16_t)((s_tail + 1) & (MS72SF1_RX_RING_SIZE - 1));
}

static inline uint8_t rb_peek(uint16_t idx_from_tail)
{
    uint16_t idx = (uint16_t)((s_tail + idx_from_tail) & (MS72SF1_RX_RING_SIZE - 1));
    return s_rx[idx];
}

static inline void rb_drop(uint16_t n)
{
    s_tail = (uint16_t)((s_tail + n) & (MS72SF1_RX_RING_SIZE - 1));
}

static inline void rb_read_bytes(uint8_t *dst, uint16_t n)
{
    for (uint16_t i = 0; i < n; i++)
        dst[i] = rb_peek(i);
    rb_drop(n);
}

/* ---------- UART init ---------- */
bool ms72sf1_drv_init(uart_cfg_t *cfg)
{
    ms72sf1_uart = cfg->instance;
    uart_init(ms72sf1_uart, cfg->baudrate);

    // GPIO_FUNC_UART = F2 = 2, see hardware/gpio.h
    // GPIO_FUNC_UART_AUX = F11
    gpio_set_function(cfg->tx_pin, cfg->func);
    gpio_set_function(cfg->rx_pin, cfg->func);

    uart_set_format(ms72sf1_uart, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(ms72sf1_uart, true);

    uart_set_hw_flow(ms72sf1_uart, false, false);

#ifdef _MS72SF1_DEBUG_
    printf("[MS72SF1] UART init @ %d baud\r\n", cfg->baudrate);
    printf("[MS72SF1] TX pin=%d RX pin=%d\r\n",
       cfg->tx_pin, cfg->rx_pin);
#endif
    s_head = s_tail = 0;

    sleep_ms(10); /* let UART settle */
    uart_write_blocking(ms72sf1_uart, multi_target_cmd, sizeof(multi_target_cmd));
    return true;
}

uint16_t temp_global_cnt = 0;

/* ---------- robust resync parser ---------- */
static void try_parse_frames(void)
{

    /**
     * Work only when we have at least one full frame.
     */
    while (rb_count() >= MS72SF1_FRAME_BYTES)
    {
        uint16_t count = rb_count();
        int found_at = -1;

        for (uint16_t off = 0; off < (count-3); off++)
        {
            if (rb_peek(off + 0) == MS72SF1_HDR0 &&
                rb_peek(off + 1) == MS72SF1_HDR1 &&
                rb_peek(off + 2) == MS72SF1_HDR2 &&
                rb_peek(off + 3) == MS72SF1_HDR3)
            {
                found_at = (int)off;
                break;
            }
        }

        // if (debug_flag_ms72sf1_raw) {
        //     printf("\r\n[MS72SF1] - Found c=%u, f_at=%d\r\n", count, found_at);
        // }

        if (found_at < 0)
        {
            /* No header found; keep last 3 bytes in case header splits across polls */
            if (count > 3) {
                rb_drop((uint16_t)(count - 3));
            }
            return;
        }

        /* Drop noise before header */
        if (found_at > 0)
            rb_drop((uint16_t)found_at);

        /* Now tail check for the candidate frame */
        if (rb_count() < MS72SF1_FRAME_BYTES)
            return;

        uint8_t tail0 = rb_peek((uint16_t)(MS72SF1_FRAME_BYTES - 2));
        uint8_t tail1 = rb_peek((uint16_t)(MS72SF1_FRAME_BYTES - 1));
        if (tail0 != MS72SF1_TAIL0 || tail1 != MS72SF1_TAIL1)
        {
            /* False header hit; drop one byte and rescan */
            rb_drop(1);
            continue;
        }

        /* Valid frame. Consume header+payload+tail and publish. */
        uint8_t raw[MS72SF1_FRAME_BYTES];
        rb_read_bytes(raw, MS72SF1_FRAME_BYTES);

        /* raw[0..3] is header; payload starts at raw[4] */
        const uint8_t *payload = &raw[4];

        // new proposal
        ms72sf1_target_raw_t *target = s_last_data.target;

        for (int i = 0; i < MS72SF1_OBJECT_SLOTS; i++) {
            uint8_t *buf_rx = (uint8_t *)payload + i * sizeof(ms72sf1_object_raw_t);

            if (buf_rx[0] == 0 && buf_rx[2] == 0 && buf_rx[6] == 0) {
                target[i].x_raw = 0;
                target[i].y_raw = 0;
                target[i].v_raw = 0;
                target[i].dist_mm = 0;
                target[i].active = false;
            } else {
                int16_t x = (int16_t)((buf_rx[0] | (buf_rx[1] << 8)));
                int16_t y = (int16_t)((buf_rx[2] | (buf_rx[3] << 8)) - 0x8000);
                int16_t v = (int16_t)((buf_rx[4] | (buf_rx[5] << 8)));
                
                target[i].x_raw = (x < 0 ? x ^ 0x7FFF : x);
                target[i].y_raw = (y < 0 ? y ^ 0x7FFF : y);
                target[i].v_raw = (v < 0 ? v ^ 0x7FFF : v);
                target[i].dist_mm = (uint16_t)(buf_rx[6] | (buf_rx[7] << 8));
                target[i].active = true;
            }
        }


        // s_last_frame.report = report;
        // s_last_frame.rx_time_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
        s_last_data.rx_time_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
        s_last_data.ready = true;
        #ifdef _MS72SF1_DEBUG_
        rx_stream_started = false; /* for debug prints */
        #endif

        #ifdef _MS72SF1_DEBUG_
        if (debug_flag_ms72sf1_raw) {
            printf("[MS72SF1] Frame received.\r\n");
        }
        #endif // _MS72SF1_DEBUG_
        /* Stop after publishing one frame (keeps latency low and avoids starving main loop) */
        return;
    }
}

/**
 * Polling-based driver:
 * UART HW RX FIFO
 *     ↓
 * ms72sf1_drv_poll()
 *     ↓
 * rb_push()
 *     ↓
 * try_parse_frames()
 *     ↓
 * s_last_data
 */
void ms72sf1_drv_poll(void)
{
    while (uart_is_readable(ms72sf1_uart))
        rb_push((uint8_t)uart_getc(ms72sf1_uart));

    try_parse_frames();
}

bool ms72sf1_drv_get_ready(ms72sf1_data_t *out)
{
    if (!out || !s_last_data.ready)
        return false;

    *out = s_last_data;
    s_last_data.ready = false;
    return true;
}

void ms72sf1_drv_set_debug(bool enable)
{
    debug_flag_ms72sf1_print = enable;
}
// void ms72sf1_drv_set_raw_debug(bool enable)
// {
//     debug_flag_ms72sf1_raw = enable;
// }

// void ms72sf1_drv_set_flag_debug(bool enable)
// {
//     debug_flag_ms72sf1_flag = enable;
// }

void ms72sf1_drv_send_multi_target_cmd(void)
{
    uart_write_blocking(ms72sf1_uart, multi_target_cmd, sizeof(multi_target_cmd));
}

void ms72sf1_drv_send_single_target_cmd(void)
{
    uart_write_blocking(ms72sf1_uart, single_target_cmd, sizeof(single_target_cmd));
}


