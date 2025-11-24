/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #include "stdlib.h"
 #include "partition.h"


/* 
In C/C++ you can get its location with the Pico SDK partition API:

const pico_partition_t *cfg = pico_partition_find_first("Config", NULL, NULL);
if (cfg) {
    printf("Config partition at 0x%08lx, size %lu bytes\n",
           cfg->flash_offset, cfg->size_bytes);
}

Then use the flash APIs to read/write:

flash_range_erase(cfg->flash_offset, FLASH_SECTOR_SIZE);
flash_range_program(cfg->flash_offset, data, len);

Always erase before writing and ensure you’re running 
from XIP-safe code (SDK’s hardware_flash handles this).

 */


