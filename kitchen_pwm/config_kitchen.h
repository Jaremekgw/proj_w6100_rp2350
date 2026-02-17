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
 * Pin connected to OE for TXB0108 - remove this because was used only for debugging
 */
// #define OE_PIN 22
// #define OE_OFF false
// #define OE_ON true

// #define PIN_TEST_14 14
// #define PIN_TEST_15 15

/**
 * Configuration for Flash memory Config partition
 */
#define CONFIG_FLASH_OFFSET 0x001f6000
#define CONFIG_SECTOR_SIZE  4096
// #define CONFIG_OFFSET   (CONFIG_FLASH_OFFSET + CONFIG_SECTOR_SIZE)
#define CONFIG_SIZE         (8 * 1024)

#define DATA_FLASH_OFFSET   (CONFIG_FLASH_OFFSET + CONFIG_SIZE)
#define DATA_SIZE           (40 * 1024)

/**
 * Configuration for networking
 * For checking OUI, see https://www.wireshark.org/tools/oui-lookup.html or https://maclookup.app/
 */
#define NETINFO_MAC     {0x00, 0x08, 0xDC, 0x12, 0x34, 0x59}    // MAC address; 00:08:DC Wiznet's OUI

#define NETINFO_IP      {192, 168, 14, 228}                  // IP address
#define NETINFO_SN      {255, 255, 255, 0}                      // Subnet Mask
#define NETINFO_GW      {192, 168, 14, 1}                      // Gateway
#define NETINFO_DNS     {192, 168, 14, 1}                            // DNS server

#if _WIZCHIP_ > W5500
#define NETINFO_LLA     {0xfe,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x08,0xdc,0xff,0xfe,0x57,0x57,0x25}
#define NETINFO_GUA     {0x00}
#define NETINFO_SN6     {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
#define NETINFO_GW6     {0x00}
#define NETINFO_DNS6    {0x20,0x01,0x48,0x60,0x48,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x88,0x88}
#define NETINFO_IPMODE  NETINFO_STATIC_ALL
#else
#define NETINFO_DHCP    NETINFO_STATIC
#endif

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

#define NUM_CHANNELS        4   // 3 for RGB, or 4 for RGBW - used in DDP (network.c)
#define NUM_PIXELS          1


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

// doc: I2C0_SDA --> 0,
// doc: I2C0_SCL --> 1,

// doc: I2C1_SDA --> 2, 6, 10, 14, 18, 22, 26, 30, 34, 38
// doc: I2C1_SCL --> 3, 7, 11, 15, 19, 23, 27, 31, 35, 39

// doc: UART0_TX --> 0, 2, 12, 14, 16, 18, 28, 30, 32
// doc: UART0_RX --> 1, 3, 13, 15, 17, 19, 29, 31, 33

// doc: UART1_TX --> 4, 6, 8, 10, 20, 22, 24, 26
// doc: UART1_RX --> 5, 7, 9, 11, 21, 23, 25, 27

/**
 * PWM configuration for RGBW LED driver
 */
#define WRAP_BITS   10      // bits for PWM_WRAP
#define PWM_WRAP ((1 << WRAP_BITS) - 1)  // 10-bit resolution
#define LINEAR_BITS 10
#define LINEAR_MAX ((1 << LINEAR_BITS) - 1) // 12 -> 4095 = 12-bit logical light domain
                                            // 10 -> 1023 = 10-bit logical light domain
#define PWM_MAX      PWM_WRAP     // hardware domain
#define PWM_MIN_ON   1

/**
 * Optionally 1 driver (gp3) or rgbw (gp4,5,6,7)
 */

// #define PWM_FREQ   600   // max value 600 kHz (min 100 Hz)
// for direct LED driviing set 2kHz
#define PWM_FREQ   2000   //

// #define PWM_LED_W    3  // GP4 -> PWM_1B     cos nie dziala, 
#define PWM_LED_W    4  // GP4 -> PWM_2A
#define PWM_LED_B    5  // GP5 -> PWM_2B
#define PWM_LED_R    6  // GP6 -> PWM_3A
#define PWM_LED_G    7  // GP7 -> PWM_3B

// // Fixed wiring
// #define VL53_PIN_SPI_I2C_N -1  // tied to 3V3 on PCB

//+5V                     //                    green
// GND                    //                    yellow
#define RD03D_TX_PIN  15  // GP15 -> UART0_RX   black
#define RD03D_RX_PIN  14  // GP14 -> UART0_TX   red
#define RD03D_BAUDRATE 256000  // default UART rate for RD03D

