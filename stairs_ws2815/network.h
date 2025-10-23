/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdint.h>
#include "socket.h"
// #include "stdio.h"
// #include "string.h"
// #include "wizchip_spi.h"



void init_net_info(void);
int32_t loopback_loop(uint8_t *msg);
int32_t ddp_loop(uint32_t *pkt_counter, uint32_t *last_push_ms);
// void ddp_copy_payload(const uint8_t *payload, uint32_t offset, uint32_t length);
void process_ddp_udp(SOCKET s, uint32_t *pkt_counter, uint32_t *last_push_ms);

#endif /* _NETWORK_H_ */