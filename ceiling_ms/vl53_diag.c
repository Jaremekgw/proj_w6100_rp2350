
#include "vl53_diag.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/clocks.h"
#include "hardware/regs/spi.h"
#include "hardware/structs/spi.h"
#include "pico/time.h"
#include "vl53l8cx_drv.h"
#include "vl53l8cx_api.h"
// #include "telnet.h"
#include "tcp_cli.h"
// extern void cli_flush(int sn, const char *msg);

// --- Must match your wiring ---
// #define VL53_SPI            spi1

// #define VL53_PIN_INT        8   // 6
// #define VL53_PIN_MISO       12  // 7
// #define VL53_PIN_SCK        10  // 8
// #define VL53_PIN_MOSI       11  // 9
// #define VL53_PIN_CS         9   //10
// // #define VL53_PIN_LPN        7   // 11 - not connected

// Optional: if you tie to 3V3 on board, this pin might not exist in GPIO.
// #define VL53_PIN_SPI_I2C_N   (-1)

// Optional ST driver instance if you want probe
// If you have p_dev global in your driver, expose a getter instead (recommended).
#include "vl53l8cx_api.h"
extern VL53L8CX_Configuration *vl53_get_dev(void); // implement in your driver or adjust

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------


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

static void sendf(uint8_t sn, const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    cli_flush(sn, buf);
}

static void dump_one_gpio(uint8_t sn, uint pin, const char *name)
{
    if (pin > 29) {
        sendf(sn, "  %-8s: (not connected)\r\n", name);
        return;
    }

    bool level = gpio_get(pin);
    bool is_out = gpio_is_dir_out(pin);
    gpio_function_t fn = gpio_get_function(pin);

    // Pulls (SDK does not expose direct readback of pull config; we report level/dir/func only)
    sendf(sn,
          "  %-8s: GP%-2d  level=%d  dir=%s  func=%s\r\n",
          name, pin, (int)level, is_out ? "OUT" : "IN ", gpio_func_name(fn));
}

// --------------------------------------------------------------------------
// Public diagnostics
// --------------------------------------------------------------------------

void vl53_diag_print_gpio(uint8_t sn)
{
    cli_flush(sn, "[VL53] GPIO status\r\n");
    dump_one_gpio(sn, VL53_PIN_CS,   "CS");
    dump_one_gpio(sn, VL53_PIN_INT,  "INT");
    #ifdef VL53_SPI
    // dump_one_gpio(sn, VL53_PIN_LPN,  "LPn");
    dump_one_gpio(sn, VL53_PIN_SCK,  "SCK");
    dump_one_gpio(sn, VL53_PIN_MOSI, "MOSI");
    dump_one_gpio(sn, VL53_PIN_MISO, "MISO");
    #else
    dump_one_gpio(sn, VL53_PIN_SCL,  "SCL");
    dump_one_gpio(sn, VL53_PIN_SDA, "SDA");
    #endif // VL53_SPI

    // VL53_PIN_SPI_I2C_N is not used, directly tied to 3V3
    // dump_one_gpio(sn, VL53_PIN_SPI_I2C_N, "SPI_I2C_N");
    cli_flush(sn, "\r\n");
}

#ifdef VL53_SPI
void vl53_diag_print_spi1(uint8_t sn)
{
    cli_flush(sn, "[VL53] SPI1 status\r\n");

    // SPI hardware registers (RP2040/RP2350 compatible naming in Pico SDK)
    spi_hw_t *hw = spi_get_hw(VL53_SPI);

    // CR0: data size / CPOL / CPHA etc
    uint32_t cr0 = hw->cr0;
    uint32_t cr1 = hw->cr1;
    uint32_t cpsr = hw->cpsr;

    int data_bits = (int)((cr0 & SPI_SSPCR0_DSS_BITS) >> SPI_SSPCR0_DSS_LSB) + 1;
    int cpol = (cr0 & SPI_SSPCR0_SPO_BITS) ? 1 : 0;
    int cpha = (cr0 & SPI_SSPCR0_SPH_BITS) ? 1 : 0;

    // Enabled bit in CR1
    bool enabled = (cr1 & SPI_SSPCR1_SSE_BITS) != 0;

    sendf(sn, "  enabled=%d\r\n", (int)enabled);
    sendf(sn, "  format: bits=%d  CPOL=%d  CPHA=%d  order=MSB\r\n", data_bits, cpol, cpha);

    // Clock divider info. Pico SDK sets: SCK = peri_clk / (cpsr * (1 + scr))
    int scr = (int)((cr0 & SPI_SSPCR0_SCR_BITS) >> SPI_SSPCR0_SCR_LSB);
    int prescale = (int)(cpsr & SPI_SSPCPSR_CPSDVSR_BITS);

    uint32_t peri_hz = clock_get_hz(clk_peri);
    uint32_t sck_hz = 0;
    if (prescale != 0) {
        sck_hz = peri_hz / (prescale * (1u + (uint32_t)scr));
    }

    sendf(sn, "  clk_peri=%lu Hz\r\n", (unsigned long)peri_hz);
    sendf(sn, "  cpsr(prescale)=%d  scr=%d  => sck≈%lu Hz\r\n",
          prescale, scr, (unsigned long)sck_hz);

    // Pin function sanity check
    cli_flush(sn, "  pin mux:\r\n");
    dump_one_gpio(sn, VL53_PIN_SCK,  "SCK");
    dump_one_gpio(sn, VL53_PIN_MOSI, "MOSI");
    dump_one_gpio(sn, VL53_PIN_MISO, "MISO");
    dump_one_gpio(sn, VL53_PIN_CS,   "CS");

    cli_flush(sn, "\r\n");
}
#endif // VL53_SPI

void vl53_diag_probe_bus(uint8_t sn)
{
    cli_flush(sn, "[VL53] Bus probe\r\n");

    VL53L8CX_Configuration *dev = vl53_get_dev();
    if (!dev) {
        cli_flush(sn, "  ERROR: vl53 device handle is NULL\r\n\r\n");
        return;
    }

    // Light probe: check data-ready (does not require printing a full frame)
    uint8_t ready = 0;
    uint8_t st = vl53l8cx_check_data_ready(dev, &ready);

    sendf(sn, "  vl53l8cx_check_data_ready: status=%u ready=%u\r\n\r\n", st, ready);
}


void vl53_diag_raw_spi_test(uint8_t sn)
{
    uint8_t val = 0xAA;
    uint8_t rd  = 0x00;
    uint8_t st;
    VL53L8CX_Configuration *p_dev = vl53_get_dev();

    cli_flush(sn, "[VL53] RAW SPI test\r\n");

    // Write 0xAA to register 0x7FFF
    st = WrByte(&p_dev->platform, 0x7FFF, val);
    printf("WrByte(0x7FFF, 0x%02X) status=%u\r\n", val, st);

    // Read back
    st = RdByte(&p_dev->platform, 0x7FFF, &rd);
    printf("RdByte(0x7FFF) status=%u value=0x%02X\r\n", st, rd);

    cli_flush(sn, "\r\n");
}




void vl53_diag_cs_active(uint8_t sn)
{
    cli_flush(sn, "[VL53] CS active\r\n");

    VL53L8CX_Configuration *dev = vl53_get_dev();
    if (!dev) {
        cli_flush(sn, "  ERROR: vl53 device handle is NULL\r\n\r\n");
        return;
    }

    vl53_cs_set_active(&dev->platform);

    cli_flush(sn, "    GPIO status\r\n");
    dump_one_gpio(sn, VL53_PIN_CS,   "  CS");
    dump_one_gpio(sn, VL53_PIN_INT,  "  INT");
    #ifdef VL53_SPI
    dump_one_gpio(sn, VL53_PIN_SCK,  "  SCK");
    dump_one_gpio(sn, VL53_PIN_MOSI, "  MOSI");
    dump_one_gpio(sn, VL53_PIN_MISO, "  MISO");
    #else
    dump_one_gpio(sn, VL53_PIN_SCL,  "  SCL");
    dump_one_gpio(sn, VL53_PIN_SDA, "  SDA");
    #endif // VL53_SPI
    cli_flush(sn, "\r\n");
}

void vl53_diag_cs_inactive(uint8_t sn)
{
    cli_flush(sn, "[VL53] CS inactive\r\n");

    VL53L8CX_Configuration *dev = vl53_get_dev();
    if (!dev) {
        cli_flush(sn, "  ERROR: vl53 device handle is NULL\r\n\r\n");
        return;
    }

    vl53_cs_set_inactive(&dev->platform);

    cli_flush(sn, "    GPIO status\r\n");
    dump_one_gpio(sn, VL53_PIN_CS,   "  CS");
    dump_one_gpio(sn, VL53_PIN_INT,  "  INT");
    #ifdef VL53_SPI
    dump_one_gpio(sn, VL53_PIN_SCK,  "  SCK");
    dump_one_gpio(sn, VL53_PIN_MOSI, "  MOSI");
    dump_one_gpio(sn, VL53_PIN_MISO, "  MISO");
    #else
    dump_one_gpio(sn, VL53_PIN_SCL,  "  SCL");
    dump_one_gpio(sn, VL53_PIN_SDA, "  SDA");
    #endif // VL53_SPI
    cli_flush(sn, "\r\n");
}

// void vl53_diag_read_one(int sn)
// {
//     VL53L8CX_Configuration *dev = vl53_get_dev();
//     static VL53L8CX_ResultsData results;
//     uint8_t st;

//     st = vl53l8cx_get_ranging_data(dev, &results);
//     // Print a couple of values (don’t spam)
//     char msg[128];
//     snprintf(msg, sizeof(msg),
//              "get_ranging_data: status=%u stream=%lu d0=%d st0=%u\r\n",
//              st,
//              (unsigned long)results.streamcount,
//              (int)results.distance_mm[0],
//              (unsigned)results.target_status[0]);
//     cli_flush(sn, msg);
// }

void vl53_diag_read_one(uint8_t sn)
{
    VL53L8CX_Configuration *dev = vl53_get_dev();
    static VL53L8CX_ResultsData results;
    uint8_t st;

    uint8_t ready;
    // uint8_t st;

    st = vl53l8cx_check_data_ready(dev, &ready);
    if (st != 0) {
        // communication error
        printf("vl53l8cx_check_data_ready error: %u\n", st);
        return;
    }

    // if (!ready) {
    //     // no new frame yet
        printf("vl53 not ready:0x%02X\n", ready);
    //     return;
    // }

    // now it IS safe to read
    st = vl53l8cx_get_ranging_data(dev, &results);
    if (st != 0) {
        // read error
        printf("vl53l8cx_get_ranging_data error: %u\n", st);
        return;
    }

    char msg[160];
    snprintf(msg, sizeof(msg),
        "get_ranging_data: status=%u "
        "d0=%d st0=%u stream=%u\r\n",
        st,
        (int)results.distance_mm[0],
        (unsigned)results.target_status[0],
        (unsigned)dev->streamcount
    );

    cli_flush(sn, msg);
}

void vl53_diag_start_ranging(uint8_t sn)
{
    uint8_t st = 0xee;
    uint8_t v = 0xdd;

    cli_flush(sn, "[VL53] Start ranging\r\n");

    VL53L8CX_Configuration *dev = vl53_get_dev();
    if (!dev) {
        cli_flush(sn, "  ERROR: vl53 device handle is NULL\r\n\r\n");
        return;
    }
    st = vl53l8cx_start_ranging(dev);
    RdByte(&dev->platform, 0x0006, &v);

    printf("start_ranging status = %u\n", st);
    printf("SYS__MODE_START = 0x%02X\n", v);


}
