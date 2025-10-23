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
static uint32_t pkt_counter = 0;
static uint32_t last_push_ms = 0;
int32_t ret;

int main() {
    // variables for performance measurement
    uint32_t time_start, time_diff, time_min=0, time_max=0;

    // --- MCU Init ---
    stdio_init_all(); // Initialize the main control peripheral. Sets up UART/USB for logging
    sleep_ms(3000);
    sleep_ms(8000);

    // --- WIZnet init ---
    wizchip_spi_initialize();   // sets up SPI hardware (not PIO)
    wizchip_cris_initialize();  // sets up interrupt control macros
    wizchip_reset();            // toggles GPIO for chip reset
    // wizchip_initialize();       // runs SPI-level init of W6100/W5500
    wizchip_init_nonblocking(); // non-blocking version of wizchip_initialize() for W6100

    wizchip_check();            // reads version register, verifies SPI comm

    // network_initialize(g_net_info); // configures IP address etc.
    // print_network_information(g_net_info); // Read back the configuration information and print it
    init_net_info();

    // --- LED driver init ---
    ws2815_init(); // Initialize WS2815 LED control

    // --- Open UDP socket for DDP ---
    // SOCKET ddp_sock = 0;
    // uint8_t status;
    // if (socket(ddp_sock, Sn_MR_UDP, UDP_DDP_PORT, 0) != ddp_sock) {
    //     printf("UDP socket open failed\n");
    //     while (1) tight_loop_contents();
    // }
    // printf("DDP UDP listening on port %d\n", UDP_DDP_PORT);

    // absolute_time_t last_log = get_absolute_time();
 
    // --- Main loop ---
    while (true) {
        time_start = time_us_32();
        loopback_loop(message_buf);
 
        // Poll the UDP socket
        ret = ddp_loop(&pkt_counter, &last_push_ms);
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
            if (message_buf[0] == 'i') {
                // Loop time us: min 6us max 416us
                printf("Loop time us: min %dus max %dus\r\n", time_min, time_max);
            } else if (message_buf[0] == 'r') {
                // for (uint i = 0; i < NUM_PIXELS; i++) {
                //     // Set random colors for demonstration
                //     ws2815_set_pixel_color(i, rand() % 256, rand() % 256, rand() % 256);
                // }
                // ws2815_show(); // Update the WS2815 LED strip with current colors
                printf("Set random colors for demonstration\r\n");
            }
            message_buf[0] = 0;
        }
 
        // Manage ws2815 loop control
        ws2815_loop();

        tight_loop_contents(); // yield to SDK
    }
}


