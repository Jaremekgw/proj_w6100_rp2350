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
#include "wizchip_spi.h"
#include "loopback.h"
// #include "wiznet_socket.h"
#include "config_kitchen.h"
#include "network.h"        // see tree
#include "wizchip_custom.h"
#include "efu_update.h"
#include "partition.h"
#include "flash_cfg.h"      // see tree
#include "vl53l8cx_drv.h"
#include "pwm_api.h"
#include "rd03d_api.h"
#include <stdio.h>


// variables for TCP loopback
// static uint8_t message_buf[2] = {
//     0, 0
// };

int32_t ret;

static repeating_timer_t timer;
volatile uint32_t tmr_ms_tick = 0;


// bool timer_callback(repeating_timer_t *rt) {
bool timer_callback() {
    tmr_ms_tick++;  // Increment every 1 ms
    return true;    // Return true to keep repeating
}



int main() {
    // variables for performance measurement
    // uint32_t time_start, time_diff, time_min=0, time_max=0;

    // --- MCU Init ---
    stdio_init_all(); // Initialize the main control peripheral. Sets up UART/USB for logging
    // debug: gpio_init(OE_PIN);
    // debug: gpio_set_dir(OE_PIN, GPIO_OUT);
    // debug: gpio_put(OE_PIN, OE_OFF);
    sleep_ms(2000);
    // sleep_ms(8000);

    // wizchip_sw_reset();            // Full chip reset via MR register - it creates a problem with ctlwizchip(CW_INIT_WIZCHIP, memsize)
    
    // --- WIZnet init ---
    // #define SPI_CLK  40      // remember to set in wizchip_spi.h speed 40 MHz to receive at 4 Mbps DDP
    wizchip_spi_initialize();   // sets up SPI hardware (not PIO)
    wizchip_cris_initialize();  // sets up interrupt control macros
    wizchip_reset();            // toggles GPIO for chip reset
    wizchip_init_nonblocking(); // non-blocking version of wizchip_initialize() for W6100
    wizchip_check();            // reads version register, verifies SPI comm

    // --- Set general configuration ---
    config_init();

    // --- Network init ---
    init_net_info();
    show_current_partition();
    efu_server_init();

    // --- Open UDP socket for DDP ---
    udp_socket_init();
    udp_interrupts_enable();          // sets up interrupts for UDP socket for DDP reception
    wiznet_gpio_irq_init();     // sets up GPIO interrupt for WIZnet IRQ pin

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
    rd03d_filter_cfg_t *cfg = NULL;
    rd03d_api_init(cfg);

    // Create repeating timer with 1 ms interval
    // if (!add_repeating_timer_ms(1, timer_callback, NULL, &timer)) {
    if (!add_repeating_timer_ms(1, timer_callback, NULL, &timer)) {
        printf("Failed to add timer\n");
        while (1);
    }

    // absolute_time_t last_log = get_absolute_time();
    // gpio_init(PIN_TEST_14);
    // gpio_init(PIN_TEST_15);
    // gpio_set_dir(PIN_TEST_14, GPIO_OUT);
    // gpio_set_dir(PIN_TEST_15, GPIO_OUT);
 
    // debug: gpio_put(OE_PIN, OE_ON);


    #ifndef OUTDOOR_TREE_WS2815
    printf("[VL53] Jump to loop\n");
    #endif  // OUTDOOR_TREE_WS2815

    // --- Main loop ---
    while (true) {
        // time_start = time_us_32();  // compare with get_absolute_time()
        // remove: loopback_loop(message_buf);
        tcp_cli_service();          // Socket 0 : CLI                       [5000]
        // http_server_service();   // TCP_HTTP_SOCKET : HTTP (future)      [80]
        // tcp_ota_service(); $ vim ../pico-examples/pico_w/wifi/ota_update/README.md
        efu_server_poll();          // TCP_EFU_SOCKET  : Eth-Fw-Upd         [4243]


        // Poll the UDP socket
        ddp_loop();       //  (&pkt_counter, &last_push_ms);

        pwm_api_poll();   // non-blocking, cheap

        rd03d_api_poll();

        // Manage ws2815 loop control
        // run_periodically_ws2815_tasks();    // ws2815_loop();

        // VL53L8CX non-blocking polling
        #ifdef VL53L8CX_DEV
        vl53l8cx_loop();
        #endif // VL53L8CX_DEV

        tight_loop_contents(); // yield to SDK
    }
}


