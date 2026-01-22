/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <stdbool.h>
#include "vl53l8cx_api.h"

// Call once at boot
bool vl53l8cx_init_driver(void);

// Start ranging (after init)
bool vl53l8cx_start_drv_ranging(void);

// Call repeatedly from main loop
void vl53l8cx_loop(void);

VL53L8CX_Configuration *vl53_get_dev(void);


// #pragma once
// #include <stdbool.h>

// bool vl53l8cx_process(void);

// // void vl53l8cx_platform_init(void);
// void vl53l8cx_init_driver(void);   // your existing init
//     // vl53l8cx_start_ranging();