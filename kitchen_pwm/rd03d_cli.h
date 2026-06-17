/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <stdbool.h>
#include "rd03d_api.h"

void rd03d_cli_register(void);
void rd03d_cli_tick(void);
bool rd03d_cli_change_dump_continuous(void);
