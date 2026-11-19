/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "wizchip_conf.h"

#define PROJECT_NAME "tarrace_ws2815"
#define FW_VERSION "1.0.1"
// WS2805 data lines are driven directly from the RP2350 GPIOs (no level
// shifter / output-enable buffer in this build).

// #define PIN_TEST_14 14
// #define PIN_TEST_15 15

// /**
//  * Configuration for Flash memory Config partition
//  */
// #define CONFIG_FLASH_OFFSET 0x001f6000
// #define CONFIG_SECTOR_SIZE  4096
// #define CONFIG_DATA_OFFSET (CONFIG_FLASH_OFFSET + CONFIG_SECTOR_SIZE)
// #define CONFIG_DATA_SIZE   (32*1024 - CONFIG_SECTOR_SIZE)

/**
 * Configuration for networking
 * For checking OUI, see https://www.wireshark.org/tools/oui-lookup.html or https://maclookup.app/
 */
// #define NETINFO_MAC     {0x00, 0x08, 0xDC, 0x12, 0x34, 0x59}    // MAC address; 00:08:DC Wiznet's OUI

#define NETINFO_IP      {192, 168, 14, 229}                    // IP address
#define NETINFO_SN      {255, 255, 255, 0}                      // Subnet Mask
#define NETINFO_GW      {192, 168, 14, 1}                      // Gateway
#define NETINFO_DNS     {8, 8, 8, 8}                            // DNS server


/**
 * Configuration for TCP LOOPBACK
 * Not used currently
 */
//#define TCP_LOOPBACK_SOCKET 0      // with port TCP_LOOPBACK_PORT 8000
//#define TCP_LOOPBACK_PORT   8000

/**
 * Configuration for TCP CLI protocols
 * connect example:
 *   $ nc 192.168.14.225 5000
 *   $ telnet 192.168.14.225 5000
 */
#define TCP_CLI_SOCKET      0
#define TCP_CLI_PORT        5000
#define CLI_TIMEOUT         20   // 20 seconds
#define CLI_BUF_RX_SIZE     1024

/**
 * An Over-The-Air (OTA) software update mechanism
 * 
 * ~/project/pico2/pico-examples$ vim pico_w/wifi/ota_update/README.md
 * 
 */
#define TCP_EFU_SOCKET      1
#define TCP_EFU_PORT        4243    // OTA port=4242

/**
 * Configuration for UDP protocols
 */
#define UDP_DDP_SOCKET      5      // with port UDP_DDP_PORT 4048
#define UDP_DDP_PORT        4048

/**
 * Configuration for future
 */
// #define TCP_HTTP_SOCKET  1
// #define TCP_HTTP_PORT    80
// #define TCP_OTA_SOCKET   2
// #define TCP_OTA_PORT     4242
//  Remote logging & variable view (for live debug)
//  Example: a UDP packet every second with JSON text like
//  {"t":27.5,"uptime":3221,"fps":60}
// #define UDP_DEB_SOCKET   3
// #define UDP_DESTIP       192.168.14.200
// #define UDP_DESTPORT     3000

/**
 * Configuration for WS2805 LED strip
 */
#define NUM_CHANNELS        5   // 3 for RGB, 4 for RGBW, 5 for RGBCCT (WS2805)
#define NUM_PIXELS          40  // 40  number of pixels per strip, steps: 1-14=55px; 15-16=57px
#define NUM_STRIPS          16  // number of parallel strips being driven - later cnange to 7
#define WS2815_PIN_BASE     0   // first GPIO of 16 used for parallel output

/**
 * Data line polarity.
 * 0 = normal, non-inverted, idle-LOW signal (direct GPIO drive). This is
 *     correct for WS2805.
 * 1 = invert the GPIO output, for use with an INVERTING buffer/level shifter
 *     (single-transistor stage, 74HCT14, etc.).
 */
#define DATA_INVERT         0

/**
 * Mapping from framebuffer channel order (as written by patterns / received
 * over DDP) to the on-the-wire channel order expected by the LED chip.
 *   index  = framebuffer channel (0=R, 1=G, 2=B, 3=C/warm, 4=T/cold)
 *   value  = position of that channel within the pixel on the wire
 * WorldSemi chips send Green first, so R,G,B -> G,R,B. The extra WS2805
 * white / colour-temperature channels are appended after the RGB triplet.
 */
#if   NUM_CHANNELS == 3
#define CHANNEL_WIRE_ORDER  { 1, 0, 2 }           // RGB      -> G R B
#elif NUM_CHANNELS == 4
#define CHANNEL_WIRE_ORDER  { 1, 0, 2, 3 }        // RGBW     -> G R B W
#elif NUM_CHANNELS == 5
#define CHANNEL_WIRE_ORDER  { 1, 0, 2, 3, 4 }     // RGBCCT   -> G R B C T
#else
#error "Define CHANNEL_WIRE_ORDER for this NUM_CHANNELS"
#endif

#define _LOOPBACK_DEBUG_    // Enable LOOPBACK debug messages on USB
#define _DDP_DEBUG_         // Enable DDP debug messages on USB
#define _UDP_DEBUG_         // Enable UDP debug messages on USB
#define _TIME_DEBUG_        // Enable timing debug messages on USB
#define _EFU_DEBUG_         // Enable OTA debug messages on USB
// #define BOOT_INFO_ON_USB  // Enable boot info print on USB
