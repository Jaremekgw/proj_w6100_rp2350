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
#include "socket.h"


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

#if (_PHY_IO_MODE_ == _PHY_IO_MODE_MII_)
// For MDIO BMSR (Basic Mode Status Register)
#define BMSR_LINK_STATUS   (1 << 2)   // bit 2: Link up
#define BMSR_100FULL       (1 << 14)
#define BMSR_100HALF       (1 << 13)
#define BMSR_10FULL        (1 << 12)
#define BMSR_10HALF        (1 << 11)
#endif // _PHY_IO_MODE_MII_

// Helper function
void w6100_read_phy_status(void) {
    uint16_t reg;
    uint8_t link = 0, speed = 0, duplex = 0;

#if (_PHY_IO_MODE_ == _PHY_IO_MODE_PHYCR_)
    // --- Internal PHY Control mode (default on W6100-EVB-Pico2) ---
    reg = getPHYSR();  // 8-bit register read from W6100
    link   = (reg & PHYSR_LNK) ? 1 : 0;
    speed  = (reg & PHYSR_SPD) ? 100 : 10;
    duplex = (reg & PHYSR_DPX) ? 1 : 0;

#elif (_PHY_IO_MODE_ == _PHY_IO_MODE_MII_)
    // --- External PHY via MDIO interface ---
    uint16_t bmsr = wiz_mdio_read(PHYRAR_BMSR);
    link   = (bmsr & BMSR_LINK_STATUS) ? 1 : 0;

    // Optionally, read PHY specific status (vendor-specific)
    // For standard BMSR, speed/duplex bits might be missing, so:
    uint16_t bmcr = wiz_mdio_read(PHYRAR_BMCR);
    duplex = (bmcr & (1 << 8)) ? 1 : 0;     // BMCR_DUPLEX_MODE
    speed  = (bmcr & (1 << 13)) ? 100 : 10; // BMCR_SPEED_SELECT
#else
#warning "Unknown PHY IO mode"
#endif

    printf("PHY: link=%s, speed=%dM, duplex=%s\n",
           link ? "up" : "down",
           speed,
           duplex ? "full" : "half");
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
        {2,2,2,2,2,2,2,2},      // TX sockets
        {2,2,2,2,2,2,2,2}       // RX sockets
    };

    printf(" Start init W6x00.\n");
    if (ctlwizchip(CW_INIT_WIZCHIP, memsize) == -1) {
        printf("wizchip init failed\n");
        return;
    }
    printf(" W6x00 initialized, check phy link.\n");

    // Non-blocking Check PHY link status, so just print status once
    // uint8_t temp;
    // if (ctlwizchip(CW_GET_PHYLINK, &temp) == 0) {
    //     printf("PHY link %s\n", temp == PHY_LINK_ON ? "ON" : "OFF");
    // }
    w6100_read_phy_status();

    for (SOCKET sn = 0; sn < _WIZCHIP_SOCK_NUM_; sn++) {
        close(sn);                  // Force close all sockets
        setSn_IR((uint32_t)sn, 0xFF);         // Clear any pending interrupts
    }
}

/* 
void check_phy_link_W6x00(void) {
    uint8_t temp;
    if (ctlwizchip(CW_GET_PHYLINK, &temp) == 0) {
        printf("PHY link %s\n", temp == PHY_LINK_ON ? "ON" : "OFF");
    }
    // CW_GET_PHYCONF can be used to get current PHY configuration if needed
    // CW_GET_PHYSTATUS can be used to get current PHY status if needed
    // ctlwizchip(CW_GET_PHYSTATUS, &temp);
}
 */



