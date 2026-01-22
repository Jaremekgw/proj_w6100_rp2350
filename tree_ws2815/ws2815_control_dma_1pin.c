/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "pico/stdio.h"
#include "pico/sem.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "config_tree.h"
#include "ws2815_control_dma.h"
#include "ws2815.pio.h"
#include "led_pattern.h"


// External globals (must be provided somewhere else)

// global defines for WS2815 control over single GPIO pin with direct access PIO
#define NUM_LEDS 3
// #define WS2815_MAX_LEDS   NUM_LEDS

PIO ws2815_pio;     // = pio0; using PIO instance 0
uint ws2815_sm;     // = 0; state machine index
// LED framebuffer
uint32_t ws2815_buf[NUM_LEDS];      // LED buffer
bool ws2815_is_rgbw = false;
volatile bool ws2815_refresh_required = false;
// DMA state
int ws2815_dma_ch = -1;
volatile bool ws2815_dma_done = true;

// Latch timing
absolute_time_t ws2815_latch_deadline;

// version 2
// ---------------- DMA control code ----------------

/**
 * DMA interrupt handler
 */
void __isr dma_handler_ws2815(void)
{
    dma_hw->ints0 = 1u << ws2815_dma_ch;   // clear interrupt
    ws2815_dma_done = true;

    // schedule latch (WS2815 requires 280–300 µs LOW)
    ws2815_latch_deadline = make_timeout_time_us(350);
}

/**
 * Install DMA IRQ handler for WS2815
 */
void ws2815_dma_irq_setup(void)
{
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler_ws2815);
    irq_set_enabled(DMA_IRQ_0, true);
}

/**
 * DMA configuration for WS2815
 * This config streams 32-bit LED words to the PIO TX FIFO.
 * 
 */
void ws2815_dma_init(void)
{
    ws2815_dma_ch = dma_claim_unused_channel(true);

    dma_channel_config c = dma_channel_get_default_config(ws2815_dma_ch);
    channel_config_set_dreq(&c, pio_get_dreq(ws2815_pio, ws2815_sm, true));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);

    dma_channel_configure(
        ws2815_dma_ch,
        &c,
        &ws2815_pio->txf[ws2815_sm],   // write address (PIO FIFO)
        NULL,                          // read address (set per frame)
        0,                             // transfer count (set later)
        false                          // don't start yet
    );

    // Enable IRQ on end of transfer
    dma_hw->inte0 |= 1u << ws2815_dma_ch;
}

/**
 * Start DMA transfer for one LED frame
 * 
 * @param led_count Number of LEDs to transfer
 */
void ws2815_start_dma_write(uint led_count)
{
    if (!ws2815_dma_done) return;   // still busy

    ws2815_dma_done = false;

    dma_channel_set_read_addr(ws2815_dma_ch, ws2815_buf, false);
    dma_channel_set_trans_count(ws2815_dma_ch, led_count, true);
}


const float ws_freq = 800000.0f;   // WS2815 = 800 kHz nominal, shuld work 1000000.0f too
// #define WS2815_PIN_BASE     2

void ws2815_init(void) {
    uint offset;
    // This function initializes the PIO and DMA for WS2815 control.
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2815_program, &ws2815_pio, &ws2815_sm, &offset, \
                                                                    WS2815_PIN_BASE, 1, true);

    hard_assert(success);

    ws2815_program_init(ws2815_pio, ws2815_sm, offset, WS2815_PIN_BASE, ws_freq, false);
    // parallel: ws2815_program_init(pio, sm, offset, WS2815_PIN_BASE, NUM_STRIPS, 800000);

    // --- old ---
    // sem_init(&reset_delay_complete_sem, 1, 1); // initially posted so we don't block first time
    // dma_init(pio, sm);

    // This is a stub function for the purpose of this example.
    // The actual implementation would involve initializing PIO and DMA
    // for WS2815 control.

    // --- new ---
    ws2815_dma_irq_setup();
    ws2815_dma_init();

    ws2815_dma_done = true;
    ws2815_latch_deadline = make_timeout_time_us(300);

    printf("WS2815 initialized.\n");
}


uint8_t get_pattern_index(void) {
    return 0; // pattern_index;
}


void simple_fill_buffer(void);

/**
 * Main loop for createing different patterns automatically
 * Function called periodically every 20 ms from main loop
 */
void ws2815_pattern_loop(uint32_t period_ms) {
    static int counter = 100;

    if (++counter >= 1) { // for 20 every 400 ms
        counter = 0;
        printf("Simple fill buffer\n");
        simple_fill_buffer();
    }
}


/**
 * DMA controlled WS2815 output loop
 */
void ws2815_loop(uint32_t period_ms)
{
    if (!ws2815_dma_done)
        return;     // still transmitting frame

    if (!time_reached(ws2815_latch_deadline))
        return;     // waiting for latch period

    if (!ws2815_refresh_required)
        return;     // nothing to send

    ws2815_refresh_required = false;

    ws2815_start_dma_write(NUM_LEDS);
}


void simple_fill_buffer(void) {
    static uint8_t hue = 0;
    for (uint i = 0; i < NUM_LEDS; i++) {
        // Simple color wheel based on hue
        uint8_t r, g, b;
        uint8_t region = hue / 43;
        uint8_t remainder = (hue - (region * 43)) * 6;

        switch (region) {
            case 0:
                r = 255; g = remainder; b = 0;
                break;
            case 1:
                r = 255 - remainder; g = 255; b = 0;
                break;
            case 2:
                r = 0; g = 255; b = remainder;
                break;
            case 3:
                r = 0; g = 255 - remainder; b = 255;
                break;
            case 4:
                r = remainder; g = 0; b = 255;
                break;
            default:
                r = 255; g = 0; b = 255 - remainder;
                break;
        }

        if (ws2815_is_rgbw) {
            // For RGBW, set white channel to zero
            ws2815_buf[i] = (r << 24) | (g << 16) | (b << 8) | 0;
        } else {
            ws2815_buf[i] = (r << 16) | (g << 8) | b;
        }
    }
    hue += 1; // Increment hue for next frame
    ws2815_refresh_required = true;
}
