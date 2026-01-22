/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

#include "rd03d_drv.h"
#include "rd03d_protocol.h"

#define RD03D_UART     uart0
#define RX_BUF_SIZE    256

static uint8_t  rx_buf[RX_BUF_SIZE];
static uint16_t head, tail;

static rd03d_frame_t frame_q;
static bool frame_ready;

/* ---------- Ring buffer ---------- */
static inline uint16_t rb_count(void)
{
    return (head - tail) & (RX_BUF_SIZE - 1);
}

static inline void rb_push(uint8_t b)
{
    rx_buf[head] = b;
    head = (head + 1) & (RX_BUF_SIZE - 1);
}

static inline bool rb_pop(uint8_t *b)
{
    if (head == tail)
        return false;
    *b = rx_buf[tail];
    tail = (tail + 1) & (RX_BUF_SIZE - 1);
    return true;
}

/* ---------- UART ---------- */
bool rd03d_drv_init(void)
{
    uart_init(RD03D_UART, 256000);
    gpio_set_function(2, GPIO_FUNC_UART);
    gpio_set_function(3, GPIO_FUNC_UART);
    uart_set_format(RD03D_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(RD03D_UART, true);

    head = tail = 0;
    frame_ready = false;
    return true;
}

/* ---------- Parser ---------- */
static void parse_frames(void)
{
    while (rb_count() >= 5)
    {
        uint8_t b;
        rb_pop(&b);
        if (b != RD03D_FRAME_HEADER_0)
            continue;

        rb_pop(&b);
        if (b != RD03D_FRAME_HEADER_1)
            continue;

        rb_pop(&frame_q.msg_id);
        rb_pop(&frame_q.len);

        if (frame_q.len > sizeof(frame_q.payload))
            continue;

        if (rb_count() < frame_q.len + 1)
            return;

        for (uint8_t i = 0; i < frame_q.len; i++)
            rb_pop(&frame_q.payload[i]);

        uint8_t checksum;
        rb_pop(&checksum);

        /* TODO: checksum validation */

        frame_ready = true;
        return;
    }
}

void rd03d_drv_poll(void)
{
    while (uart_is_readable(RD03D_UART))
        rb_push(uart_getc(RD03D_UART));

    parse_frames();
}

bool rd03d_drv_get_frame(rd03d_frame_t *out)
{
    if (!frame_ready)
        return false;

    *out = frame_q;
    frame_ready = false;
    return true;
}
