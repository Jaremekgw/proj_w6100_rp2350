/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

#include "rd03d_drv.h"

/* Your wiring */
#ifndef RD03D_UART
#define RD03D_UART        uart0
#endif

#ifndef RD03D_UART_TX_PIN
#define RD03D_UART_TX_PIN 2   /* GP2 -> UART0_TX */
#endif

#ifndef RD03D_UART_RX_PIN
#define RD03D_UART_RX_PIN 3   /* GP3 -> UART0_RX */
#endif

#ifndef RD03D_BAUDRATE
#define RD03D_BAUDRATE    256000
#endif

/* RD-03D report format:
 * Header: AA FF 03 00
 * Payload: 3 objects × 8 bytes = 24 bytes
 * Tail: 55 CC
 * Total: 4 + 24 + 2 = 30 bytes
 * See Table 5-1 / 5-2 :contentReference[oaicite:1]{index=1}
 */
#define RD03D_HDR0 0xAA
#define RD03D_HDR1 0xFF
#define RD03D_HDR2 0x03
#define RD03D_HDR3 0x00
#define RD03D_TAIL0 0x55
#define RD03D_TAIL1 0xCC

#define RD03D_PAYLOAD_BYTES  (RD03D_OBJECT_SLOTS * sizeof(rd03d_object_raw_t))
#define RD03D_FRAME_BYTES    (4u + RD03D_PAYLOAD_BYTES + 2u)

#ifndef RD03D_RX_RING_SIZE
/* Must be power-of-two for mask indexing */
#define RD03D_RX_RING_SIZE 256
#endif

#if (RD03D_RX_RING_SIZE & (RD03D_RX_RING_SIZE - 1)) != 0
#error "RD03D_RX_RING_SIZE must be a power-of-two"
#endif

static uint8_t  s_rx[RD03D_RX_RING_SIZE];
static uint16_t s_head, s_tail;

static rd03d_frame_t s_last_frame;
static volatile bool s_frame_ready;

/* ---------- ring buffer helpers ---------- */
static inline uint16_t rb_count(void)
{
    return (uint16_t)((s_head - s_tail) & (RD03D_RX_RING_SIZE - 1));
}

static inline void rb_push(uint8_t b)
{
    s_rx[s_head] = b;
    s_head = (uint16_t)((s_head + 1) & (RD03D_RX_RING_SIZE - 1));
    /* Overrun policy: drop oldest */
    if (s_head == s_tail)
        s_tail = (uint16_t)((s_tail + 1) & (RD03D_RX_RING_SIZE - 1));
}

static inline uint8_t rb_peek(uint16_t idx_from_tail)
{
    uint16_t idx = (uint16_t)((s_tail + idx_from_tail) & (RD03D_RX_RING_SIZE - 1));
    return s_rx[idx];
}

static inline void rb_drop(uint16_t n)
{
    s_tail = (uint16_t)((s_tail + n) & (RD03D_RX_RING_SIZE - 1));
}

static inline void rb_read_bytes(uint8_t *dst, uint16_t n)
{
    for (uint16_t i = 0; i < n; i++)
        dst[i] = rb_peek(i);
    rb_drop(n);
}

/* ---------- UART init ---------- */
bool rd03d_drv_init(void)
{
    uart_init(RD03D_UART, RD03D_BAUDRATE);

    gpio_set_function(RD03D_UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(RD03D_UART_RX_PIN, GPIO_FUNC_UART);

    uart_set_format(RD03D_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(RD03D_UART, true);

    s_head = s_tail = 0;
    s_frame_ready = false;
    return true;
}

/* ---------- robust resync parser ---------- */
static void try_parse_frames(void)
{
    /* Scan for header within current buffered bytes.
     * We only commit when we can also validate the tail.
     */
    while (rb_count() >= RD03D_FRAME_BYTES)
    {
        /* Find the first candidate header position within the ring.
         * Limit scan to (count - frame_bytes + 1) positions so a full frame can exist.
         */
        uint16_t count = rb_count();
        uint16_t max_scan = (uint16_t)(count - RD03D_FRAME_BYTES + 1);

        int found_at = -1;
        for (uint16_t off = 0; off <= max_scan; off++)
        {
            if (rb_peek(off + 0) == RD03D_HDR0 &&
                rb_peek(off + 1) == RD03D_HDR1 &&
                rb_peek(off + 2) == RD03D_HDR2 &&
                rb_peek(off + 3) == RD03D_HDR3)
            {
                found_at = (int)off;
                break;
            }
        }

        if (found_at < 0)
        {
            /* No header found; keep last 3 bytes in case header splits across polls */
            if (count > 3)
                rb_drop((uint16_t)(count - 3));
            return;
        }

        /* Drop noise before header */
        if (found_at > 0)
            rb_drop((uint16_t)found_at);

        /* Now tail check for the candidate frame */
        if (rb_count() < RD03D_FRAME_BYTES)
            return;

        uint8_t tail0 = rb_peek((uint16_t)(RD03D_FRAME_BYTES - 2));
        uint8_t tail1 = rb_peek((uint16_t)(RD03D_FRAME_BYTES - 1));
        if (tail0 != RD03D_TAIL0 || tail1 != RD03D_TAIL1)
        {
            /* False header hit; drop one byte and rescan */
            rb_drop(1);
            continue;
        }

        /* Valid frame. Consume header+payload+tail and publish. */
        uint8_t raw[RD03D_FRAME_BYTES];
        rb_read_bytes(raw, RD03D_FRAME_BYTES);

        /* raw[0..3] is header; payload starts at raw[4] */
        const uint8_t *payload = &raw[4];

        /* Copy into packed structure (byte exact) */
        rd03d_report_raw_t report = {0};
        for (uint32_t i = 0; i < RD03D_PAYLOAD_BYTES; i++)
            ((uint8_t *)&report)[i] = payload[i];

        s_last_frame.report = report;
        s_last_frame.rx_time_ms = (uint32_t)to_ms_since_boot(get_absolute_time());
        s_frame_ready = true;

        /* Stop after publishing one frame (keeps latency low and avoids starving main loop) */
        return;
    }
}

void rd03d_drv_poll(void)
{
    while (uart_is_readable(RD03D_UART))
        rb_push((uint8_t)uart_getc(RD03D_UART));

    try_parse_frames();
}

bool rd03d_drv_get_frame(rd03d_frame_t *out)
{
    if (!out || !s_frame_ready)
        return false;

    *out = s_last_frame;
    s_frame_ready = false;
    return true;
}

