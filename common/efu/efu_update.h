/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #include "pico/stdlib.h"

uint8_t get_efu_socket_status(void);
void efu_server_init(uint8_t sn, uint16_t port);
void efu_server_poll(void);
