
#pragma once
#include <stdint.h>

void vl53_diag_print_gpio(int sn);
void vl53_diag_print_spi1(int sn);
void vl53_diag_probe_bus(int sn);   // optional - calls ST API
#ifdef VL53_SPI
void vl53_diag_raw_spi_test(int sn);
#endif // VL53_SPI

void vl53_diag_cs_active(int sn);
void vl53_diag_cs_inactive(int sn);
void vl53_diag_read_one(int sn);
void vl53_diag_start_ranging(int sn);




