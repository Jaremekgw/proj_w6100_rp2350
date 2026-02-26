/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * 
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "network.h"

#define TELNET_PROMPT  "> "

void cli_send(uint8_t sn, const char *msg) {
    // Send the main message first
    send(sn, (uint8_t *)msg, (uint16_t)strlen(msg));
}

void cli_flush(uint8_t sn, const char *msg) {
    // Send the main message first
    send(sn, (uint8_t *)msg, (uint16_t)strlen(msg));

    // Then send the prompt
    const char *prompt = TELNET_PROMPT;
    send(sn, (uint8_t*)prompt, (uint16_t)strlen(prompt));
}

// --- Common functions for telnet ---
bool parse_ipv4(const char *s, uint8_t out[4]) {
    int a, b, c, d;
    if (sscanf(s, "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
        return false;
    if ((a|b|c|d) & ~0xFF)
        return false;
    out[0] = (uint8_t)a;
    out[1] = (uint8_t)b;
    out[2] = (uint8_t)c;
    out[3] = (uint8_t)d;
    return true;
}

