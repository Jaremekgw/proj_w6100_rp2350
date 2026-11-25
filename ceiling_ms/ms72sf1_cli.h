/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <stdbool.h>
#include "ms72sf1_api.h"

void ms72sf1_cli_register(void);
void ms72sf1_cli_tick(void);
bool ms72sf1_cli_change_dump_continuous(void);
