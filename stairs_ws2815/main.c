/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include "port_common.h"

#include "wizchip_conf.h"
#include "wizchip_spi.h"
#include "loopback.h"
#include "socket.h"
#include "config.h"
#include "network.h"
#include "ws2815_control_dma_parallel.h"
#include "wizchip_custom.h"


// variables for TCP loopback
static uint8_t message_buf[2] = {
    0, 0
};

// variables for DDP processing
//static uint32_t pkt_counter = 0;
//static uint32_t last_push_ms = 0;
int32_t ret;

static repeating_timer_t timer;
volatile uint32_t tmr_ms_tick = 0;

bool timer_callback(repeating_timer_t *rt) {
    tmr_ms_tick++;     // Increment every 1 ms
    return true;    // Return true to keep repeating
}

void run_periodically_ws2815_tasks(void) {
    static uint32_t last_tmr_loop = 0;
    static uint32_t last_tmr_patt = 0;
    uint32_t period_loop = 2;   // ms
    uint32_t period_patt = 20;  // ms

    // Create pattern, in the loop every 20 ms
    if (tmr_ms_tick - last_tmr_patt >= period_patt) {
        last_tmr_patt += period_patt;
        ws2815_pattern_loop();
    }
    // Check WS2815 update loop every 2 ms
    if (tmr_ms_tick - last_tmr_loop >= period_loop) {
        last_tmr_loop += period_loop;
        ws2815_loop();
    }
}


int main() {
    // variables for performance measurement
    uint32_t time_start, time_diff, time_min=0, time_max=0;

    // --- MCU Init ---
    stdio_init_all(); // Initialize the main control peripheral. Sets up UART/USB for logging
    sleep_ms(3000);
    sleep_ms(8000);

    // wizchip_sw_reset();            // Full chip reset via MR register - it creates a problem with ctlwizchip(CW_INIT_WIZCHIP, memsize)

    // --- WIZnet init ---
    // #define SPI_CLK  40      // remember to set in wizchip_spi.h speed 40 MHz to receive at 4 Mbps DDP
    wizchip_spi_initialize();   // sets up SPI hardware (not PIO)
    wizchip_cris_initialize();  // sets up interrupt control macros
    wizchip_reset();            // toggles GPIO for chip reset
    // wizchip_initialize();       // runs SPI-level init of W6100/W5500
    wizchip_init_nonblocking(); // non-blocking version of wizchip_initialize() for W6100
    wizchip_check();            // reads version register, verifies SPI comm
    // --- Network init ---
    init_net_info();

    // --- Open UDP socket for DDP ---
    udp_socket_init();
    udp_interrupts_enable();          // sets up interrupts for UDP socket for DDP reception
    wiznet_gpio_irq_init();     // sets up GPIO interrupt for WIZnet IRQ pin

    // --- LED driver init ---
    ws2815_init(); // Initialize WS2815 LED control


    // Create repeating timer with 1 ms interval
    if (!add_repeating_timer_ms(1, timer_callback, NULL, &timer)) {
        printf("Failed to add timer\n");
        while (1);
    }

    // absolute_time_t last_log = get_absolute_time();
 
    // --- Main loop ---
    while (true) {
        time_start = time_us_32();  // compare with get_absolute_time()
        loopback_loop(message_buf);
 
        // Service RX immediately if IRQ flagged
        // wiznet_service_if_needed();  inside ddp_loop()


        // Poll the UDP socket
        ret = ddp_loop();       //  (&pkt_counter, &last_push_ms);
            // status = getSn_SR(ddp_sock);
            // if (status == SOCK_UDP) {
            //     process_ddp_udp(ddp_sock, &pkt_counter, &last_push_ms);
            // } else if (status == SOCK_CLOSED) {
            //     // re-open if closed
            //     socket(ddp_sock, Sn_MR_UDP, UDP_DDP_PORT, 0);
            // }
/*
        // optional timeout fallback (no PUSH)
        if (absolute_time_diff_us(last_log, get_absolute_time()) > 2e6) {
            printf("Packets: %lu | Last render %lu ms ago\n",
                   pkt_counter,
                   to_ms_since_boot(get_absolute_time()) - last_push_ms);
            last_log = get_absolute_time();
        }
 */
 
        time_diff = time_us_32() - time_start;
        if (time_diff < time_min || time_min == 0) {
            time_min = time_diff;
        }
        if (time_diff > time_max) {
            time_max = time_diff;
        }

        if (message_buf[0] != 0) {
            switch (message_buf[0]) {
                case 'i':
                    // Loop time us: min 6us max 416us
                    printf("Loop time us: min %dus max %dus\r\n", time_min, time_max);
                    break;         
                case 'r':
                    set_pattern_index(0xF0); // random pattern
                    printf("Set random pattern for demonstration\r\n");
                    break;
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                    uint8_t patid = message_buf[0] - '1';
                    set_pattern_index(patid); // set pattern by index
                    printf("Set pattern:%d [1-6].\n", patid+1);
                    break;                    
                default:
            }
            message_buf[0] = 0;
        }
 
        // Manage ws2815 loop control
        run_periodically_ws2815_tasks();    // ws2815_loop();

        tight_loop_contents(); // yield to SDK
    }
}


