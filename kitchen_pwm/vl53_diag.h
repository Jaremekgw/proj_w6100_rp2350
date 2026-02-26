
#pragma once
#include <stdint.h>

void vl53_diag_print_gpio(uint8_t sn);
void vl53_diag_print_spi1(uint8_t sn);
void vl53_diag_probe_bus(uint8_t sn);   // optional - calls ST API
#ifdef VL53_SPI
void vl53_diag_raw_spi_test(uint8_t sn);
#endif // VL53_SPI

void vl53_diag_cs_active(uint8_t sn);
void vl53_diag_cs_inactive(uint8_t sn);
void vl53_diag_read_one(uint8_t sn);
void vl53_diag_start_ranging(uint8_t sn);


