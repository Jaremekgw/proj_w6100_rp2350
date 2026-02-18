/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "wizchip_conf.h"

#define FW_VERSION "1.0.1"

// MOD_VER_OUTDOOR_TREE_WS2815
// #define OUTDOOR_TREE_WS2815



/**
 * Pin connected to OE for TXB0108
 */
#define OE_PIN 22
#define OE_OFF false
#define OE_ON true

// #define PIN_TEST_14 14
// #define PIN_TEST_15 15

// /**
//  * Configuration for Flash memory Config partition
//  */
// #define CONFIG_FLASH_OFFSET 0x001f6000
// #define CONFIG_SECTOR_SIZE  4096
// // #define CONFIG_OFFSET   (CONFIG_FLASH_OFFSET + CONFIG_SECTOR_SIZE)
// #define CONFIG_SIZE         (8 * 1024)

// #define DATA_FLASH_OFFSET   (CONFIG_FLASH_OFFSET + CONFIG_SIZE)
// #define DATA_SIZE           (40 * 1024)

/**
 * Configuration for networking
 * For checking OUI, see https://www.wireshark.org/tools/oui-lookup.html or https://maclookup.app/
 */
// #define NETINFO_MAC     {0x00, 0x08, 0xDC, 0x12, 0x34, 0x59}    // MAC address; 00:08:DC Wiznet's OUI

#define NETINFO_IP      {192, 168, 178, 226}                    // IP address
// #define NETINFO_IP      {192, 168, 14, 226}                  // IP address
#define NETINFO_SN      {255, 255, 255, 0}                      // Subnet Mask
#define NETINFO_GW      {192, 168, 178, 1}                      // Gateway
#define NETINFO_DNS     {8, 8, 8, 8}                            // DNS server

// #if _WIZCHIP_ > W5500
// #define NETINFO_LLA     {0xfe,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x08,0xdc,0xff,0xfe,0x57,0x57,0x25}
// #define NETINFO_GUA     {0x00}
// #define NETINFO_SN6     {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
// #define NETINFO_GW6     {0x00}
// #define NETINFO_DNS6    {0x20,0x01,0x48,0x60,0x48,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x88,0x88}
// #define NETINFO_IPMODE  NETINFO_STATIC_ALL
// #else
// #define NETINFO_DHCP    NETINFO_STATIC
// #endif

#define ETHERNET_BUF_MAX_SIZE 1024

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
#define CLI_TIMEOUT_MS      20000

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
 * Configuration for WS2815 LED strip
 * for project 'stairs' leds are connected parallel on GPIO0..GPIO15
 * for project 'tree' leds are connected individually on GPIO2..GPIO5
 * Possible NUM_STRIPS=1 and NUM_STRIPS=4
 * 
 * led strip for christmas tree has 591 leds. max 8A
 * 500 leds --> 2,99 A  (max 6,7 A)
 * 100 leds --> 1,1 A   (max 1,3 A)
 * 50 leds  --> 0,67 A
 */
#define NUM_CHANNELS        3   // 3 for RGB, or 4 for RGBW - used in DDP (network.c)
#define NUM_STRIPS          1  // 1 or 4 number of parallel strips being driven

#if NUM_STRIPS > 1

#define NUM_LEDS_SM0 30
#define NUM_LEDS_SM1 30
#define NUM_LEDS_SM2 30
#define NUM_LEDS_SM3 30
#define NUM_PIXELS NUM_LEDS_SM0 + NUM_LEDS_SM1 + NUM_LEDS_SM2 + NUM_LEDS_SM3

#else

// only if NUM_STRIPS = 1
#ifdef OUTDOOR_TREE_WS2815
#define NUM_PIXELS          409  // for outside leds
#else
#define NUM_PIXELS          591     // for christmas tree
#endif  //  OUTDOOR_TREE_WS2815

#endif  //  NUM_STRIPS > 1

#define WS2815_PIN_BASE     2   // first GPIO of 16 used for parallel output

#define _LOOPBACK_DEBUG_    // Enable LOOPBACK debug messages on USB
#define _DDP_DEBUG_         // Enable DDP debug messages on USB
#define _UDP_DEBUG_         // Enable UDP debug messages on USB
#define _TIME_DEBUG_        // Enable timing debug messages on USB
#define _EFU_DEBUG_         // Enable OTA debug messages on USB
// #define BOOT_INFO_ON_USB  // Enable boot info print on USB

/**
 * VL53L8CX – SPI1 or I2C (dedicated, independent from W6100 SPI0)
 */

// #define VL53L8CX_DEV // to implement communcation with VL53L8CX sensor
// #define VL53_SPI           spi1
#ifdef VL53_SPI
#define VL53_BAUDRATE      4   // 4 MHz for bring-up
#else
#define VL53_I2C_PORT     i2c1
#define VL53_I2C_ADDR     0x52
// The I²C bus on the VL53L8CX has a maximum speed of 1 Mbit/s/s and uses a device 8-bit address of 0x52
#define VL53_BAUDRATE      1   // 1 MHz for bring-up
#endif // VL53_SPI

// pin configurations
#ifdef VL53_SPI
#define VL53_PIN_INT       8  // GP8  -> INT (active LOW, open-drain)   doc: SPI1_RX, I2C0_SDA
#define VL53_PIN_MISO      12 // GP12  -> MISO                          doc: SPI1_RX, I2C0_SDA
#define VL53_PIN_SCK       10 // GP10  -> SCL / CLK                     doc: SPI1_SCLK, I2C1_SDA
#define VL53_PIN_MOSI      11 // GP11  -> SDA / MOSI                    doc: SPI1_TX, I2C1_SCL
#define VL53_PIN_CS        9  // GP9 -> NCS (active LOW)                doc: SPI1_SS_N, I2C0_SCL
// #define VL53_PIN_LPN       11  // GP11 -> LPn (enable / reset control)
#else
#define VL53_PIN_INT       8  // GP8    -> INT (active LOW, open-drain)
#define VL53_PIN_SCL       11 // GP11   -> SDA / MOSI       doc: I2C1_SCL
#define VL53_PIN_SDA       10 // GP10   -> SCL / CLK        doc: I2C1_SDA
#define VL53_PIN_CS        9  // GP9    -> NCS (active LOW)

#endif // VL53_SPI


// // Fixed wiring
// #define VL53_PIN_SPI_I2C_N -1  // tied to 3V3 on PCB



