/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// #include <stdio.h>
// #include <stdlib.h>
#include "pico/stdlib.h"
#include "partition.h"
#include "hardware/flash.h"
#include "flash_cfg.h"
#include "config_kitchen.h"
#include "utility.h"
#include "wizchip_conf.h"
#include "pico/unique_id.h"
#include <stdio.h>


// #define NETINFO_IP      {192, 168, 178, 225} 
const config_t default_config = {
    .version = 0x0101,
    .reserved = 0,

    .net_info.mac    = NETINFO_MAC,
    .net_info.ip     = NETINFO_IP,
    .net_info.sn     = NETINFO_SN,
    .net_info.gw     = NETINFO_GW,
    .net_info.dns    = NETINFO_DNS,
#if _WIZCHIP_ > W5500
    // for _WIZCHIP_ > W5500
    .net_info.lla    = NETINFO_LLA,
    .net_info.gua    = NETINFO_GUA,
    .net_info.sn6    = NETINFO_SN6,
    .net_info.gw6    = NETINFO_GW6,
    .net_info.dns6   = NETINFO_DNS6,
    .net_info.ipmode = NETINFO_IPMODE,  // NETINFO_STATIC_ALL,
#else
    // for _WIZCHIP_ <= W5500
    .net_info.dhcp   = NETINFO_STATIC,  // 
#endif

    .crc32 = 0 /* computed at runtime */
};

/**
 * Table of general configurations in RAM
 * [0] - current config
 * [1] - changing config before save (for save to flash)
 * [2] - loaded config from flash
 */
static config_t gen_cfg[3];


/**
 * Try to get configuration from flash,
 * if not present, use default configuration
 * and put it to id = 0 (current config)
 * 
 */
void config_init(void) {
    if (config_load() == BOOTROM_OK) {
        // loaded successfully from flash
        memcpy(&gen_cfg[0], &gen_cfg[2], sizeof(config_t));
    } else {
        pico_unique_board_id_t id;

        // copy from default
        memcpy(&gen_cfg[0], &default_config, sizeof(config_t));
        // modify mac aaddress to be unique
        pico_get_unique_board_id(&id);
        gen_cfg[0].net_info.mac[3] = id.id[5];
        gen_cfg[0].net_info.mac[4] = id.id[6];
        gen_cfg[0].net_info.mac[5] = id.id[7];
    }
    // prepare changing config as copy of current
    memcpy(&gen_cfg[1], &gen_cfg[0], sizeof(config_t));
    memset(&gen_cfg[2], 0, sizeof(config_t)); // clear loaded config
}

void config_default(void) {
    pico_unique_board_id_t id;

    // copy from default
    memcpy(&gen_cfg[1], &default_config, sizeof(config_t));
    // modify mac aaddress to be unique
    pico_get_unique_board_id(&id);
    gen_cfg[1].net_info.mac[3] = id.id[5];
    gen_cfg[1].net_info.mac[4] = id.id[6];
    gen_cfg[1].net_info.mac[5] = id.id[7];

    // prepare changing config as copy of current
    memcpy(&gen_cfg[1], &gen_cfg[0], sizeof(config_t));
}
void config_recovery(void) {
    // prepare changing config as copy of current
    memcpy(&gen_cfg[1], &gen_cfg[0], sizeof(config_t));
}

wiz_NetInfo *config_get_net_info(void) {
    return &gen_cfg[0].net_info;
}


config_t *config_get(uint8_t select) {
    switch(select) {
        case 1:
            return &gen_cfg[1];
        case 2:
            return &gen_cfg[2];
        // default:
        //   return &g_cfg[0]; 
    };
    return &gen_cfg[0];
}

int config_show(config_t *cfg, int id, char *msg, size_t msg_max_sz) {
    char *cursor = msg;
    size_t remaining = msg_max_sz;

    msg_printf(&cursor, &remaining,
        "Configuration[%d]:\r\n"
        "\tVersion: %04x\r\n"
        "\tIP : %d.%d.%d.%d\r\n"
        "\tSN : %d.%d.%d.%d\r\n"
        "\tGW : %d.%d.%d.%d\r\n"
        "\tDNS: %d.%d.%d.%d\r\n"
        "\tIP Mode: %s\r\n",
        id,
        cfg->version,
        cfg->net_info.ip[0], cfg->net_info.ip[1], cfg->net_info.ip[2], cfg->net_info.ip[3],
        cfg->net_info.sn[0], cfg->net_info.sn[1], cfg->net_info.sn[2], cfg->net_info.sn[3],
        cfg->net_info.gw[0], cfg->net_info.gw[1], cfg->net_info.gw[2], cfg->net_info.gw[3],
        cfg->net_info.dns[0], cfg->net_info.dns[1], cfg->net_info.dns[2], cfg->net_info.dns[3],
        (cfg->net_info.ipmode == NETINFO_STATIC_ALL) ? "Static" :
        (cfg->net_info.ipmode == NETINFO_DHCP_V4) ? "DHCP IPv4" :
        (cfg->net_info.ipmode == NETINFO_DHCP_V6) ? "DHCP IPv6" :
        (cfg->net_info.ipmode == NETINFO_DHCP_ALL) ? "DHCP Both" : "Unknown"
    );

    return (int)(msg_max_sz - remaining);
}



int config_load(void) {
    int ret;
    config_t *flash_cfg = &gen_cfg[2];
    // uint8_t flash_buf[sizeof(config_t)];
    uint32_t offset = XIP_BASE + CONFIG_FLASH_OFFSET;
    cflash_flags_t flags;
    flags.flags = (CFLASH_OP_VALUE_READ << CFLASH_OP_LSB) | (CFLASH_SECLEVEL_VALUE_SECURE << CFLASH_SECLEVEL_LSB);

    // printf("Loading config using flash_read() from 0x%08x\n", offset);

    ret = rom_flash_op(flags, offset, sizeof(config_t), (uint8_t *)flash_cfg);
    if (ret == BOOTROM_ERROR_NOT_PERMITTED) {
        printf("Not permitted, as expected\n");
        return ret;
    } else if (ret) {
        printf("Flash OP failed with %d\n", ret);
        return ret;
    }

    uint32_t flash_crc32 = config_crc32(flash_cfg, sizeof(config_t) - sizeof(uint32_t));
    if (flash_crc32 != flash_cfg->crc32) {
        printf("Config CRC32 mismatch: computed 0x%08x, stored 0x%08x\r\n",
               flash_crc32, flash_cfg->crc32);
        return BOOTROM_ERROR_INVALID_DATA;
    }

    return BOOTROM_OK;
}

/**
 * Saving configuration
 * Writes must erase the entire 4 KB block.
 */
bool config_save(const config_t *cfg) {
    config_t tmp;
    memcpy(&tmp, cfg, sizeof(tmp));

    tmp.crc32 = config_crc32(&tmp, sizeof(tmp) - sizeof(uint32_t));

    uint32_t ints = save_and_disable_interrupts();
    /* Erase 4KB block */
    flash_range_erase(CONFIG_FLASH_OFFSET, CONFIG_SECTOR_SIZE);

    /* Write exactly sizeof(config_t), but flash writes require 256-byte alignment */
    flash_range_program(CONFIG_FLASH_OFFSET, 
                        (const uint8_t *)&tmp,
                        sizeof(tmp));

    restore_interrupts(ints);
    return true;
}




