/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * 
 * examples:
 *  $ nc 192.168.178.225 5000
 *  $ telnet 192.168.178.225 5000
 */

#include <stdio.h>
#include <string.h>

#include <port_common.h>
#include "wizchip_conf.h"
// #include "wizchip_spi.h"
#include "socket.h"
#include "pico/bootrom.h"
#include "telnet.h"
#include "ws2815_control_dma.h"
#include "config_tree.h"
#include "partition.h"
#include "flash_cfg.h"
#include "utility.h"
#include "efu_update.h"
#include "vl53_diag.h"

// This help has 790 bytes
const char *help_msg =
"\r\n"
"========================================\r\n"
"   WIZnet RP2350 Remote CLI Interface   \r\n"
"========================================\r\n"
"\r\n"
"Available commands:\r\n"
"  help   \t\t\t- Show this help menu\r\n"
"  info   \t\t\t- Display board and firmware information\r\n"
"  set [p]\t\t\t- Set active LED/pattern index to value [p]\r\n"
"  get    \t\t\t- Get current pattern index\r\n"
"  time   \t\t\t- Show timing statistics (min/max execution)\r\n"
"  status \t\t\t- Show system analog and digital state\r\n"
"  on     \t\t\t- Enable outputs\r\n"
"  off    \t\t\t- Disable outputs\r\n"
"  load   \t\t\t- Load config from flash - for debug\r\n"
"  save   \t\t\t- Save config to flash\r\n"
"  show   \t\t\t- Show config values\r\n"
"  rgb <r> <g> <b> \t\t- Set color LEDs\r\n"
"  max <value> \t\t- Show config values\r\n"
"  part   \t\t\t- Show partition information\r\n"
"  config ip <a.b.c.d>  \t- Set IP address\r\n"
"  config sn <a.b.c.d>  \t- Set Subnet Mask\r\n"
"  config gw <a.b.c.d>  \t- Set Gateway\r\n"
"  config dns <a.b.c.d> \t- Set DNS server\r\n"
"  config save          \t- Save current config to flash\r\n"
"  config show          \t- Show current config values\r\n"
"  config clean         \t- Clean current config (use default)\r\n"
"  config default       \t- Restore factory default configuration\r\n"
"  exit   \t\t\t- Close the CLI connection\r\n"
"\r\n"
"Examples:\r\n"
"  > set 3\r\n"
"  > get\r\n"
"  > time\r\n"
"\r\n"
"Type 'help' anytime to show this menu.\r\n"
"========================================\r\n";

const char *cli_greeting =
"\r\n"
"========================================\r\n"
"   Welcome to WIZnet RP2350 CLI Shell   \r\n"
"========================================\r\n"
"Connected from: %d.%d.%d.%d\r\n"
"Type 'help' to see available commands.\r\n"
"----------------------------------------\r\n";

#define TELNET_PROMPT  "> "

//void telnet_send(int sn, const char *msg, size_t len) {
//    // Send the main message first
//    send(sn, (uint8_t*)msg, len);   // strlen(msg)
void telnet_send(uint8_t sn, const char *msg) {
    // Send the main message first
    send(sn, (uint8_t *)msg, (uint16_t)strlen(msg));

    // Then send the prompt
    const char *prompt = TELNET_PROMPT;
    send(sn, (uint8_t*)prompt, (uint16_t)strlen(prompt));
}


void telnet_greeting(uint8_t sn, const uint8_t *client_ip) {
    char msg[256];

    snprintf(msg, sizeof(msg), cli_greeting, 
        client_ip[0], client_ip[1], client_ip[2], client_ip[3]);

    telnet_send(sn, msg);  // (uint8_t*)  , strlen(msg)
}

static bool parse_ipv4(const char *s, uint8_t out[4]) {
    int a, b, c, d;
    if (sscanf(s, "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
        return false;
    if ((a|b|c|d) & ~0xFF)
        return false;
    out[0] = (uint8_t)a;
    out[1] = (uint8_t)b;
    out[2] = (uint8_t)c;
    out[3] = (uint8_t)d;
    return true;
}

void cmd_config_show(uint8_t sn) {
        char msg[200];     // current use ??? bytes
        config_t *config = NULL;
        int len;
        uint8_t id = 0;

        config = config_get(id);
        // snprintf(msg, sizeof(msg), "Config [0]\r\n");
        // send(sn, (uint8_t *)msg, strlen(msg));
        len = config_show( config, id, msg, sizeof(msg));   // sending > 102 bytes
        printf("Telnet sent %d bytes to console for config[0]\r\n", len);
        send(sn, (uint8_t *)msg, (uint16_t)strlen(msg));

        config = config_get(++id);
        // snprintf(msg, sizeof(msg), "Config [1]\r\n");
        // send(sn, (uint8_t *)msg, strlen(msg));
        len = config_show( config, id, msg, sizeof(msg));   // sending > 102 bytes
        printf("Telnet sent %d bytes to console for config[1]\r\n", len);
        send(sn, (uint8_t *)msg, (uint16_t)strlen(msg));

        config = config_get(++id);
        // snprintf(msg, sizeof(msg), "Config [2]\r\n");
        // send(sn, (uint8_t *)msg, (uint16_t)strlen(msg));
        len = config_show( config, id, msg, sizeof(msg));   // sending > 102 bytes
        printf("Telnet sent %d bytes to console for config[2]\r\n", len);

        telnet_send(sn, msg);
}
void cmd_config_save(uint8_t sn) {
        bool ret;
        char msg[30];
        config_t *config = config_get(1);

        ret = config_save(config);
        snprintf(msg, sizeof(msg),
                "Config saved to flash: %d\r\n", ret);
        telnet_send((uint32_t)sn, msg);
}
void cmd_config_set_ip(int sn, uint8_t *ip) {
    (void)sn;    
    config_t *config = config_get(1);
    memcpy(config->net_info.ip, ip, 4);
}
void cmd_config_set_sn(int sn, uint8_t *snm) {
    (void)sn;
    config_t *config = config_get(1);
    memcpy(config->net_info.sn, snm, 4);
}
void cmd_config_set_gw(int sn, uint8_t *gw) {
    (void)sn;
    config_t *config = config_get(1);
    memcpy(config->net_info.gw, gw, 4);
}
void cmd_config_set_dns(int sn, uint8_t *dns) {
    (void)sn;
    config_t *config = config_get(1);
    memcpy(config->net_info.dns, dns, 4);
}

 void handle_command(const char *cmd, uint8_t sn) {
    // int sn = TCP_CLI_SOCKET;

    if (strcmp(cmd, "help") == 0) {
        telnet_send(sn, help_msg);
    }
    else if (strcmp(cmd, "info") == 0) {
        // const char *msg = "Board: RP2350\r\nFW: 1.0.4\r\n";
        char msg[80];
        static const char * const efu_status_table[]= {
            "UNKNOWN",          // 0
            "SOCK_CLOSED",      // 1
            "SOCK_INIT",        // 2
            "SOCK_LISTEN",      // 3
            "SOCK_ESTABLISHED", // 4
            "SOCK_CLOSE_WAIT"   // 5
    };
        const char *efu_stat_msg = efu_status_table[0];

        int8_t efu_status = get_efu_socket_status();
        switch(efu_status) {
            case SOCK_CLOSED:
                efu_stat_msg = efu_status_table[1];
                break;
            case SOCK_INIT:
                efu_stat_msg = efu_status_table[2];
                break;
            case SOCK_LISTEN:       // after listen(TCP_EFU_SOCKET)
                efu_stat_msg = efu_status_table[3];
                break;
            case SOCK_ESTABLISHED:
                efu_stat_msg = efu_status_table[4];
                break;
            case SOCK_CLOSE_WAIT:
                efu_stat_msg = efu_status_table[5];
                break;
        }

        snprintf(msg, sizeof(msg),
            "Project : stairs_ws2815\r\n"
            "FW      : %s\r\n"
            "EFU stat: 0x%x (%s)\r\n",
            FW_VERSION, efu_status, efu_stat_msg);

        telnet_send(sn, msg);
        // read_boot_info();
    }
    else if (strcmp(cmd, "part") == 0) {
        char msg[800];     // current use 407 bytes
        int len = partition_info(msg, sizeof(msg));
        printf("Telnet sent %d bytes to console\r\n", len);
        telnet_send(sn, msg);
    }


    else if (strncmp(cmd, "rgb", 3) == 0) {
        uint16_t r, g, b;

        // Skip "rgb" and parse three integers
        if (sscanf(cmd + 3, "%u %u %u", &r, &g, &b) == 3 &&
            r <= 255 && g <= 255 && b <= 255) 
        {
            char msg[64];
            // Example: your function to apply RGB globally or per pattern
            // set_global_rgb((uint8_t)r, (uint8_t)g, (uint8_t)b);
            set_rgb((uint8_t)r, (uint8_t)g, (uint8_t)b);

            snprintf(msg, sizeof(msg),
                    "RGB set to %u %u %u\r\n", r, g, b);
            telnet_send(sn, msg);
        } 
        else {
            const char *err =
                "Usage: rgb <r> <g> <b>\r\n"
                "Each value must be 0–255.\r\n";
            telnet_send(sn, err);
        }
    }
    else if (strncmp(cmd, "max", 3) == 0) {
        uint32_t val;

        // Parse a single unsigned integer after "max"
        if (sscanf(cmd + 3, "%u", &val) == 1 && val <= 65535) {
            char msg[64];

            // Example: apply the parameter in your firmware
            // set_max_value((uint16_t)val);
            set_max_led(val);

            snprintf(msg, sizeof(msg),
                    "Max value set to %u\r\n", val);
            telnet_send(sn, msg);
        }
        else {
            const char *err =
                "Usage: max <value>\r\n"
                "Value must be 0–65535.\r\n";
            telnet_send(sn, err);
        }
    }



    else if (strncmp(cmd, "set", 3) == 0) {
        int pattern = -1;
        uint8_t ret_pattern = 0;

        if (strlen(cmd) < 4) {
            char msg[64];
            // switch off LEDs
            ret_pattern = set_pattern_index(0);
            snprintf(msg, sizeof(msg),
                    "Pattern index set to %d\r\n", ret_pattern);
            telnet_send(sn, msg);
        }
        else if (sscanf(cmd + 3, "%d", &pattern) == 1 && pattern >= 0) {
            char msg[64];

            ret_pattern = set_pattern_index((uint8_t)pattern);
            snprintf(msg, sizeof(msg),
                    "Pattern index set to %d\r\n", ret_pattern);
            telnet_send(sn, msg);
        } else {
            // ❌ parameter missing or invalid
            const char *err = "Usage: set [p]\r\nExample: set 3\r\n";
            telnet_send(sn, err);
        }
    }
    else if (strcmp(cmd, "on") == 0) {
        char msg[32];
        gpio_put(OE_PIN, OE_ON);
        snprintf(msg, sizeof(msg),
                    "Enable outputs\r\n");
            telnet_send(sn, msg);
    }
    else if (strcmp(cmd, "off") == 0) {
        char msg[32];
        gpio_put(OE_PIN, OE_OFF);
        snprintf(msg, sizeof(msg),
                    "Disable outputs\r\n");
            telnet_send(sn, msg);
    }


    else if (strcmp(cmd, "get") == 0) {
        uint8_t ret_pattern;
        char msg[64];

        ret_pattern = get_pattern_index();

        snprintf(msg, sizeof(msg),
                "Pattern index: %d\r\n", ret_pattern);
        telnet_send(sn, msg);
    }

    
    else if (strcmp(cmd, "vl53 gpio") == 0) {
        vl53_diag_print_gpio(sn);
    }
    else if (strcmp(cmd, "vl53 spi") == 0) {
        #ifdef VL53_SPI
        vl53_diag_print_spi1(sn);
        #endif // VL53_SPI
    }
    else if (strcmp(cmd, "vl53 probe") == 0) {
        vl53_diag_probe_bus(sn);
    }
    else if (strcmp(cmd, "vl53 raw") == 0) {
        #ifdef VL53_SPI
        vl53_diag_raw_spi_test(sn);
        #endif
    }
    else if (strcmp(cmd, "vl53 read") == 0) {
        vl53_diag_read_one(sn);
    }
    else if (strcmp(cmd, "vl53 start") == 0) {
        vl53_diag_start_ranging(sn);
    }


    else if (strcmp(cmd, "cson") == 0) {
        vl53_diag_cs_active(sn);
    }
    else if (strcmp(cmd, "csoff") == 0) {
        vl53_diag_cs_inactive(sn);
    }



    else if (strncmp(cmd, "config", 6) == 0) {
        const char *p = cmd + 6;
        while (*p == ' ') p++;

        if (*p == '\0') {
            telnet_send(sn, "Usage: config <ip|sn|gw|dns|save|show|clean|default> ...\r\n");
            return;
        }

        /* --- config ip <a.b.c.d> --- */
        if (strncmp(p, "ip", 2) == 0 && (p[2] == ' ')) {
            uint8_t ip[4];
            const char *arg = p + 3;
            while (*arg == ' ') arg++;
            if (parse_ipv4(arg, ip)) {
                cmd_config_set_ip(sn, ip);
                telnet_send(sn, "IP updated\r\n");
            } else {
                telnet_send(sn, "Invalid IP format\r\n");
            }
        }

        /* --- config sn <mask> --- */
        else if (strncmp(p, "sn", 2) == 0 && (p[2] == ' ')) {
            uint8_t snm[4];
            const char *arg = p + 3;
            while (*arg == ' ') arg++;
            if (parse_ipv4(arg, snm)) {
                cmd_config_set_sn(sn, snm);
                telnet_send(sn, "Subnet mask updated\r\n");
            } else {
                telnet_send(sn, "Invalid subnet mask\r\n");
            }
        }

        /* --- config gw <a.b.c.d> --- */
        else if (strncmp(p, "gw", 2) == 0 && (p[2] == ' ')) {
            uint8_t gw[4];
            const char *arg = p + 3;
            while (*arg == ' ') arg++;
            if (parse_ipv4(arg, gw)) {
                cmd_config_set_gw(sn, gw);
                telnet_send(sn, "Gateway updated\r\n");
            } else {
                telnet_send(sn, "Invalid gateway\r\n");
            }
        }

        /* --- config dns <a.b.c.d> --- */
        else if (strncmp(p, "dns", 3) == 0 && (p[3] == ' ')) {
            uint8_t dns[4];
            const char *arg = p + 4;
            while (*arg == ' ') arg++;
            if (parse_ipv4(arg, dns)) {
                cmd_config_set_dns(sn, dns);
                telnet_send(sn, "DNS updated\r\n");
            } else {
                telnet_send(sn, "Invalid DNS address\r\n");
            }
        }

        /* --- config save --- */
        else if (strcmp(p, "save") == 0) {
            cmd_config_save(sn);
        }

        /* --- config show --- */
        else if (strcmp(p, "show") == 0) {
            cmd_config_show(sn);
        }

        /* --- config clean --- */
        else if (strcmp(p, "clean") == 0) {
            config_recovery();
            telnet_send(sn, "Configuration cleaned\r\n");
        }

        /* --- config default --- */
        else if (strcmp(p, "default") == 0) {
            config_default();
            telnet_send(sn, "Factory default configuration restored\r\n");
        }

        else {
            telnet_send(sn, "Unknown config command\r\n");
        }
    }

    else if (strcmp(cmd, "exit") == 0) {
        const char *msg="Closing connection...\r\n";
        send(sn, (uint8_t *)msg, (uint16_t)strlen(msg));
        sleep_ms(2);
        // Graceful disconnect, better to use telnet
        disconnect(sn);
        // close(sn);      // option, telnet has problems
        printf("[CLI] Socket %d disconnected by user\r\n", sn);

    }

    // else if (strcmp(cmd, "led 0") == 0) {
    //     gpio_put(PICO_DEFAULT_LED_PIN, 0);
    //     telnet_send(sn, "LED OFF\r\n"); // , 9
    // }
    // else if (strcmp(cmd, "reboot") == 0) {
    //     telnet_send(sn, "Rebooting...\r\n");    // , 14
    //     sleep_ms(100);
    //     reset_usb_boot(0, 0);
    // }

    // else if (strcmp(cmd, "init") == 0) {
    //     int ret;
    //     char msg[30];

    //     printf("Call function: config_init\r\n");
    //     ret = config_init();
    //     snprintf(msg, sizeof(msg),
    //             "Config initialized: %d\r\n", ret);
    //     telnet_send(sn, msg);
    // }


    // else if (strncmp(cmd, "flash_erase", 11) == 0) {
    //     uint32_t offs = 0, size = 4096;
    //     if (sscanf(cmd + 11, "%x %u", &offs, &size) >= 1) {
    //         printf("[TELNET] Erasing flash offset 0x%08x, size %u\n", offs, size);
    //         // Make sure offset is within range
    //         if ((offs + size) <= (PICO_FLASH_SIZE_BYTES/2)) {
    //             util_flash_erase(offs, size);
    //             printf("[TELNET] Done.\n");
    //         } else {
    //             printf("[TELNET] Range out of flash.\n");
    //         }
    //     } else {
    //         printf("Usage: flash_erase <hex_offset> [size]\n");
    //     }
    // }

    else {
        const char *msg = "Unknown command\r\n";
        telnet_send(sn, msg);
    }
}
