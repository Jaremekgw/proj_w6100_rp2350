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
#include "ws2815_control_dma.h"
#include "ws2815.pio.h"
#include "led_pattern.h"

// const float ws_freq = 700000.0f;   //
// const float ws_freq = 800000.0f;   // WS2815 = 800 kHz nominal, shuld work 1000000.0f too
const float ws_freq = 1000000.0f;   // max 1MHz for WS2815

// --- for DDP protocol ---
#define DDP_COM_TIMEOUT_MS 2000         // ms delay to switch to pattern mode after last DDP packet
uint32_t ddp_update_timeout = 0;

const struct {
    pattern pat;
    const char *name;
} pattern_table[] = {
        {pattern_breath,  "Snake 1"},       // 1 ok
        {pattern_rainbow,  "Snake 1"},      // 2 ok
        {pattern_color_wipe,  "Snake 1"},   // 3 like train - weak - wykasowac
        {pattern_twinkle,  "Snake 1"},      // 4 sparkles
        {pattern_chase,  "Snake 1"},        // 5 one small train - red
        {pattern_fire,  "Snake 1"},         // 6 too reddish - kiepsko zmienilem
        {pattern_snow,  "Snake 1"},         // 7 funny
        {pattern_christmas_fade,  "Snake 1"},       // 8  red / green - slaby
        {pattern_christmas_fade_wave,  "Snake 1"},  // 9  red / green - ok

        {pattern_christmas_palette,  "Snake 1"},    // 10  ladne, spokojne, troche slabo widac zielone i niebieskie
        {pattern_warm_white_with_sparks,  "Snake 1"},         // 11 -- nice

        {pattern_falling_sparks,  "Snake 1"},   // 12 - do przetestowania na choince
        {pattern_ornaments,  "Snake 1"},        // 13 - statyczny dosyc ciekawy, sprawdzic jak na choince
        {pattern_ornaments_multicolor, "Ornament multicolor"},  // 14 - fajny ornament, ale nie zmienia kolorow
        {pattern_ornaments_cycling, "Ornament color cycle"},    // 15 - troche przeskakuja kolory, moze trzeba dopracowac
        {pattern_ornament_clusters, "Ornament cluster"},        // 16 - moze do poprawki, ale sprawdzic na choince
        // {pattern_falling_snow, "Drifting snow"},       // 16 - slabe, sprawdzic na choince - tak jak pattern_drifting_snow

        {pattern_global_color_fade, "Global color fade"},       // 17 - nie widze zeby cos sie zmienialo, wszystko swieci na bialo
        {pattern_cluster_color_fade, "Cluster color fade"},     // 18 - taki sam jak ornament cluster cycling

        {pattern_snakes1,  "Snake 1"},   // 19 ok, krotki
        {pattern_snakes2,  "Snake 2"},   // 20 kiepski, nie stosowac
        {pattern_snakes3,  "Snake 3"},   // 21 ok, dlugi
        {pattern_snakes4,  "Snake 4"},   // 22 calkiem ok, na poczatku tylko jakis impuls
        {pattern_snakes5,  "Snake 5"},   // 23 cos za jasny, inne lepsze

        {pattern_connection_show,  "Connect"},   // 24 do podlaczenia kabli
        {pattern_fade_show,  "Connect"},   // 24 do podlaczenia kabli

        
};
#define PAT_AUTO    200
#define PAT_ZERO    201
#define PAT_IDLE    202
uint8_t pattern_index = 0x00, pattern_last_index;


// --- global set from telnet ---
uint8_t rgb[3] = {0, 0, 0};
uint32_t max_led = NUM_PIXELS;

// LED framebuffer
uint32_t ws2815_buf[NUM_PIXELS];      // LED buffer



// --- for WS2815 ---

#if NUM_STRIPS > 1
#define WS2815_PARALLEL
#if NUM_STRIPS != 4
#error "Currently only NUM_STRIPS = 4 is supported in WS2815_PARALLEL mode"
#endif // NUM_STRIPS != 4
// #else
#endif // NUM_STRIPS > 1


#ifdef WS2815_PARALLEL
typedef struct {
    PIO     pio;
    uint    sm;
    uint    offset;
    uint    pin;               // GPIO for this strip
    uint32_t *buf;             // pointer to LED framebuffer
    uint     led_count;

    int      dma_ch;
    volatile bool dma_done;
    volatile bool refresh;

    absolute_time_t latch_deadline;
} ws_strip_t;

// uint32_t ws2815_buf_sm0[NUM_LEDS_SM0];
// uint32_t ws2815_buf_sm1[NUM_LEDS_SM1];
// uint32_t ws2815_buf_sm2[NUM_LEDS_SM2];
// uint32_t ws2815_buf_sm3[NUM_LEDS_SM3];

ws_strip_t strips[NUM_STRIPS] = {
    { .pin = WS2815_PIN_BASE,  .buf = ws2815_buf,
        .led_count = NUM_LEDS_SM0, .refresh = false },
    { .pin = WS2815_PIN_BASE+1,  .buf = ws2815_buf + NUM_LEDS_SM0,
        .led_count = NUM_LEDS_SM1, .refresh = false },
    { .pin = WS2815_PIN_BASE+2,  .buf = ws2815_buf + NUM_LEDS_SM0 + NUM_LEDS_SM1,
        .led_count = NUM_LEDS_SM2, .refresh = false },
    { .pin = WS2815_PIN_BASE+3,  .buf = ws2815_buf + NUM_LEDS_SM0 + NUM_LEDS_SM1 + NUM_LEDS_SM2,
        .led_count = NUM_LEDS_SM3, .refresh = false },

    // { .pin = WS2815_PIN_BASE,  .buf = ws2815_buf_sm0, .led_count = NUM_LEDS_SM0, .refresh = false },
    // { .pin = WS2815_PIN_BASE+1,  .buf = ws2815_buf_sm1, .led_count = NUM_LEDS_SM1, .refresh = false },
    // { .pin = WS2815_PIN_BASE+2,  .buf = ws2815_buf_sm2, .led_count = NUM_LEDS_SM2, .refresh = false },
    // { .pin = WS2815_PIN_BASE+3,  .buf = ws2815_buf_sm3, .led_count = NUM_LEDS_SM3, .refresh = false },

};


#else
// External globals (must be provided somewhere else)

// global defines for WS2815 control over single GPIO pin with direct access PIO
// #define NUM_LEDS 3    see NUM_PIXELS
// #define WS2815_MAX_LEDS   NUM_LEDS

PIO ws2815_pio;     // = pio0; using PIO instance 0
uint ws2815_sm;     // = 0; state machine index

bool ws2815_is_rgbw = false;
volatile bool ws2815_refresh_required = false;
// DMA state
int ws2815_dma_ch = -1;
volatile bool ws2815_dma_done = true;

// Latch timing
absolute_time_t ws2815_latch_deadline;
#endif // WS2815_PARALLEL



// version 2
// ---------------- DMA control code ----------------

#ifdef WS2815_PARALLEL
// Parallel version (4 strips simultaneously)
void __isr ws2815_dma_irq_handler(void)
{
    uint32_t flags = dma_hw->ints0;

    for (int i = 0; i < NUM_STRIPS; i++) {
        ws_strip_t *s = &strips[i];

        if (flags & (1u << s->dma_ch)) {
            dma_hw->ints0 = 1u << s->dma_ch;   // clear IRQ
            s->dma_done = true;
            s->latch_deadline = make_timeout_time_us(350);
        }
    }
}
#else
// Single strip version
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
#endif // WS2815_PARALLEL


#ifdef WS2815_PARALLEL
/**
 * Install DMA IRQ handler for WS2815
 */
void ws2815_dma_irq_setup(void)
{
    irq_set_exclusive_handler(DMA_IRQ_0, ws2815_dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

#else

/**
 * Install DMA IRQ handler for WS2815
 */
void ws2815_dma_irq_setup(void)
{
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler_ws2815);
    irq_set_enabled(DMA_IRQ_0, true);
}

#endif // WS2815_PARALLEL


#ifdef WS2815_PARALLEL

void ws2815_init_strip(ws_strip_t *s)
{
    // Claim SM and load program
    bool ok = pio_claim_free_sm_and_add_program_for_gpio_range(
                    &ws2815_program,
                    &s->pio,
                    &s->sm,
                    &s->offset,
                    s->pin,
                    1,
                    true);
    hard_assert(ok);

    // Initialize PIO SM
    ws2815_program_init(s->pio, s->sm, s->offset,
                        s->pin, 800000.0f, false);

    // Claim DMA
    s->dma_ch = dma_claim_unused_channel(true);
    s->dma_done = true;
    s->latch_deadline = make_timeout_time_us(300);

    dma_channel_config c = dma_channel_get_default_config(s->dma_ch);
    channel_config_set_dreq(&c, pio_get_dreq(s->pio, s->sm, true));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);

    dma_channel_configure(
        s->dma_ch,
        &c,
        &s->pio->txf[s->sm],   // write address
        NULL,                  // read = set before start
        0,
        false                  // don't start
    );

    dma_hw->inte0 |= 1u << s->dma_ch;
}

void ws2815_start_dma(ws_strip_t *s)
{
    if (!s->dma_done) return;

    s->dma_done = false;

    dma_channel_set_read_addr(s->dma_ch, s->buf, false);
    dma_channel_set_trans_count(s->dma_ch, s->led_count, true);
}


#else
/**
 * DMA configuration for WS2815
 * This config streams 32-bit LED words to the PIO TX FIFO.
 * 
 */
void ws2815_dma_init(void)
{
    ws2815_dma_ch = dma_claim_unused_channel(true);

    dma_channel_config c = dma_channel_get_default_config((uint)ws2815_dma_ch);
    channel_config_set_dreq(&c, pio_get_dreq(ws2815_pio, ws2815_sm, true));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);

    dma_channel_configure(
        (uint)ws2815_dma_ch,
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

    dma_channel_set_read_addr((uint)ws2815_dma_ch, ws2815_buf, false);
    dma_channel_set_trans_count((uint)ws2815_dma_ch, led_count, true);
}
#endif // WS2815_PARALLEL


// ---------------- WS2815 control code ----------------
#ifdef WS2815_PARALLEL
void ws2815_init(void)
{
    ws2815_dma_irq_setup();

    for (int i = 0; i < NUM_STRIPS; i++)
        ws2815_init_strip(&strips[i]);
}
#else
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
#endif // WS2815_PARALLEL


#ifdef WS2815_PARALLEL
// static void copy_linear_to_strip(uint32_t **buf, ws_strip_t *s) {
//     size_t bytes = s->led_count * sizeof(uint32_t);
//     uint32_t len = s->led_count;

//     if (dynamic_debug) {
//         printf("input buf addr=0x%08x len=%d\r\n", *buf, len);
//     }

//     memcpy(s->buf, *buf, bytes);
//     *buf += s->led_count;
//     s->refresh = true;
// }

static void refresh_parallel_buf() {
    // uint32_t *buf = ws2815_buf;

    for (int i = 0; i < NUM_STRIPS; i++) {
        strips[i].refresh = true;
        // copy_linear_to_strip(&buf, &strips[i]);
    }
}
#endif // WS2815_PARALLEL



/**
 * Main loop for createing different patterns automatically
 * Function called periodically every 20 ms from main loop
 */
void ws2815_pattern_loop(uint32_t period_ms) {
    static int counter = 100;
    static int pat=0;
    static int zero_counter = 0;

    if (ddp_update_timeout) {
        ddp_update_timeout =
            (ddp_update_timeout >= period_ms)? (ddp_update_timeout - period_ms) : 0;
        if (ddp_update_timeout == 0)
            printf("DDP communication timeout\n");
        return;
    }

    if (++counter >= 2) { // 20 means every 400 ms
        counter = 0;

        // pattern_simple(ws2815_buf, rgb, max_led);
        // pattern_snakes1(ws2815_buf, max_led, (int)1);
        // pattern_snakes2(ws2815_buf, max_led, (int)-1);

        if (pattern_index > 0 && pattern_index <= count_of(pattern_table)) {
            pat = pattern_index - 1;
            if (pattern_last_index != pattern_index) {
                pattern_last_index = pattern_index;
                // loop_count = 0;
                // dir = 1;
                // printf("Pattern %d=%s dir:%s\n", pat + 1, pattern_table[pat].name, dir == 1 ? "(forward)" : dir ? "(backward)" : "(still)");
                printf("Pattern %d=%s\n", pat + 1, pattern_table[pat].name);
            } 
            pattern_table[pat].pat(ws2815_buf, max_led, (int)1);
        } else {
            // pattern_warm_white_with_sparks(ws2815_buf, max_led, (int)1);
            if (zero_counter) {
                zero_counter--;
                return;
            } else {
                pattern_zero(ws2815_buf, max_led, (int)1);
                zero_counter = 500;   // keep zero pattern for 5*400ms = 2s
            }
        }

        #ifdef WS2815_PARALLEL
        // convert from linear buffer to buffers for each strip
            refresh_parallel_buf();
        #else
            ws2815_refresh_required = true;
        #endif // WS2815_PARALLEL

    }
}


/**
 * DMA controlled WS2815 output loop
 */
void ws2815_loop(uint32_t period_ms)
{
    (void)period_ms;
#ifdef WS2815_PARALLEL
    for (int i = 0; i < NUM_STRIPS; i++) {
        ws_strip_t *s = &strips[i];

        if (!s->dma_done)
            continue;

        if (!time_reached(s->latch_deadline))
            continue;

        if (!s->refresh)
            continue;

        s->refresh = false;
        ws2815_start_dma(s);
    }
#else
    if (!ws2815_dma_done)
        return;     // still transmitting frame

    if (!time_reached(ws2815_latch_deadline))
        return;     // waiting for latch period

    if (!ws2815_refresh_required)
        return;     // nothing to send

    ws2815_refresh_required = false;

    ws2815_start_dma_write(NUM_PIXELS);
#endif // WS2815_PARALLEL
}



// --- for DDP protocol ---

/**
 * copy data from ddp protocol to pixel buffer
 * from uint8_t strip_buffer[NUM_PIXELS][NUM_CHANNELS];
 * to: uint32_t ws2815_buf[NUM_PIXELS];
 * 
 * Input 3 bytes per pixel  (pixel_t[NUM_CHANNELS])
 * Output 4 bytes per pixel (uint32_t ws2815_buf[NUM_PIXELS])
 */
void ws2815_show(uint8_t *fb) {
    // typedef uint8_t pixel_t[NUM_CHANNELS];
    // pixel_t *in_pixel;
    int id;
    uint8_t *input_byte;
    uint8_t r, g, b;
    // uint32_t color;
    
    // in_pixel = (pixel_t*)fb;
    input_byte = fb;
    // pixel = &framebuf[0];

    for (id = 0; id < NUM_PIXELS; id++) {
        r = *input_byte++;
        g = *input_byte++;
        b = *input_byte++;
        // color = (r << 24) | (g << 16) | (b << 8);
        ws2815_buf[id] = (r << 24) | (g << 16) | (b << 8);
    }
  
#ifdef WS2815_PARALLEL
    // convert from linear buffer to buffers for each strip
    // convert_parallel_buf(ws2815_buf);
    refresh_parallel_buf();
#else
    ws2815_refresh_required = true;
#endif // WS2815_PARALLEL

    // memcpy(framebuf, fb, sizeof(framebuf)); // strips[16]*pixels[57]*channels[3] = 2736 bytes
    // ddp_update_framebuf = true;
    ddp_update_timeout = DDP_COM_TIMEOUT_MS;
    // printf("Framebuf recieved. Value pixel[0]=%#04x,%#04x,%#04x pixel[1]=%#04x,%#04x,%#04x\n", sb[0][0], sb[0][1], sb[0][2], sb[1][0], sb[1][1], sb[1][2]);
    printf("Framebuf recieved.\n");
}


uint8_t get_pattern_index(void) {
    return 0; // pattern_index;
}

void set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    rgb[0] = r;
    rgb[1] = g;
    rgb[2] = b;
}
void set_max_led(uint32_t max) {
    if (max < NUM_PIXELS)
        max_led = max;
    else
        max_led = NUM_PIXELS;
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


