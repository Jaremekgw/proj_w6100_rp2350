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
// #include "socket.h"
#include "config_tree.h"
#include "network.h"
#include "ws2815_control_dma.h"
#include "wizchip_custom.h"
#include "led_pattern.h"
#include "efu_update.h"
#include "partition.h"
#include "flash_cfg.h"
#include "vl53l8cx_drv.h"

// variables for TCP loopback
// static uint8_t message_buf[2] = {
//     0, 0
// };

int32_t ret;

static repeating_timer_t timer;
volatile uint32_t tmr_ms_tick = 0;

const network_t default_network = {
    .ip  = NETINFO_IP,
    .sn  = NETINFO_SN,
    .gw  = NETINFO_GW,
    .dns = NETINFO_DNS
};

bool timer_callback(repeating_timer_t *rt) {
    (void)rt;
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
        ws2815_pattern_loop(period_patt);
    }
    // Check WS2815 update loop every 2 ms
    if (tmr_ms_tick - last_tmr_loop >= period_loop) {
        last_tmr_loop += period_loop;
        ws2815_loop(period_loop);
    }
}


int main() {
    // variables for performance measurement
    // uint32_t time_start, time_diff, time_min=0, time_max=0;

    // --- MCU Init ---
    stdio_init_all(); // Initialize the main control peripheral. Sets up UART/USB for logging
    gpio_init(OE_PIN);
    gpio_set_dir(OE_PIN, GPIO_OUT);
    gpio_put(OE_PIN, OE_OFF);
    sleep_ms(2000);
    // sleep_ms(8000);

    // wizchip_sw_reset();            // Full chip reset via MR register - it creates a problem with ctlwizchip(CW_INIT_WIZCHIP, memsize)
    
    // --- WIZnet init ---
    // #define SPI_CLK  40      // remember to set in wizchip_spi.h speed 40 MHz to receive at 4 Mbps DDP
    wizchip_spi_initialize();   // sets up SPI hardware (not PIO)
    wizchip_cris_initialize();  // sets up interrupt control macros
    wizchip_reset();            // toggles GPIO for chip reset
    // wizchip_initialize();       // runs SPI-level init of W6100/W5500
    wizchip_init_nonblocking(); // non-blocking version of wizchip_initialize() for W6100
    wizchip_check();            // reads version register, verifies SPI comm

    // --- Set general configuration ---
    config_init(&default_network); // load configuration from flash or use default

    // --- Network init ---
    init_net_info();
    show_current_partition();
    efu_server_init(TCP_EFU_SOCKET, TCP_EFU_PORT);

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

    // --- LED driver init ---
    ws2815_init(); // Initialize WS2815 LED control


    // Create repeating timer with 1 ms interval
    if (!add_repeating_timer_ms(1, timer_callback, NULL, &timer)) {
        printf("Failed to add timer\n");
        while (1);
    }

    // absolute_time_t last_log = get_absolute_time();
    // gpio_init(PIN_TEST_14);
    // gpio_init(PIN_TEST_15);
    // gpio_set_dir(PIN_TEST_14, GPIO_OUT);
    // gpio_set_dir(PIN_TEST_15, GPIO_OUT);
 
    gpio_put(OE_PIN, OE_ON);
    init_start_strips();

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
        ret = ddp_loop();       //  (&pkt_counter, &last_push_ms);
  

        // Manage ws2815 loop control
        run_periodically_ws2815_tasks();    // ws2815_loop();

        // VL53L8CX non-blocking polling
        #ifdef VL53L8CX_DEV
        vl53l8cx_loop();
        #endif // VL53L8CX_DEV

        tight_loop_contents(); // yield to SDK
    }
}


