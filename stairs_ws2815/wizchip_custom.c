/**
    Copyright (c) 2022 WIZnet Co.,Ltd

    SPDX-License-Identifier: BSD-3-Clause

    This file is based on wizchip_spi.c from WIZnet ioLibrary_Driver,
    and support only (_WIZCHIP_ == W6100) case with SPI interface.
*/

#include <stdio.h>

#include "port_common.h"

#include "wizchip_conf.h"
#include "wizchip_spi.h"
#include "board_list.h"


#include "wizchip_qspi_pio.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/critical_section.h"
#include "hardware/dma.h"


static inline void wizchip_select(void) {
    gpio_put(PIN_CS, 0);
}

static inline void wizchip_deselect(void) {
    gpio_put(PIN_CS, 1);

}

static uint8_t wizchip_read(void) {
    uint8_t rx_data = 0;
    uint8_t tx_data = 0xFF;

    spi_read_blocking(SPI_PORT, tx_data, &rx_data, 1);

    return rx_data;
}

static void wizchip_write(uint8_t tx_data) {
    spi_write_blocking(SPI_PORT, &tx_data, 1);
}

static void wizchip_read_buf(uint8_t* rx_data, datasize_t len) {
    uint8_t tx_data = 0xFF;

    spi_read_blocking(SPI_PORT, tx_data, rx_data, len);
}

static void wizchip_write_buf(uint8_t* tx_data, datasize_t len) {
    spi_write_blocking(SPI_PORT, tx_data, len);
}

void wizchip_init_nonblocking(void) {

    /* Deselect the FLASH : chip select high */
    wizchip_deselect();
    /* CS function register */
    reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
    /* SPI function register */
    reg_wizchip_spi_cbfunc(wizchip_read, wizchip_write, wizchip_read_buf, wizchip_write_buf);

    // Manually perform what wizchip_initialize() does
    //wizchip_deselect(); // <-- your own function, not the static one
    //reg_wizchip_cs_cbfunc(my_chip_select, my_chip_deselect);
    //reg_wizchip_spi_cbfunc(my_spi_read, my_spi_write,
    //                       my_spi_read_buf, my_spi_write_buf);

    uint8_t memsize[2][8] = {
        {2,2,2,2,2,2,2,2},
        {2,2,2,2,2,2,2,2}
    };

    printf(" Start init W6x00.\n");
    if (ctlwizchip(CW_INIT_WIZCHIP, memsize) == -1) {
        printf("wizchip init failed\n");
        return;
    }
    printf(" W6x00 initialized, check phy link.\n");

    // Non-blocking Check PHY link status, so just print status once
    uint8_t temp;
    if (ctlwizchip(CW_GET_PHYLINK, &temp) == 0) {
        printf("PHY link %s\n", temp == PHY_LINK_ON ? "ON" : "OFF");
    }
}

