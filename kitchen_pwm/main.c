/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// #include <stdio.h>
#include "pico/stdio.h"
#include "stdlib.h"
// #include "port_common.h"

#include "wizchip_conf.h"
#include "loopback.h"
#include "config.h"
#include "network.h"
#include "wizchip_custom.h"
#include "efu_update.h"
#include "partition.h"
#include "flash_cfg.h"
#include "vl53l8cx_drv.h"
#include "pwm_api.h"
#include "rd03d_api.h"
#include <stdio.h>
#include "telnet.h"
// for debug: #include "rd03d_drv.h"
#include "rd03d_drv.h"
#include "rd03d_cli.h"


static repeating_timer_t timer;
volatile uint32_t tmr_ms_tick = 0;

const network_t default_network = {
    .ip  = NETINFO_IP,
    .sn  = NETINFO_SN,
    .gw  = NETINFO_GW,
    .dns = NETINFO_DNS
};

// bool timer_callback(repeating_timer_t *rt) {
bool timer_callback() {
    tmr_ms_tick++;  // Increment every 1 ms
    return true;    // Return true to keep repeating
}

// DDP variables
//                            (NUM_STRIPS*NUM_PIXELS*NUM_CHANNELS)
#define DDP_DATA_BUF_SIZE     (NUM_PIXELS*NUM_CHANNELS)  // clamp to your RAM
uint8_t ddp_buf_frame[DDP_DATA_BUF_SIZE]; // buffer for receiving DDP packets
// uint8_t rx_fb[NUM_STRIPS*NUM_PIXELS*NUM_CHANNELS]; // flat rx buffer for UDP DDP packets


int main() {
    // int32_t ret;

    // --- MCU Init ---
    stdio_init_all(); // Initialize the main control peripheral. Sets up UART/USB for logging
    // debug: gpio_init(OE_PIN);
    // debug: gpio_set_dir(OE_PIN, GPIO_OUT);
    // debug: gpio_put(OE_PIN, OE_OFF);
    sleep_ms(2000);
    // sleep_ms(8000);

    // --- WIZnet init with network configuration ---
    wizchip_custom_init(&default_network);

    show_current_partition();

    // --- Eth-Fw-Upd server init ---
    efu_server_init(TCP_EFU_SOCKET, TCP_EFU_PORT);

    // --- Telnet CLI init ---
    telnet_init();
 
    // --- Open UDP socket for DDP ---
    udp_ddp_init(UDP_DDP_SOCKET, UDP_DDP_PORT, ddp_buf_frame, DDP_DATA_BUF_SIZE);

    #ifndef OUTDOOR_TREE_WS2815
    // --- VL53L8CX driver init ---
    #ifdef VL53L8CX_DEV
    printf("[VL53] Init start\n");
    vl53l8cx_init_driver();
    printf("[VL53] Start ranging\n");
    vl53l8cx_start_drv_ranging();       // vl53l8cx_start_ranging();
    #endif // VL53L8CX_DEV
    #endif  // OUTDOOR_TREE_WS2815

    sleep_ms(500);

    // --- LED driver init ---
    pwm_mod_init();

    // --- radar sensor RD03D driver init ---
    // Many radar modules need: 100–500 ms after power-up
    rd03d_filter_cfg_t *cfg = NULL;
    rd03d_api_init(cfg);

    // Create repeating timer with 1 ms interval
    // if (!add_repeating_timer_ms(1, timer_callback, NULL, &timer)) {
    if (!add_repeating_timer_ms(1, timer_callback, NULL, &timer)) {
        printf("Failed to add timer\n");
        while (1);
    }

    // debug: gpio_put(OE_PIN, OE_ON);

    #ifndef OUTDOOR_TREE_WS2815
    printf("[VL53] Jump to loop\n");
    #endif  // OUTDOOR_TREE_WS2815

    // --- Main loop ---
    while (true) {
        // Poll the TCP CLI for telnet connections and commands
        tcp_cli_service();          // Socket 0 : CLI                       [5000]

        // Poll the Eth-Fw-Upd server for incoming firmware update requests
        efu_server_poll();          // TCP_EFU_SOCKET  : Eth-Fw-Upd         [4243]

        // Poll the UDP socket
        ddp_loop();

        // non-blocking, cheap
        pwm_api_poll();

        // sensor radar RD03D non-blocking polling
        // first check if data parsed, then call driver poll to fetch new data, to keep latency low and avoid starving main loop
        rd03d_api_poll();
        rd03d_drv_poll();
        // rd03d_cli_tick();

        // Manage ws2815 loop control
        // run_periodically_ws2815_tasks();

        // VL53L8CX non-blocking polling
        #ifdef VL53L8CX_DEV
        vl53l8cx_loop();
        #endif // VL53L8CX_DEV

        tight_loop_contents(); // yield to SDK
    }
}


