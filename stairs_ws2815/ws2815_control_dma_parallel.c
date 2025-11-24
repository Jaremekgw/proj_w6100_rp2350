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

#include "config.h"
#include "ws2815_control_dma_parallel.h"
// #include "generated/ws2815_parallel.pio.h"
#include "ws2815.pio.h"
#include "led_pattern.h"


// --------------  old structures and defines from ws2812_parallel.c  ----------------
#define VALUE_PLANE_COUNT (8)   // (8 + FRAC_BITS)
typedef struct {
    // stored LSB first, only NUM_STRIPS bits used in each plane
    uint32_t planes[VALUE_PLANE_COUNT];
} value_bits_t;

// One color bit plane (uint32_t) consists of bits for all strips at given bit position
// For NUM_STRIPS strips, we need NUM_PIXELS * NUM_CHANNELS such planes
static value_bits_t colors[NUM_PIXELS * NUM_CHANNELS];


// ------------------- Framebuffer for display 2D (NUM_PIXELS * NUM_STRIPS) patterns ------------------
// typedef uint8_t pixel_t[NUM_CHANNELS];
// typedef pixel_t strip_buffer_t[NUM_PIXELS];
typedef uint8_t strip_buffer_t[NUM_PIXELS][NUM_CHANNELS];
strip_buffer_t framebuf[NUM_STRIPS];   // fb[strip][pixel][channel], channel = R,G,B (should be transfered to G,R,B)

#define DDP_COM_TIMEOUT_MS 2000         // ms delay to switch to pattern mode after last DDP packet
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

// start of each value fragment (+1 for NULL terminator)
static uintptr_t fragment_start[NUM_PIXELS * NUM_CHANNELS + 1];

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

void output_strips_dma(value_bits_t *bits, uint value_length) {
    for (uint i = 0; i < value_length; i++) {
        fragment_start[i] = (uintptr_t) bits[i].planes; // MSB first
    }
    fragment_start[value_length] = 0;
    dma_channel_hw_addr(DMA_CB_CHANNEL)->al3_read_addr_trig = (uintptr_t) fragment_start;
}

// ========================== Manage pixel colors  ==========================



// typedef void (*pattern)(uint len, uint t);
const struct {
    pattern pat;
    const char *name;
} pattern_table[] = {
        {pattern_snakes,  "Snakes!"},       // pattern 0
        {pattern_random,  "Random data"},   // pattern 1
        {pattern_sparkle, "Sparkles"},      // pattern 2
        {pattern_drop1,   "Drop_1"},     // pattern 3
        {pattern_solid,  "Solid!"},     // pattern 4
        {pattern_jaremek, "Jaremek"},   // pattern 5
};

/**
 * takes 8 bit color values and store in bit planes
 * example: transform_strips(strips, count_of(strips), colors, NUM_PIXELS * 4);
 * 
 * @param strips array of strip_t pointers
 * @param num_strips number of strips
 * @param values output array of value_bits_t               // static value_bits_t colors[NUM_PIXELS * 4];
 * @param value_length length of values array (should be at least max strip data_len)   // NUM_PIXELS * 4 = 64 * 4 = 256
 */

/* void transform_strips(strip_t **strips, uint num_strips, value_bits_t *values, uint value_length) {
    for (uint v = 0; v < value_length; v++) {       // value_length = num_pixels * 4 = 256
        memset(&values[v], 0, sizeof(values[v]));   // clear current plane (colors[v])
        for (uint i = 0; i < num_strips; i++) {     // num_strips = 5
            if (v < strips[i]->data_len) {
                // todo clamp?
                uint32_t value = strips[i]->data[v];    // 8 bit color value R/G/B/W with FRAC_BITS above 8 bits
                for (int j = 0; j < VALUE_PLANE_COUNT; j++, value >>= 1u) {    // for every bit in value (8 + FRAC_BITS) and if remaining set bits
                    // removed '&& value' to see all planes
                    if (value & 1u)
                        // values[v].planes[j] |= (1u << i);
                        values[v].planes[VALUE_PLANE_COUNT - 1 - j] |= 1u << i;
                }
            }
        }
    }
}

void transform_framebuf(strip_buffer_t *strips, uint num_strips, value_bits_t *values, uint value_length) {
    for (uint v = 0; v < value_length; v++) {       // value_length = num_pixels * 3 = 57 * 3 = 171; every color is 8bit value
        memset(&values[v], 0, sizeof(values[v]));   // clear current plane (colors[v])
        for (uint i = 0; i < num_strips; i++) {     // num_strips = number of bits sent in parallel

            if (v < sizeof(strips[0])) {
                // todo clamp?
                uint8_t value = strips[i][v / NUM_CHANNELS][v % NUM_CHANNELS];
                for (int j = 0; j < VALUE_PLANE_COUNT; j++, value >>= 1u) {    // for every bit in value (8 + FRAC_BITS) and if remaining set bits
                    // removed '&& value' to see all planes
                    if (value & 1u)
                        // values[v].planes[j] |= (1u << i);
                        values[v].planes[VALUE_PLANE_COUNT - 1 - j] |= 1u << i;
                }
            }
        }
    }
}
 */


void transform_framebuf(strip_buffer_t *strips, uint num_strips, value_bits_t *values, uint pixels) {
    uint p, r, g, b;

    if (pixels > NUM_PIXELS)
        pixels = NUM_PIXELS;
    if (num_strips > NUM_STRIPS)
        num_strips = NUM_STRIPS;
    for (p = 0; p < pixels; p++) {       // every pixel has 3 color (8 plane_bits in buffer 'colors[]') values in order G,R,B
        g = p * NUM_CHANNELS;
        r = g + 1;
        b = g + 2;
        memset(&values[g], 0, sizeof(values[0]) * NUM_CHANNELS);   // clear current 3 planes (in buffer colors[]) representing one pixel
        for (uint i = 0; i < num_strips; i++) {     // num_strips = number of bits sent in parallel

            if (p < count_of(strips[0])) {    // amount of pixels in colors[] can't be more than in framebuffer[]
                uint8_t value_red = strips[i][p][0];
                uint8_t value_green = strips[i][p][1];
                uint8_t value_blue = strips[i][p][2];
                for (int j = 0; j < VALUE_PLANE_COUNT; j++) {    // for every bit in value (8 + FRAC_BITS) and if remaining set bits
                    // removed '&& value' to see all planes
                    // bit0 from strip is put as a bit7 in plane_bits[0] as MSB first is passing to GPIO
                    if (value_red & 1u)
                        // values[v].planes[j] |= (1u << i);
                        values[r].planes[VALUE_PLANE_COUNT - 1 - j] |= 1u << i;
                    if (value_green & 1u)
                        values[g].planes[VALUE_PLANE_COUNT - 1 - j] |= 1u << i;
                    if (value_blue & 1u)
                        values[b].planes[VALUE_PLANE_COUNT - 1 - j] |= 1u << i;
                    value_red >>= 1u;
                    value_green >>= 1u;
                    value_blue >>= 1u;
                    if (!(value_red || value_green || value_blue))
                        break;
                }
            }
        }
    }
}

#define PAT_AUTO    200
#define PAT_ZERO    201
#define PAT_IDLE    202
uint8_t pattern_index = 0x00, pattern_last_index;

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
    patern_update_framebuf = true;
}

uint8_t set_pattern_index(uint8_t index) {
    if (index > count_of(pattern_table))
        pattern_index = PAT_AUTO;
    else if (index == 0)
        pattern_index = PAT_ZERO;
    else
        pattern_index = index;
    return pattern_index;
}

uint8_t get_pattern_index(void) {
    // if (pattern_index < count_of(pattern_table))
    //     return (pattern_index + 1);
    return pattern_index;
}

/**
 * Main loop for createing different patterns automatically
 * Function called periodically every 20 ms from main loop
 */
void ws2815_pattern_loop(uint32_t period_ms) {
    // static int t = 0;
    static int loop_count = 0;
    static int pat=0; // dir=0;

    if (ddp_update_timeout) {
        ddp_update_timeout =
            (ddp_update_timeout >= period_ms)? (ddp_update_timeout - period_ms) : 0;
        if (ddp_update_timeout == 0)
            printf("DDP communication timeout\n");
        return;
    }

    if (pattern_index <= count_of(pattern_table)) {
        pat = pattern_index - 1;
        if (pattern_last_index != pattern_index) {
            pattern_last_index = pattern_index;
            loop_count = 0;
            // dir = 1;
            // printf("Pattern %d=%s dir:%s\n", pat + 1, pattern_table[pat].name, dir == 1 ? "(forward)" : dir ? "(backward)" : "(still)");
             printf("Pattern %d=%s\n", pat + 1, pattern_table[pat].name);
        }   
    } 
    else if (pattern_index == PAT_AUTO) {
        // select random pattern
        if (++loop_count > 800) {   // 200 * 20ms = 4s
            loop_count = 0;
            // change pattern
            pat = rand() % count_of(pattern_table);
            pattern_last_index = pat + 1;
            // dir = (rand() >> 30) & 1 ? 1 : -1;
            // if (rand() & 1) dir = 0;
            // printf("Pattern %d=%s dir:%s\n", pat + 1, pattern_table[pat].name, dir == 1 ? "(forward)" : dir ? "(backward)" : "(still)");
            printf("Pattern %d=%s\n", pat + 1, pattern_table[pat].name);
        }
    } 
    else if (pattern_index != PAT_IDLE) {
        pattern_index = PAT_IDLE;
        size_t framesize = sizeof(framebuf);

        memset(framebuf, 0, sizeof(framebuf));
        patern_update_framebuf = true;
        printf("Clear buffer, size: %d\n", framesize);
        return;
    }
    else {  // pattern_index == PAT_IDLE
        return;
    }

    pattern_table[pat].pat(&framebuf[0][0][0], NUM_STRIPS, NUM_PIXELS);
    patern_update_framebuf = true;
}

/**
 * Main loop function called periodically (2 ms) to manage WS2815 output
 */
void ws2815_loop(uint32_t period_ms) {

    //printf("strip0.len=%d strip0.0=%#04x strip0.1=%#04x strip0.2=%#04x strip0.3=%#04x strip0.4=%#04x\n", strip0.data_len, strip0.data[0], strip0.data[1], strip0.data[2], strip0.data[3], strip0.data[4]);

    if (!ddp_update_framebuf && !patern_update_framebuf)
        return;
    ddp_update_framebuf = false;
    patern_update_framebuf = false;

    // transform_strips(strips, count_of(strips), colors, NUM_PIXELS * NUM_CHANNELS);  // , brightness
    transform_framebuf(framebuf, count_of(framebuf), colors, NUM_PIXELS * NUM_CHANNELS);

    //for(int c=0; c<3; c++) {
    //    printf("Color:%d\n", c);
    //    for(int p=0; p<8; p++) {
    //        printf("\tplane:%02d value=%#08x\n", p, colors[c].planes[p]);
    //    }
    //}
    //printf("colors[0].planes[0]=0x%x colors[0].planes[1]=0x%x colors[0].planes[2]=0x%x\n\n", colors[0].planes[0], colors[0].planes[1], colors[0].planes[2]);

    sem_acquire_blocking(&reset_delay_complete_sem);
            // output_strips_dma(states[current], NUM_PIXELS * NUM_CHANNELS);
    output_strips_dma(colors, NUM_PIXELS * NUM_CHANNELS);
    // output_plains_sm(pio, sm, colors, NUM_PIXELS * NUM_CHANNELS);

}

void ws2815_show(uint8_t *fb) {
    strip_buffer_t *pixel;
    pixel = &framebuf[0];

    memcpy(framebuf, fb, sizeof(framebuf)); // strips[16]*pixels[57]*channels[3] = 2736 bytes
    ddp_update_framebuf = true;
    ddp_update_timeout = DDP_COM_TIMEOUT_MS;
    // printf("Framebuf recieved. Value pixel[0]=%#04x,%#04x,%#04x pixel[1]=%#04x,%#04x,%#04x\n", sb[0][0], sb[0][1], sb[0][2], sb[1][0], sb[1][1], sb[1][2]);
    printf("Framebuf recieved. Value pixel[0]=%#04x,%#04x,%#04x pixel[1]=%#04x,%#04x,%#04x\n", pixel[0][0], pixel[0][1], pixel[0][2], pixel[1][0], pixel[1][1], pixel[1][2]);
}

