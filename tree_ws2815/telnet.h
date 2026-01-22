/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

void telnet_greeting(int sn, const uint8_t *client_ip);
void handle_command(const char *cmd, int sn);
