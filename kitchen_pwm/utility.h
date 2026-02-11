/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <pico/stdio.h>

// CRC utility (standard reflected CRC32-IEEE)
uint32_t config_crc32(const void *data, size_t len);
// uint32_t config_crc32_hw(const void *data, size_t len);
uint32_t crc32_step(uint32_t crc, const uint8_t *buf, uint32_t len);

int msg_printf(char **cursor, size_t *remaining, const char *fmt, ...);


