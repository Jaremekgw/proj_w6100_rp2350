/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

 /*
 1. Responsibilities (clear and strict)
    vl53l8cx_platform.*
        ONLY hardware access
        - SPI read/write
        - CS control
        - delay
        - INT pin read
        No sensor logic. No state machine.

    vl53l8cx_drv.*
        Sensor logic
        - init
        - start ranging
        - loop-style read (vl53l8cx_loop)
        - owns ST device handle
 */

#include <stdio.h>

#include "vl53l8cx_platform.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/time.h"

// ===== Hardware configuration (single source of truth) =====

// SPI protocol
#define VL53_SPI_READ_BIT   0x8000

// #define CS_LOW()    gpio_put(VL53_PIN_CS, 0) // gpio_put(p_platform->pin_cs, 0)
// #define CS_HIGH()   gpio_put(VL53_PIN_CS, 1) // gpio_put(p_platform->pin_cs, 1)

static inline void vl53_cs_select(uint8_t pin_cs)
{
    gpio_set_dir(pin_cs, GPIO_OUT);
    gpio_put(pin_cs, 0);   // actively drive LOW
}

static inline void vl53_cs_deselect(uint8_t pin_cs)
{
    gpio_set_dir(pin_cs, GPIO_IN);  // Hi-Z, pulled up by sensor to 1.8 V
}

void vl53_cs_set_active(vl53l8cx_platform_t *p_platform)
{
    vl53_cs_select(p_platform->pin_cs);
}
void vl53_cs_set_inactive(vl53l8cx_platform_t *p_platform)
{
    vl53_cs_deselect(p_platform->pin_cs);
}

#ifdef VL53_SPI
void vl53_spi_init(vl53l8cx_platform_t *p_platform)
{
    // Initialize SPI1 at conservative speed first
    spi_init(p_platform->spi_port, p_platform->spi_baudrate * 1000 * 1000); // 1 MHz bring-up
    // spi_init(SPI_PORT, SPI_CLK * 1000 * 1000);

    spi_set_format(
        p_platform->spi_port,
        8,                  // bits
        SPI_CPOL_0,         // CPOL
        SPI_CPHA_0,         // CPHA  (SPI mode 0)
        SPI_MSB_FIRST
    );

    // Assign SPI function to GPIOs
    gpio_set_function(p_platform->pin_sck, GPIO_FUNC_SPI);    // VL53_PIN_SCK
    gpio_set_function(p_platform->pin_mosi, GPIO_FUNC_SPI);     // VL53_PIN_MOSI
    gpio_set_function(p_platform->pin_miso, GPIO_FUNC_SPI);     // VL53_PIN_MISO

    // Optional: ensure no internal pulls fight the level shifter
    gpio_disable_pulls(p_platform->pin_sck);
    gpio_disable_pulls(p_platform->pin_mosi);
    // MISO is input on MCU side; usually safe either way, but disable pulls to avoid biasing
    gpio_disable_pulls(p_platform->pin_miso);

    // Make sure CS is deselected before any bus traffic
    // vl53_cs_deselect_od(p_platform->pin_cs);

    sleep_ms(2);
}
#else
static void i2c_bus_recover(uint sda_gpio, uint scl_gpio) {
    // Temporarily take control as GPIO
    gpio_set_function(sda_gpio, GPIO_FUNC_SIO);
    gpio_set_function(scl_gpio, GPIO_FUNC_SIO);

    gpio_set_dir(sda_gpio, GPIO_IN);
    gpio_pull_up(sda_gpio);

    gpio_set_dir(scl_gpio, GPIO_OUT);
    gpio_put(scl_gpio, 1);
    sleep_us(5);

    // If SDA is low, clock SCL up to 9 pulses to release it
    for (int i = 0; i < 9 && gpio_get(sda_gpio) == 0; i++) {
        gpio_put(scl_gpio, 0);
        sleep_us(5);
        gpio_put(scl_gpio, 1);
        sleep_us(5);
    }

    // Generate a STOP: SDA low -> SCL high -> SDA high
    // (Only if we can pull SDA low via output; if not, skip)
    gpio_set_dir(sda_gpio, GPIO_OUT);
    gpio_put(sda_gpio, 0);
    sleep_us(5);
    gpio_put(scl_gpio, 1);
    sleep_us(5);
    gpio_put(sda_gpio, 1);
    sleep_us(5);

    // Release lines
    gpio_set_dir(sda_gpio, GPIO_IN);
    gpio_pull_up(sda_gpio);

    // Restore I2C function will be done by caller
}

void vl53_i2c_init(vl53l8cx_platform_t *p_platform)
{
    // Initialize SPI1 at conservative speed first
    i2c_init(p_platform->i2c_port, p_platform->baudrate * 100 * 1000); // 1 MHz bring-up
    // spi_init(SPI_PORT, SPI_CLK * 1000 * 1000);

    // spi_set_format(
    //     p_platform->i2c_port,
    //     8,                  // bits
    //     SPI_CPOL_0,         // CPOL
    //     SPI_CPHA_0,         // CPHA  (SPI mode 0)
    //     SPI_MSB_FIRST
    // );

    i2c_bus_recover(p_platform->pin_sda, p_platform->pin_scl);
    
    // Assign SPI function to GPIOs
    gpio_set_function(p_platform->pin_scl, GPIO_FUNC_I2C);    // VL53_PIN_SCL
    gpio_set_function(p_platform->pin_sda, GPIO_FUNC_I2C);     // VL53_PIN_SDA

    // Optional: ensure no internal pulls fight the level shifter
    gpio_disable_pulls(p_platform->pin_scl);
    gpio_disable_pulls(p_platform->pin_sda);

    sleep_ms(2);
}
#endif // VL53_SPI

void vl53_gpio_init(vl53l8cx_platform_t *p_platform)
{
    // Chip select is manual
    /* ---------------------------------------------------------
     * Chip Select (NCS)
     * Emulated open-drain, active LOW
     * --------------------------------------------------------- */
    gpio_init(p_platform->pin_cs);  // VL53_PIN_CS
    /* Start deselected:
     * - input = Hi-Z
     * - sensor pull-up brings it to IOVDD (~1.8 V)
     */
    gpio_set_dir(p_platform->pin_cs, GPIO_IN);
    gpio_put(p_platform->pin_cs, 0);   // deselect VL53_PIN_CS
    // gpio_pull_up(p_platform->pin_cs);

    // // LPn must be high before init
    // gpio_init(p_platform->pin_lpn);
    // gpio_set_dir(p_platform->pin_lpn, GPIO_OUT);
    // gpio_put(p_platform->pin_lpn, 1);   // ENABLE communications
    // sleep_ms(2);

    // Enable pull-up on INT pin
    gpio_init(p_platform->pin_int);
    gpio_set_dir(p_platform->pin_int, GPIO_IN);
    // gpio_pull_up(p_platform->pin_int);
}


static const char *gpio_func_name(gpio_function_t f) {
    switch (f) {
        case GPIO_FUNC_HSTX:  return "HSTX";
        case GPIO_FUNC_SPI:  return "SPI";
        case GPIO_FUNC_UART: return "UART";
        case GPIO_FUNC_I2C:  return "I2C";
        case GPIO_FUNC_PWM:  return "PWM";
        case GPIO_FUNC_SIO:  return "SIO";
        case GPIO_FUNC_PIO0: return "PIO0";
        case GPIO_FUNC_PIO1: return "PIO1";
        case GPIO_FUNC_PIO2: return "PIO2";
        case GPIO_FUNC_GPCK: return "GPCK";
        case GPIO_FUNC_USB: return "USB";
        case GPIO_FUNC_UART_AUX: return "AUX";
        case GPIO_FUNC_NULL: return "NULL1";
        default:             return "NULL";
    }
}
static void print_one_gpio(uint pin, const char *name)
{
    // if (pin < 0) {
    //     printf("  %-8s: (not connected)\r\n", name);
    //     return;
    // }

    bool level = gpio_get(pin);
    bool is_out = gpio_is_dir_out(pin);
    gpio_function_t fn = gpio_get_function(pin);

    // Pulls (SDK does not expose direct readback of pull config; we report level/dir/func only)
    printf("  %-8s: GP%-2d  level=%d  dir=%s  func=%s\r\n",
          name, pin, (int)level, is_out ? "OUT" : "IN ", gpio_func_name(fn));
}


#ifdef VL53_SPI
/* -------------------------------------------------------------------------- */
/* Internal SPI helpers (your preferred style, PRIVATE)                        */
/* -------------------------------------------------------------------------- */

static inline void vl53_spi_write(
    vl53l8cx_platform_t *p_platform,
    uint16_t reg,
    const uint8_t *buf,
    uint32_t len
)
{
    uint8_t hdr[2] = {
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xFF)
    };

    // gpio_put(p_platform->pin_cs, 0); // CS_LOW();
    vl53_cs_select(p_platform->pin_cs);
    spi_write_blocking(p_platform->spi_port, hdr, 2);
    spi_write_blocking(p_platform->spi_port, buf, len);
    // gpio_put(p_platform->pin_cs, 1);    // CS_HIGH();
    vl53_cs_deselect(p_platform->pin_cs);
}

static inline void vl53_spi_read(
    vl53l8cx_platform_t *p_platform,
    uint16_t reg,
    uint8_t *buf,
    uint32_t len
)
{
    uint16_t cmd = reg | VL53_SPI_READ_BIT;

    uint8_t hdr[2] = {
        (uint8_t)(cmd >> 8),
        (uint8_t)(cmd & 0xFF)
    };

    vl53_cs_select(p_platform->pin_cs);
    spi_write_blocking(p_platform->spi_port, hdr, 2);
    spi_read_blocking(p_platform->spi_port, 0x00, buf, len);
    vl53_cs_deselect(p_platform->pin_cs);
}
#else
#define I2C_SUCCESS 0
#define I2C_FAILED 1
#define I2C_BUFFER_EXCEEDED 2

// #define I2C_DEVICE i2c1
#define MAX_I2C_BUFFER 0x8100


int8_t i2c_write_register(VL53L8CX_Platform *p_platform, uint16_t index, uint8_t * values, uint32_t count){
    // i2c_inst_t *i2c, char adresse_7_bits,
    int stat;
    uint8_t adresse_7_bits = p_platform->i2c_addr >> 1;
    uint8_t buffer[MAX_I2C_BUFFER];
    absolute_time_t timeout_time;

    if(count > MAX_I2C_BUFFER - 2){
            return I2C_BUFFER_EXCEEDED;
    }

    buffer[0] = (uint8_t)((index >> 8) & 0xFF);
    buffer[1] = (uint8_t)(index & 0xFF);

    for (uint32_t i=0; i<count; i++) {
        buffer[2+i] = values[i];
    }

    // Define timeout - now + 1s.
    timeout_time = time_us_64() + 1000000;

    print_one_gpio(p_platform->pin_sda, "SDA");
    print_one_gpio(p_platform->pin_scl, "SCL");
    print_one_gpio(p_platform->pin_cs, "CS ");
    print_one_gpio(p_platform->pin_int, "INT");
    printf("[VL53] [platform] i2c write addr=0x%x buf0=0x%x buf1=0x%x buf2=0x%x\n", adresse_7_bits<<1, buffer[0], buffer[1], buffer[2]);
    // statu = i2c_write_blocking (i2c, adresse_7_bits, hdr, 2, 0);
    stat = i2c_write_blocking_until (p_platform->i2c_port, adresse_7_bits, buffer, 2 + count, 0, timeout_time);

    if(stat == PICO_ERROR_GENERIC){
        printf("Error: i2c write generic, addr:0x%x\n", adresse_7_bits << 1);
        return I2C_FAILED;
    } else if(stat == PICO_ERROR_TIMEOUT){
        printf("Error: i2c write timeout, addr:0x%x\n", adresse_7_bits << 1);
        return I2C_FAILED;
    }

    return I2C_SUCCESS;
}

/// @brief Blocking function allowing to write a register on an I2C device
/// @param address_7_bits
/// @param index : register to write
/// @param values : values to write
/// @param count : number of byte to send
/// @return 0: Success, -1 or -2: Failed
int8_t i2c_read_register(VL53L8CX_Platform *p_platform, uint16_t index, uint8_t *pdata, uint32_t count){
    // i2c_inst_t *i2c, char adresse_7_bits,
    // uint16_t cmd = index | VL53_SPI_READ_BIT; pytanie czy dla spi to jest prawidlowe?, w SPI nie ma bitu read
    // uint8_t hdr[2] = {
    //     (uint8_t)(cmd >> 8),
    //     (uint8_t)(cmd & 0xFF)
    // };

    int statu;
    uint8_t adresse_7_bits = p_platform->i2c_addr >> 1;
    //    uint8_t buffer[MAX_I2C_BUFFER];
    //uint8_t index_to_unint8[2];
    uint8_t hdr[2] = {
        (uint8_t)((index >> 8) & 0xFF),
        (uint8_t)(index & 0xFF)
    };
        // index_to_unint8[0] =  (index >> 8) & 0xFF;
        // index_to_unint8[1] =  index & 0xFF;

    statu = i2c_write_blocking (p_platform->i2c_port, adresse_7_bits, hdr, 2, 0);
    if(statu == PICO_ERROR_GENERIC){
        // printf("I2C - Write - Envoi registre Echec %x\n", adresse_7_bits);
        return I2C_FAILED;
    }

    statu = i2c_read_blocking (p_platform->i2c_port, adresse_7_bits, pdata, count, 0);
    if(statu == PICO_ERROR_GENERIC){
        // printf("I2C - Lecture registre Echec\n");
        return I2C_FAILED;
    }

    return I2C_SUCCESS;
}


#endif // VL53_SPI

/* -------------------------------------------------------------------------- */
/* ST-mandated platform API                                                    */
/* -------------------------------------------------------------------------- */

uint8_t RdByte(
    VL53L8CX_Platform *p_platform,
    uint16_t RegisterAdress,
    uint8_t *p_value
)
{
    // (void)p_platform;
    #ifdef VL53_SPI
    vl53_spi_read(p_platform, RegisterAdress, p_value, 1);
    return 0;
    #else
    int8_t status;
    vl53_cs_set_active(p_platform);
    status = i2c_read_register(p_platform, RegisterAdress, p_value, 1);
    vl53_cs_set_inactive(p_platform);

    if (status < 0) {
        return 255; // Custom error code for failure
    }
    return 0; // Success
    #endif // VL53_SPI
}

uint8_t WrByte(
    VL53L8CX_Platform *p_platform,
    uint16_t RegisterAdress,
    uint8_t value
)
{
    // (void)p_platform;
    #ifdef VL53_SPI
    vl53_spi_write(p_platform, RegisterAdress, &value, 1);
    return 0;
    #else
    int8_t status;
    vl53_cs_set_active(p_platform);
    status = i2c_write_register(p_platform, RegisterAdress, &value, 1);
    vl53_cs_set_inactive(p_platform);
    if (status < 0) {
        return 255; // Custom error code for failure
    }
    return 0; // Success
    #endif // VL53_SPI
}


uint8_t RdMulti(
    VL53L8CX_Platform *p_platform,
    uint16_t RegisterAdress,
    uint8_t *p_values,
    uint32_t size
)
{
    (void)p_platform;
    #ifdef VL53_SPI
    vl53_spi_read(p_platform, RegisterAdress, p_values, size);
    return 0;
    #else
    int8_t status;
    vl53_cs_set_active(p_platform);
    status = i2c_read_register(p_platform, RegisterAdress, p_values, size);
    vl53_cs_set_inactive(p_platform);
    if (status < 0) {
        return 255; // Custom error code for failure
    }
    return 0; // Success
    #endif // VL53_SPI
}

uint8_t WrMulti(
    VL53L8CX_Platform *p_platform,
    uint16_t RegisterAdress,
    uint8_t *p_values,
    uint32_t size
)
{
    (void)p_platform;
    #ifdef VL53_SPI
    vl53_spi_write(p_platform, RegisterAdress, p_values, size);
    return 0;
    #else
    int8_t status;
    vl53_cs_set_active(p_platform);
    status = i2c_write_register(p_platform, RegisterAdress, p_values, size);
    vl53_cs_set_inactive(p_platform);
    if (status < 0) {
        return 255; // Custom error code for failure
    }
    return 0; // Success
    #endif // VL53_SPI
}


uint8_t WaitMs(
    VL53L8CX_Platform *p_platform,
    uint32_t TimeMs
)
{
    (void)p_platform;
    sleep_ms(TimeMs);
    return 0;
}

void SwapBuffer(
    uint8_t *buffer,
    uint16_t size
)
{
    uint32_t *p = (uint32_t *)buffer;

    for (uint16_t i = 0; i < size; i += 4) {
        uint32_t v = *p;
        *p++ = __builtin_bswap32(v);
    }
}

#ifdef VL53_SPI
uint vl53l8cx_spi_get_baudrate(vl53l8cx_platform_t *p_platform) {
    return spi_get_baudrate(p_platform->spi_port);
}
#endif // VL53_SPI

















// -----------------------------------------------------------

// int32_t vl53_platform_write(
//     vl53l8cx_platform_t *p_platform,
//     uint16_t reg,
//     const uint8_t *buf,
//     size_t len
// )
// {
//     uint8_t hdr[2] = {
//         (uint8_t)(reg >> 8),
//         (uint8_t)(reg & 0xFF)
//     };

//     gpio_put(p_platform->pin_cs, 0);    // CS_LOW();
//     spi_write_blocking(p_platform->spi_port, hdr, 2);
//     spi_write_blocking(p_platform->spi_port, buf, len);
//     gpio_put(p_platform->pin_cs, 1);    // CS_HIGH();

//     return 0;
// }

// int32_t vl53_platform_read(
//     vl53l8cx_platform_t *p_platform,
//     uint16_t reg,
//     uint8_t *buf,
//     size_t len
// )
// {
//     uint16_t cmd = reg | VL53_SPI_READ_BIT;

//     uint8_t hdr[2] = {
//         (uint8_t)(cmd >> 8),
//         (uint8_t)(cmd & 0xFF)
//     };

//     gpio_put(p_platform->pin_cs, 0);    // CS_LOW();
//     spi_write_blocking(p_platform->spi_port, hdr, 2);
//     spi_read_blocking(p_platform->spi_port, 0x00, buf, len);
//     gpio_put(p_platform->pin_cs, 1);    // CS_HIGH();

//     return 0;
// }

// void vl53_platform_delay_ms(uint32_t ms)
// {
//     sleep_ms(ms);
// }

// bool vl53_platform_int_is_active(void)
bool vl53_platform_int_asserted(vl53l8cx_platform_t *p_platform)
{
    // INT is open-drain, active LOW
    return gpio_get(p_platform->pin_int) == 0;
}



