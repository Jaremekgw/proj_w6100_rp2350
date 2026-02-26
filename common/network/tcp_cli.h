/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

// #include <stdio.h>
// #include <string.h>
#include <stdint.h>


typedef void (*tcp_cli_on_connect_fn)(uint8_t sn, const uint8_t *client_ip);
typedef void (*tcp_cli_handle_command_fn)(const char *cmd, uint8_t sn);
typedef struct
{
    tcp_cli_on_connect_fn on_connect;   // may be NULL
    tcp_cli_handle_command_fn handle_command; // may be NULL
} tcp_cli_hooks_t;

// void telnet_send(uint8_t sn, const char *msg);
void cli_send(uint8_t sn, const char *msg);
void cli_flush(uint8_t sn, const char *msg);

