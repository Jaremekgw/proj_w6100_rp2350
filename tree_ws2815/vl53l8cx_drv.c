/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <stdio.h>
#include <pico/time.h>
#include "config_tree.h"

#include "vl53l8cx_drv.h"
#include "vl53l8cx_platform.h"
#include "vl53l8cx_api.h"   // ST ULD

// ===== ST device instance =====
static vl53l8cx_dev_t dev;
static vl53l8cx_dev_t *p_dev = &dev;
static bool ranging_active = false;

// Optional: expose results later via getter
// static vl53l8cx_results_data_t last_results;
static vl53l8cx_results_data_t vl53_results;


VL53L8CX_Configuration *vl53_get_dev(void)
{
    return p_dev; // your existing global pointer
}


// ------------------------------------------------------------
// uint8_t vl53l8cx_init(VL53L8CX_Configuration *p_dev) {   // api.c +243

bool vl53l8cx_init_driver(void)
{
    uint8_t ret = VL53L8CX_STATUS_OK;
    memset(p_dev, 0, sizeof(dev));

    #ifdef VL53_SPI
    p_dev->platform.spi_port = VL53_SPI;
    p_dev->platform.pin_cs = VL53_PIN_CS;
    p_dev->platform.pin_int = VL53_PIN_INT;
    // p_dev->platform.pin_lpn = VL53_PIN_LPN;
    p_dev->platform.pin_miso = VL53_PIN_MISO;
    p_dev->platform.pin_mosi = VL53_PIN_MOSI;
    p_dev->platform.pin_sck = VL53_PIN_SCK;
    p_dev->platform.baudrate = VL53_BAUDRATE;   // 1 MHz for bring-up
 
    vl53_spi_init(&p_dev->platform);
    #else
    p_dev->platform.i2c_port = VL53_I2C_PORT;
    p_dev->platform.i2c_addr = VL53_I2C_ADDR;
    p_dev->platform.pin_cs = VL53_PIN_CS;
    p_dev->platform.pin_int = VL53_PIN_INT;
    p_dev->platform.pin_sda = VL53_PIN_SDA;
    p_dev->platform.pin_scl = VL53_PIN_SCL;
    p_dev->platform.baudrate = VL53_BAUDRATE;   // 1 MHz for bring-up
 
    printf("[VL53] drv call vl53_i2c_init addr:0x%x\n", p_dev->platform.i2c_addr);
    vl53_i2c_init(&p_dev->platform);
    #endif // VL53_SPI
 
    printf("[VL53] drv call vl53_gpio_init \n");
    vl53_gpio_init(&p_dev->platform);


    printf("[VL53] drv start vl53l8cx_init \n");
    ret = vl53l8cx_init(p_dev);
    if (ret != VL53L8CX_STATUS_OK) {
        printf("[VL53] Error init: %u\n", ret);
        return false;
    }

    #ifdef VL53_SPI
    printf("[VL53] SPI1 baud: %u\n", vl53l8cx_spi_get_baudrate(&p_dev->platform));
    #endif // VL53_SPI

    return true;
}

bool vl53l8cx_start_drv_ranging(void)
{
    if (vl53l8cx_start_ranging(p_dev) != VL53L8CX_STATUS_OK)
        return false;

    ranging_active = true;
    return true;
}


static void vl53_print_center_zone(void)
{
    // Center index for 8x8 is zone 27 or 28 depending on orientation
    // ST defines logical ordering; start with index 27
    int16_t d = vl53_results.distance_mm[27];
    uint8_t s = vl53_results.target_status[27];

    printf("[VL53] center: %d mm (status=%u)\n", d, s);
}


void vl53l8cx_loop(void)
{
    uint8_t ready = 0;
    static absolute_time_t last_time, current_time;

    if (!ranging_active)
        return;

    // Fast GPIO check first (cheap)
    if (!vl53_platform_int_asserted(&(p_dev->platform)))
        return;

    // Confirm with sensor
    if (vl53l8cx_check_data_ready(p_dev, &ready) != VL53L8CX_STATUS_OK || !ready)
        return;


    // Blocking SPI burst read (expected)
    if (vl53l8cx_get_ranging_data(p_dev, &vl53_results) != VL53L8CX_STATUS_OK)
        return;

    // Clear interrupt inside sensor - done automatically above when get ranging data
    // vl53l8cx_clear_interrupt(&dev);

    // 1sec = 1000000
    current_time = get_absolute_time();
    if (absolute_time_diff_us(last_time, current_time) > 300000) {
        last_time = current_time;
        vl53_print_center_zone();
    }

    // vl53_print_center_zone();

        // if (vl53_results.target_status[27] == 5)
        //     use distance
        // else
        //     ignore

    // TODO:
    // - copy results to application buffer
    // - signal new frame available
}





    // ─────────────────────────────────────────────────────────────
    // VL53L8CX – SPI1 (dedicated, independent from W6100 SPI0)
    // ─────────────────────────────────────────────────────────────
// #define VL53_SPI           spi1

// #define VL53_PIN_SCK       8   // GP8  -> SCL / CLK
// #define VL53_PIN_MOSI      9   // GP9  -> SDA / MOSI
// #define VL53_PIN_MISO      7   // GP7  -> MISO
// #define VL53_PIN_CS        10  // GP10 -> NCS (active LOW)

// #define VL53_PIN_INT       6   // GP6  -> INT (active LOW, open-drain)
// #define VL53_PIN_LPN       11  // GP11 -> LPn (enable / reset control)

// // Fixed wiring
// #define VL53_PIN_SPI_I2C_N -1  // tied to 3V3 on PCB


// #include "vl53l8cx_drv.h"
// // #include "vl53l8cx_api.h"   // ST ULD
// #include "vl53l8cx_platform.h"
// #include "hardware/gpio.h"
// #include "hardware/spi.h"
// #include "pico/time.h"


// // Conservative bring-up clock (increase later if needed)
// #define VL53_SPI_BAUDRATE  (1 * 1000 * 1000) // 1 MHz

// extern vl53l8cx_dev_t vl53_dev;

// #define VL53_INT_PIN  VL53_PIN_INT

// static bool ranging_started = false;


// static inline void vl53_gpio_init(void)
// {
//     // ── Chip Select ─────────────────────────────
//     gpio_init(VL53_PIN_CS);
//     gpio_set_dir(VL53_PIN_CS, GPIO_OUT);
//     gpio_put(VL53_PIN_CS, 1);   // deselect (active LOW)

//     // ── LPn (communication enable) ───────────────
//     gpio_init(VL53_PIN_LPN);
//     gpio_set_dir(VL53_PIN_LPN, GPIO_OUT);
//     gpio_put(VL53_PIN_LPN, 1);  // enable interface

//     // ── INT (open-drain, active LOW) ─────────────
//     gpio_init(VL53_PIN_INT);
//     gpio_set_dir(VL53_PIN_INT, GPIO_IN);
//     gpio_pull_up(VL53_PIN_INT); // required for open-drain
// }

// static inline void vl53_spi_init(void)
// {
//     // ── SPI peripheral ───────────────────────────
//     spi_init(VL53_SPI, VL53_SPI_BAUDRATE);

//     spi_set_format(
//         VL53_SPI,
//         8,              // bits per transfer
//         SPI_CPOL_0,     // CPOL = 0
//         SPI_CPHA_0,     // CPHA = 0  (SPI mode 0)
//         SPI_MSB_FIRST
//     );

//     // ── SPI pins (AF selection) ──────────────────
//     gpio_set_function(VL53_PIN_SCK,  GPIO_FUNC_SPI);
//     gpio_set_function(VL53_PIN_MOSI, GPIO_FUNC_SPI);
//     gpio_set_function(VL53_PIN_MISO, GPIO_FUNC_SPI);
// }

// // Public init entry point
// void vl53l8cx_init_driver(void)
// // void vl53l8cx_platform_init(void)
// {
//     vl53_gpio_init();
//     vl53_spi_init();

//     // Sensor requires LPn HIGH before communication
//     sleep_ms(2);
// }

// /**
//  * Interrupt usage (recommended)
//  * INT is open-drain, active LOW.
//  */
// bool vl53_data_ready(void)
// {
//     return gpio_get(VL53_PIN_INT) == 0;
// }

// /*
// You can later attach a GPIO IRQ
// gpio_set_irq_enabled(
//     VL53_PIN_INT,
//     GPIO_IRQ_EDGE_FALL,
//     true
// );
// */


// bool vl53l8cx_process(void)
// {
//     static vl53l8cx_results_data_t results;
//     uint8_t ready = 0;

//     if (!ranging_started)
//         return false;

//     // Fast path: check INT pin (active LOW)
//     if (gpio_get(VL53_INT_PIN) != 0)
//         return false;

//     // Confirm with device
//     if (vl53l8cx_check_data_ready(&vl53_dev, &ready) != VL53L8CX_STATUS_OK)
//         return false;

//     if (!ready)
//         return false;

//     // Read results (blocking SPI burst)
//     if (vl53l8cx_get_ranging_data(&vl53_dev, &results) != VL53L8CX_STATUS_OK)
//         return false;

//     // Clear interrupt inside sensor
//     vl53l8cx_clear_interrupt(&vl53_dev);

//     // TODO: user processing hook
//     // process_results(&results);

//     return true;
// }


