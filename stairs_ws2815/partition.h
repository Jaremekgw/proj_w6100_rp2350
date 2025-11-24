/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #pragma once

#include "stdint.h"
#include "string.h"
#include "pico/bootrom.h"

typedef struct {
    uint32_t start_offset;
    uint32_t start_addr;
    uint32_t end_offset;
    uint32_t end_addr;
    uint32_t size;
} partition_info_t;

void show_current_partition(void);
// uint32_t rom_get_other_image_addr(boot_info_t *boot_info);
int get_alt_part_addr(boot_info_t *boot_info, partition_info_t *part_info);
int partition_info(char *msg, size_t msg_max_sz);
void read_boot_info(void);
 
void util_flash_erase(uint32_t offs, uint32_t size);
void util_flash_read(uint32_t offs, void *data, uint32_t size);
void util_flash_write(uint32_t offs, const void *data, uint32_t size);