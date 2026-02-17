/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * Important WIZnet board with W6100 configuration in library ioLibrary_Driver:
 * #define _PHY_IO_MODE_                  _PHY_IO_MODE_PHYCR_
 * #define SPI_CLK  40      // remember to set in wizchip_spi.h speed 40 MHz to receive at 4 Mbps DDP
 * 
 */

#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "wiznet_socket.h"
// #include "stdio.h"
// #include "string.h"
// #include "wizchip_spi.h"

/* for the following we need files from library
    wiz_NetInfo     // need: wizchip_conf.h  }wiz_NetInfo;
    SOCKET        // Ethernet/socket.h:93:#define SOCKET    uint8_t 
*/

void network_initialize(wiz_NetInfo *net_info);
void print_network_information(void);
void print_ipv6_addr(uint8_t* name, uint8_t* ip6addr);

void wiznet_gpio_irq_init(void);
void udp_interrupts_enable(void);
void udp_socket_init(void);
void init_net_info(void);
void wiznet_drain_udp(void);


// int32_t loopback_loop(uint8_t *msg);
int32_t tcp_cli_service(void);
int32_t ddp_loop(); // (uint32_t *pkt_counter, uint32_t *last_push_ms);

// void ddp_copy_payload(const uint8_t *payload, uint32_t offset, uint32_t length);
void process_ddp_udp(SOCKET s, uint32_t *pkt_counter, uint32_t *last_push_ms);

#endif /* _NETWORK_H_ */