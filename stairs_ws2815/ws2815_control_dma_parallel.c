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


// --------------  old structures and defines from ws2812_parallel.c  ----------------
#define VALUE_PLANE_COUNT (8)   // (8 + FRAC_BITS)
typedef struct {
    // stored LSB first, only NUM_STRIPS bits used in each plane
    uint32_t planes[VALUE_PLANE_COUNT];
} value_bits_t;

static value_bits_t colors[NUM_PIXELS * NUM_CHANNELS];

static uint8_t strip0_data[NUM_PIXELS * NUM_CHANNELS];
static uint8_t strip1_data[NUM_PIXELS * NUM_CHANNELS];

typedef struct {
    uint8_t *data;
    uint data_len;
    uint frac_brightness; // 256 = *1.0;
} strip_t;

strip_t strip0 = {
        .data = strip0_data,
        .data_len = sizeof(strip0_data),
        //.frac_brightness = 0x100,
};
strip_t strip1 = {
        .data = strip1_data,
        .data_len = sizeof(strip1_data),
        //.frac_brightness = 0x100,
};

strip_t *strips[] = {
        &strip0,
        &strip1,
};

// ------------------- Framebuffer for patterns ------------------
// typedef uint8_t pixel_t[NUM_CHANNELS];
// typedef pixel_t strip_buffer_t[NUM_PIXELS];
typedef uint8_t strip_buffer_t[NUM_PIXELS][NUM_CHANNELS];
strip_buffer_t framebuf[NUM_STRIPS];   // fb[strip][pixel][channel], channel = G,R,B
// uint8_t framebuf[NUM_STRIPS][NUM_PIXELS][NUM_CHANNELS];   // fb[strip][pixel][channel], channel = G,R,B
bool update_framebuf = false;

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

// horrible temporary hack to avoid changing pattern code
static uint8_t *current_strip_out;
// static bool current_strip_4color;

static inline void put_pixel(uint32_t pixel_grb) {
    *current_strip_out++ = (pixel_grb >> 16u) & 0xffu;
    *current_strip_out++ = (pixel_grb >> 8u) & 0xffu;
    *current_strip_out++ = pixel_grb & 0xffu;
    //if (current_strip_4color) {
    //    *current_strip_out++ = 0;  // todo adjust?
    //}
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return 
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

void pattern_snakes(uint len, uint t) {
    for (uint i = 0; i < len; ++i) {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(urgb_u32(0xff, 0, 0));
        else if (x >= 15 && x < 25)
            put_pixel(urgb_u32(0, 0xff, 0));
        else if (x >= 30 && x < 40)
            put_pixel(urgb_u32(0, 0, 0xff));
        else
            put_pixel(0);
    }
}

void pattern_random(uint len, uint t) {
    if (t % 8)
        return;
    for (uint i = 0; i < len; ++i)
        put_pixel(rand());
}

void pattern_sparkle(uint len, uint t) {
    if (t % 8)
        return;
    for (uint i = 0; i < len; ++i)
        put_pixel(rand() % 16 ? 0 : 0xffffffff);
}

void pattern_greys(uint len, uint t) {
    uint max = 100; // let's not draw too much current!
    t %= max;
    for (uint i = 0; i < len; ++i) {
        put_pixel(t * 0x10101);
        if (++t >= max) t = 0;
    }
}

void pattern_solid(uint len, uint t) {
    t = 1;
    for (uint i = 0; i < len; ++i) {
        put_pixel(t * 0x10101);
    }
}

uint32_t jaremektable[] = {
        0x00F000, 0xF00000, 0x0000F0, 0xF0F0F0, 0x000000, 0x888800, 0x008888, 0x000000
};

void pattern_jaremek(uint len, uint t) {
    uint max = count_of(jaremektable);

    for(uint i = 0; i < len; ++i) {
        put_pixel(jaremektable[(t + i) % max]);
    }
}

typedef void (*pattern)(uint len, uint t);
const struct {
    pattern pat;
    const char *name;
} pattern_table[] = {
        {pattern_snakes,  "Snakes!"},
        {pattern_random,  "Random data"},
        {pattern_sparkle, "Sparkles"},
        {pattern_greys,   "Greys"},
        {pattern_solid,  "Solid!"},
        {pattern_jaremek, "Jaremek"},
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

void transform_strips(strip_t **strips, uint num_strips, value_bits_t *values, uint value_length) {
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

void ws2815_init(void) {
    PIO pio;
    uint sm;
    uint offset;
    // This function initializes the PIO and DMA for WS2815 control.
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2815_parallel_program, \
                                                                    &pio, &sm, &offset, \
                                                                    WS2815_PIN_BASE, \
                                                                    count_of(strips), \
                                                                    true);
    hard_assert(success);

    ws2815_parallel_program_init(pio, sm, offset, WS2815_PIN_BASE, count_of(strips), 800000);

    sem_init(&reset_delay_complete_sem, 1, 1); // initially posted so we don't block first time
    dma_init(pio, sm);

    // This is a stub function for the purpose of this example.
    // The actual implementation would involve initializing PIO and DMA
    // for WS2815 control.
    printf("WS2815 initialized.\n");
}

void ws2815_loop(void) {
    static int t = 0;
    static int loop_count = 0;
    static int pat=0, dir=0;
    
/*
    // Wait for reset delay to complete
    sem_acquire_blocking(&reset_delay_complete_sem);

    // Update strip data using a pattern
    for (uint i = 0; i < count_of(strips); i++) {
        current_strip_out = strips[i]->data;
        // current_strip_4color = (NUM_CHANNELS == 4);
        pattern_table[t % count_of(pattern_table)].pat(NUM_PIXELS * NUM_CHANNELS, t);
    }
    t++;

    // Transform strip data into bit planes
    transform_strips(strips, count_of(strips), colors, NUM_PIXELS * NUM_CHANNELS);

    // Start DMA to output the transformed data
    output_strips_dma(colors, NUM_PIXELS * NUM_CHANNELS);
 */
    {
        if (++loop_count > 1000) {
            loop_count = 0;
            // change pattern
            pat = rand() % count_of(pattern_table);
            dir = (rand() >> 30) & 1 ? 1 : -1;
            if (rand() & 1) dir = 0;
            printf("Pattern %d=%s dir:%s\n", pat + 1, pattern_table[pat].name, dir == 1 ? "(forward)" : dir ? "(backward)" : "(still)");
        }
        // for (int i = 0; i < 1000; ++i) {
        // current_strip_4color = false;
        current_strip_out = strip0.data;
        pattern_table[pat].pat(NUM_PIXELS, t);
        current_strip_out = strip1.data;
        pattern_table[pat].pat(NUM_PIXELS, t);

        // current_strip_out = &framebuf[0][0][0];     // = strip0.data;
        // pattern_table[pat].pat(NUM_PIXELS, t);
        // current_strip_out = &framebuf[1][0][0];     // = strip1.data;
        // pattern_table[pat].pat(NUM_PIXELS, t);

        // current_strip_out = &framebuf[2][0][0];
        // pattern_table[pat].pat(NUM_PIXELS, t);
        // current_strip_out = &framebuf[3][0][0];
        // pattern_table[pat].pat(NUM_PIXELS, t);
        // current_strip_out = &framebuf[4][0][0];
        // pattern_table[pat].pat(NUM_PIXELS, t);
        /*
        current_strip_out = strip2.data;
        pattern_table[pat].pat(NUM_PIXELS, t);
        current_strip_out = strip3.data;
        pattern_table[pat].pat(NUM_PIXELS, t);
        current_strip_out = strip4.data;
        pattern_table[pat].pat(NUM_PIXELS, t);
        */

        //printf("strip0.len=%d strip0.0=%#04x strip0.1=%#04x strip0.2=%#04x strip0.3=%#04x strip0.4=%#04x\n", strip0.data_len, strip0.data[0], strip0.data[1], strip0.data[2], strip0.data[3], strip0.data[4]);


        transform_strips(strips, count_of(strips), colors, NUM_PIXELS * NUM_CHANNELS);  // , brightness
        // transform_framebuf(framebuf, count_of(framebuf), colors, NUM_PIXELS * NUM_CHANNELS);

        //for(int c=0; c<3; c++) {
        //    printf("Color:%d\n", c);
        //    for(int p=0; p<8; p++) {
        //        printf("\tplane:%02d value=%#08x\n", p, colors[c].planes[p]);
        //    }
        //}
        //printf("colors[0].planes[0]=0x%x colors[0].planes[1]=0x%x colors[0].planes[2]=0x%x\n\n", colors[0].planes[0], colors[0].planes[1], colors[0].planes[2]);

                // dither_values(colors, states[current], states[current ^ 1], NUM_PIXELS * NUM_CHANNELS);
        sem_acquire_blocking(&reset_delay_complete_sem);
                // output_strips_dma(states[current], NUM_PIXELS * NUM_CHANNELS);
        output_strips_dma(colors, NUM_PIXELS * NUM_CHANNELS);
        // output_plains_sm(pio, sm, colors, NUM_PIXELS * NUM_CHANNELS);

        sleep_ms(10);    // oryginally 10ms, 1ms is too fast for sparkle
        // current ^= 1;
        t += dir;
        // brightness++;
        // if (brightness == (0x20 << FRAC_BITS)) brightness = 0;
        // }

    }
}

void ws2815_show(uint8_t *fb) {
    // uint8_t (*sb)[NUM_CHANNELS];
    // sb = framebuf[0];
    strip_buffer_t *pixel;
    pixel = &framebuf[0];

    memcpy(framebuf, fb, sizeof(framebuf));
    update_framebuf = true;
    // printf("Framebuf recieved. Value pixel[0]=%#04x,%#04x,%#04x pixel[1]=%#04x,%#04x,%#04x\n", sb[0][0], sb[0][1], sb[0][2], sb[1][0], sb[1][1], sb[1][2]);
    printf("Framebuf recieved. Value pixel[0]=%#04x,%#04x,%#04x pixel[1]=%#04x,%#04x,%#04x\n", pixel[0][0], pixel[0][1], pixel[0][2], pixel[1][0], pixel[1][1], pixel[1][2]);
}

