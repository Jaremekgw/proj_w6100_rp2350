/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 #pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "hardware/spi.h"
#include "hardware/i2c.h"

/*
 * This file IMPLEMENTS the ST VL53L8CX platform layer
 * using SPI instead of I2C.
 *
 * Function names and signatures MUST match ST expectations
 * (used inside vl53l8cx_api.c).
 */

/* -------------------------------------------------------------------------- */
/* ST-required platform structure                                              */
/* -------------------------------------------------------------------------- */

typedef struct
{
    #ifdef VL53_SPI
    // --- Transport selection ---
    spi_inst_t *spi_port;      // SPI instance (spi0 / spi1)

    // --- SPI wiring ---
    uint8_t     pin_int;  // Interrupt (optional)
    uint8_t     pin_miso;
    uint8_t     pin_sck;
    uint8_t     pin_mosi;
    uint8_t     pin_cs;   // Chip select
    // uint8_t     pin_lpn;  // LPn (optional)
    #else
    i2c_inst_t *i2c_port;    // I2C instance (i2c0 / i2c1)
    uint8_t     pin_sda;
    uint8_t     pin_scl;
    uint8_t     pin_cs;   // Chip select (optional)
    uint8_t     pin_int;  // Interrupt (optional)
    // uint8_t     pin_lpn;  // LPn (optional)
    uint8_t     i2c_addr; // 8-bit I2C address
    #endif // VL53_SPI
    // --- Timing / options ---
    uint32_t    baudrate;
    uint8_t     flags;

} vl53l8cx_platform_t;
typedef vl53l8cx_platform_t VL53L8CX_Platform;

#ifdef VL53_SPI
void vl53_spi_init(vl53l8cx_platform_t *p_platform);
#else
void vl53_i2c_init(vl53l8cx_platform_t *p_platform);
#endif // VL53_SPI

void vl53_gpio_init(vl53l8cx_platform_t *p_platform);

#ifdef VL53_SPI
uint vl53l8cx_spi_get_baudrate(vl53l8cx_platform_t *p_platform);
#endif // VL53_SPI

void vl53_cs_set_active(vl53l8cx_platform_t *p_platform);
void vl53_cs_set_inactive(vl53l8cx_platform_t *p_platform);

/* -------------------------------------------------------------------------- */
/* Mandatory ST platform functions                                             */
/* -------------------------------------------------------------------------- */

uint8_t RdByte(
    vl53l8cx_platform_t *p_platform,
    uint16_t RegisterAdress,
    uint8_t *p_value
);

uint8_t WrByte(
    vl53l8cx_platform_t *p_platform,
    uint16_t RegisterAdress,
    uint8_t value
);

uint8_t RdMulti(
    vl53l8cx_platform_t *p_platform,
    uint16_t RegisterAdress,
    uint8_t *p_values,
    uint32_t size
);


uint8_t WrMulti(
    vl53l8cx_platform_t *p_platform,
    uint16_t RegisterAdress,
    uint8_t *p_values,
    uint32_t size
);

uint8_t WaitMs(
    vl53l8cx_platform_t *p_platform,
    uint32_t TimeMs
);

void SwapBuffer(
    uint8_t *buffer,
    uint16_t size
);

// // --- SPI access ---
// int32_t vl53_platform_write(
//     uint16_t reg,
//     const uint8_t *buf,
//     size_t len
// );

// int32_t vl53_platform_read(
//     uint16_t reg,
//     uint8_t *buf,
//     size_t len
// );

// // --- Timing ---
// void vl53_platform_delay_ms(uint32_t ms);

// --- GPIO helpers ---
bool vl53_platform_int_asserted(vl53l8cx_platform_t *p_platform);


// // #pragma once
// // #include <stdint.h>
// // #include <stddef.h>

// // int32_t vl53_platform_write(
// //     uint16_t reg,
// //     const uint8_t *buf,
// //     size_t len
// // );

// // int32_t vl53_platform_read(
// //     uint16_t reg,
// //     uint8_t *buf,
// //     size_t len
// // );

// // void vl53_platform_delay_ms(uint32_t ms);

