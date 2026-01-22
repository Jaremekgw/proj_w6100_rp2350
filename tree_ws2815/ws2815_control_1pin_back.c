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
// #include "generated/ws2815_parallel.pio.h"
#include "ws2815.pio.h"
#include "led_pattern.h"


// External globals (must be provided somewhere else)

#define NUM_LEDS 3
PIO ws2815_pio;
uint ws2815_sm;
uint32_t ws2815_buf[NUM_LEDS];      // LED buffer
bool ws2815_is_rgbw = false;
bool ws2815_refresh_required = false;


// Internal state
static bool frame_in_progress = false;
static uint32_t next_led_index = 0;
static absolute_time_t latch_deadline;


// --------------  old structures and defines from ws2812_parallel.c  ----------------
// #define VALUE_PLANE_COUNT (8)   // (8 + FRAC_BITS)
// typedef struct {
//     // stored LSB first, only NUM_STRIPS bits used in each plane
//     uint32_t planes[VALUE_PLANE_COUNT];
// } value_bits_t;

// // One color bit plane (uint32_t) consists of bits for all strips at given bit position
// // For NUM_STRIPS strips, we need NUM_PIXELS * NUM_CHANNELS such planes
// static value_bits_t colors[NUM_PIXELS * NUM_CHANNELS];


// ------------------- Framebuffer for display 2D (NUM_PIXELS * NUM_STRIPS) patterns ------------------
// // typedef uint8_t pixel_t[NUM_CHANNELS];
// // typedef pixel_t strip_buffer_t[NUM_PIXELS];
typedef uint8_t strip_buffer_t[NUM_PIXELS][NUM_CHANNELS];
strip_buffer_t framebuf[NUM_STRIPS];   // fb[strip][pixel][channel], channel = R,G,B (should be transfered to G,R,B)

// #define DDP_COM_TIMEOUT_MS 2000         // ms delay to switch to pattern mode after last DDP packet
uint32_t ddp_update_timeout = 0;

bool ddp_update_framebuf = false;
bool patern_update_framebuf = false;

// ---------------- DMA control code ----------------
// bit plane content dma channel
#define DMA_CHANNEL 0
// chain channel for configuring main dma channel to output from disjoint 8 word fragments of memory
#define DMA_CB_CHANNEL 1

#define DMA_CHANNEL_MASK (1u << DMA_CHANNEL)
#define DMA_CB_CHANNEL_MASK (1u << DMA_CB_CHANNEL)
#define DMA_CHANNELS_MASK (DMA_CHANNEL_MASK | DMA_CB_CHANNEL_MASK)

// // start of each value fragment (+1 for NULL terminator)
// static uintptr_t fragment_start[NUM_PIXELS * NUM_CHANNELS + 1];

// posted when it is safe to output a new set of values
static struct semaphore reset_delay_complete_sem;
// alarm handle for handling delay
alarm_id_t reset_delay_alarm_id;

int64_t reset_delay_complete(__unused alarm_id_t id, __unused void *user_data) {
    reset_delay_alarm_id = 0;
    sem_release(&reset_delay_complete_sem);
    // no repeat
    return 0;
}

void __isr dma_complete_handler() {
    if (dma_hw->ints0 & DMA_CHANNEL_MASK) {
        // clear IRQ
        dma_hw->ints0 = DMA_CHANNEL_MASK;
        // when the dma is complete we start the reset delay timer
        if (reset_delay_alarm_id) cancel_alarm(reset_delay_alarm_id);
        reset_delay_alarm_id = add_alarm_in_us(400, reset_delay_complete, NULL, true);  // for ws2815 reset time is 280us
    }
}

void dma_init(PIO pio, uint sm) {
    dma_claim_mask(DMA_CHANNELS_MASK);

    // main DMA channel outputs 8 word fragments, and then chains back to the chain channel
    dma_channel_config channel_config = dma_channel_get_default_config(DMA_CHANNEL);
    channel_config_set_dreq(&channel_config, pio_get_dreq(pio, sm, true));
    channel_config_set_chain_to(&channel_config, DMA_CB_CHANNEL);
    channel_config_set_irq_quiet(&channel_config, true);
    dma_channel_configure(DMA_CHANNEL,
                          &channel_config,
                          &pio->txf[sm],
                          NULL, // set by chain
                          8, // 8 words for 8 bit planes
                          false);

    // chain channel sends single word pointer to start of fragment each time
    dma_channel_config chain_config = dma_channel_get_default_config(DMA_CB_CHANNEL);
    dma_channel_configure(DMA_CB_CHANNEL,
                          &chain_config,
                          &dma_channel_hw_addr(
                                  DMA_CHANNEL)->al3_read_addr_trig,  // ch DMA config (target "ring" buffer size 4) - this is (read_addr trigger)
                          NULL, // set later
                          1,
                          false);

    irq_set_exclusive_handler(DMA_IRQ_0, dma_complete_handler);
    dma_channel_set_irq0_enabled(DMA_CHANNEL, true);
    irq_set_enabled(DMA_IRQ_0, true);
}

// void output_strips_dma(value_bits_t *bits, uint value_length) {
//     for (uint i = 0; i < value_length; i++) {
//         fragment_start[i] = (uintptr_t) bits[i].planes; // MSB first
//     }
//     fragment_start[value_length] = 0;
//     dma_channel_hw_addr(DMA_CB_CHANNEL)->al3_read_addr_trig = (uintptr_t) fragment_start;
// }

// ========================== Manage pixel colors  ==========================



// // typedef void (*pattern)(uint len, uint t);
// const struct {
//     pattern pat;
//     const char *name;
// } pattern_table[] = {
//         {pattern_snakes,  "Snakes!"},       // pattern 0
//         {pattern_random,  "Random data"},   // pattern 1
//         {pattern_sparkle, "Sparkles"},      // pattern 2
//         {pattern_drop1,   "Drop_1"},     // pattern 3
//         {pattern_solid,  "Solid!"},     // pattern 4
//         {pattern_jaremek, "Jaremek"},   // pattern 5
// };



#define PAT_AUTO    200
#define PAT_ZERO    201
#define PAT_IDLE    202
uint8_t pattern_index = 0x00, pattern_last_index;
const float ws_freq = 800000.0f;   // WS2815 = 800 kHz nominal, shuld work 1000000.0f too
// #define WS2815_PIN_BASE     2

void ws2815_init(void) {
    // change to ws2815_pio, ws2815_sm
    //PIO pio;  use: global ws2815_pio
    //uint sm;  use: global ws2815_sm
    // PIO ws2815_pio;
    // uint ws2815_sm;

    uint offset;
    // This function initializes the PIO and DMA for WS2815 control.
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2815_program, &ws2815_pio, &ws2815_sm, &offset, \
                                                                    WS2815_PIN_BASE, 1, true);

    hard_assert(success);

    ws2815_program_init(ws2815_pio, ws2815_sm, offset, WS2815_PIN_BASE, ws_freq, false);
    // parallel: ws2815_program_init(pio, sm, offset, WS2815_PIN_BASE, NUM_STRIPS, 800000);

    // sem_init(&reset_delay_complete_sem, 1, 1); // initially posted so we don't block first time
    // dma_init(pio, sm);

    // This is a stub function for the purpose of this example.
    // The actual implementation would involve initializing PIO and DMA
    // for WS2815 control.
    printf("WS2815 initialized.\n");

    pattern_index = PAT_ZERO;
    // patern_update_framebuf = true;
}

/**
 * This function is for parallel WS2815 initialization
 */
/* 
void ws2815_init(void) {
    PIO pio;
    uint sm;
    uint offset;
    // This function initializes the PIO and DMA for WS2815 control.
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2815_parallel_program, \
                                                                    &pio, &sm, &offset, \
                                                                    WS2815_PIN_BASE, \
                                                                    NUM_STRIPS, \
                                                                    true);
    hard_assert(success);

    ws2815_parallel_program_init(pio, sm, offset, WS2815_PIN_BASE, NUM_STRIPS, 800000);

    sem_init(&reset_delay_complete_sem, 1, 1); // initially posted so we don't block first time
    dma_init(pio, sm);

    // This is a stub function for the purpose of this example.
    // The actual implementation would involve initializing PIO and DMA
    // for WS2815 control.
    printf("WS2815 initialized.\n");

    pattern_index = PAT_ZERO;
    // patern_update_framebuf = true;
}
 */
// uint8_t set_pattern_index(uint8_t index) {
//     if (index > count_of(pattern_table))
//         pattern_index = PAT_AUTO;
//     else if (index == 0)
//         pattern_index = PAT_ZERO;
//     else
//         pattern_index = index;
//     return pattern_index;
// }

uint8_t get_pattern_index(void) {
    // if (pattern_index < count_of(pattern_table))
    //     return (pattern_index + 1);
    return pattern_index;
}

void simple_fill_buffer(void);
/**
 * Main loop for createing different patterns automatically
 * Function called periodically every 20 ms from main loop
 */
void ws2815_pattern_loop(uint32_t period_ms) {
                        // static int t = 0;
    // static int loop_count = 0;
    // static int pat=0; // dir=0;
    static int counter = 100;

    if (ddp_update_timeout) {
        ddp_update_timeout =
            (ddp_update_timeout >= period_ms)? (ddp_update_timeout - period_ms) : 0;
        if (ddp_update_timeout == 0)
            printf("DDP communication timeout\n");
        return;
    }

    if (++counter >= 1) { // every 400 ms
        counter = 0;
        printf("Simple fill buffer\n");
        simple_fill_buffer();
    }

    // if (pattern_index <= count_of(pattern_table)) {
    //     pat = pattern_index - 1;
    //     if (pattern_last_index != pattern_index) {
    //         pattern_last_index = pattern_index;
    //         loop_count = 0;
    //         // dir = 1;
    //         // printf("Pattern %d=%s dir:%s\n", pat + 1, pattern_table[pat].name, dir == 1 ? "(forward)" : dir ? "(backward)" : "(still)");
    //          printf("Pattern %d=%s\n", pat + 1, pattern_table[pat].name);
    //     }   
    // } 
    // else if (pattern_index == PAT_AUTO) {
    //     // select random pattern
    //     if (++loop_count > 800) {   // 200 * 20ms = 4s
    //         loop_count = 0;
    //         // change pattern
    //         pat = rand() % count_of(pattern_table);
    //         pattern_last_index = pat + 1;
    //         // dir = (rand() >> 30) & 1 ? 1 : -1;
    //         // if (rand() & 1) dir = 0;
    //         // printf("Pattern %d=%s dir:%s\n", pat + 1, pattern_table[pat].name, dir == 1 ? "(forward)" : dir ? "(backward)" : "(still)");
    //         printf("Pattern %d=%s\n", pat + 1, pattern_table[pat].name);
    //     }
    // } 
    // else if (pattern_index != PAT_IDLE) {
    //     pattern_index = PAT_IDLE;
    //     size_t framesize = sizeof(framebuf);

    //     memset(framebuf, 0, sizeof(framebuf));
    //     patern_update_framebuf = true;
    //     printf("Clear buffer, size: %d\n", framesize);
    //     return;
    // }
    // else {  // pattern_index == PAT_IDLE
    //     return;
    // }

    // pattern_table[pat].pat(&framebuf[0][0][0], NUM_STRIPS, NUM_PIXELS);
    // patern_update_framebuf = true;
}

/**
 * Main loop function called periodically (2 ms) to manage WS2815 output
 */
// void ws2815_loop(uint32_t period_ms) {

//     if (!ddp_update_framebuf && !patern_update_framebuf)
//         return;
//     ddp_update_framebuf = false;
//     patern_update_framebuf = false;

//     transform_framebuf(framebuf, count_of(framebuf), colors, NUM_PIXELS * NUM_CHANNELS);

//     sem_acquire_blocking(&reset_delay_complete_sem);
//     output_strips_dma(colors, NUM_PIXELS * NUM_CHANNELS);
// }

// void ws2815_show(uint8_t *fb) {
//     strip_buffer_t *pixel;
//     pixel = &framebuf[0];

//     memcpy(framebuf, fb, sizeof(framebuf)); // strips[16]*pixels[57]*channels[3] = 2736 bytes
//     ddp_update_framebuf = true;
//     ddp_update_timeout = DDP_COM_TIMEOUT_MS;
//     // printf("Framebuf recieved. Value pixel[0]=%#04x,%#04x,%#04x pixel[1]=%#04x,%#04x,%#04x\n", sb[0][0], sb[0][1], sb[0][2], sb[1][0], sb[1][1], sb[1][2]);
//     printf("Framebuf recieved. Value pixel[0]=%#04x,%#04x,%#04x pixel[1]=%#04x,%#04x,%#04x\n", pixel[0][0], pixel[0][1], pixel[0][2], pixel[1][0], pixel[1][1], pixel[1][2]);
// }

/**
 * Direct control of WS2815 output from main loop
 * 
 */
void ws2815_loop(uint32_t period_ms) {
    // If still transmitting a frame
    if (frame_in_progress) {
        // Try to keep feeding FIFO
        while (!pio_sm_is_tx_fifo_full(ws2815_pio, ws2815_sm)) {

            if (next_led_index >= NUM_LEDS) {
                // Full frame written – schedule latch window (≥ 300 µs)
                frame_in_progress = false;
                latch_deadline = make_timeout_time_us(350);
                break;
            }

            // For RGB: 24 bits. For RGBW: 32 bits.
            uint32_t val = ws2815_buf[next_led_index++];
            // printf("Send RGB val: 0x%08x\n", val);
            pio_sm_put(ws2815_pio, ws2815_sm, val);         // <-- send next LED data
            // pio_sm_put_blocking(ws2815_pio, ws2815_sm, val);    // works
        }
        return;
    }

    // If frame ended, ensure latch period is respected
    if (!time_reached(latch_deadline)) {
        return;
    }

    // Ready for the next frame; if no new data, do nothing.
    // Your application can set a flag to request refresh.
    extern bool ws2815_refresh_required;
    if (!ws2815_refresh_required) {
        return;
    }

    // Start new frame
    ws2815_refresh_required = false;
    frame_in_progress = true;
    next_led_index = 0;
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
