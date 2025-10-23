/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

/**
 * Configuration for networking
 * For checking OUI, see https://www.wireshark.org/tools/oui-lookup.html or https://maclookup.app/
 */
#define NETINFO_MAC     {0x00, 0x08, 0xDC, 0x12, 0x34, 0x59}    // MAC address; 00:08:DC Wiznet's OUI
#define NETINFO_IP      {192, 168, 178, 225}                    // IP address
#define NETINFO_SN      {255, 255, 255, 0}                      // Subnet Mask
#define NETINFO_GW      {192, 168, 178, 1}                      // Gateway
#define NETINFO_DNS     {8, 8, 8, 8}                            // DNS server

#define TCP_LOOPBACK_PORT  8000

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

/**
 * Configuration for UDP DDP protocol
 */
#define UDP_DDP_PORT            4048

/**
 * Configuration for WS2815 LED strip
 */
#define NUM_CHANNELS        3   // 3 for RGB, or 4 for RGBW
#define NUM_PIXELS          57  // number of pixels per strip, steps: 1-14=55px; 15-16=57px
#define NUM_STRIPS          16  // number of parallel strips being driven
#define WS2815_PIN_BASE     0   // first GPIO of 16 used for parallel output
