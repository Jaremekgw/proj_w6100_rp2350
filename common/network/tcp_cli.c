/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * 
 */

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

