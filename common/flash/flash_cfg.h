/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <stdint.h>
#include "wizchip_conf.h"

#define CONFIG_MAGIC 0x434F4E46u   /* 'CONF' */

typedef struct {
    /* Versioning for forward compatibility */
    uint16_t version;       /* Increment when layout changes */
    uint16_t reserved;      /* Align to 32 bits */

    wiz_NetInfo net_info;

    /* MUST be last field */
    uint32_t crc32;
} __attribute__((packed)) config_t;


// void config_init(void);
void config_init(const network_t *default_network);
config_t *config_get(uint8_t select);

wiz_NetInfo *config_get_net_info(void);

int config_show(config_t *cfg, int id, char *msg, size_t msg_max_sz);
int config_load(void);
bool config_save(const config_t *cfg);

void config_default(void);
void config_recovery(void);
