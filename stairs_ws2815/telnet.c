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
#include "ws2815_control_dma_parallel.h"
#include "config.h"
#include "partition.h"

// This help has 790 bytes
const char *help_msg =
"\r\n"
"========================================\r\n"
"   WIZnet RP2350 Remote CLI Interface   \r\n"
"========================================\r\n"
"Board : RP2350\r\n"
"FW Ver: 1.0.0\r\n"
"\r\n"
"Available commands:\r\n"
"  help   \t\t- Show this help menu\r\n"
"  info   \t\t- Display board and firmware information\r\n"
"  set [p]\t\t- Set active LED/pattern index to value [p]\r\n"
"  get    \t\t- Get current pattern index\r\n"
"  time   \t\t- Show timing statistics (min/max execution)\r\n"
"  status \t\t- Show system analog and digital state\r\n"
"  on     \t\t- Enable outputs\r\n"
"  off    \t\t- Disable outputs\r\n"
"  part   \t\t- Show partition information\r\n"
"  exit   \t\t- Close the CLI connection\r\n"
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
void telnet_send(int sn, const char *msg) {
    // Send the main message first
    send(sn, (uint8_t *)msg, strlen(msg));

    // Then send the prompt
    const char *prompt = TELNET_PROMPT;
    send(sn, (uint8_t*)prompt, strlen(prompt));
}


void telnet_greeting(int sn, const uint8_t *client_ip) {
    char msg[256];

    snprintf(msg, sizeof(msg), cli_greeting, 
        client_ip[0], client_ip[1], client_ip[2], client_ip[3]);
/* 
    snprintf(msg, sizeof(msg),
        "\r\n"
        "========================================\r\n"
        "   Welcome to WIZnet RP2350 CLI Shell   \r\n"
        "========================================\r\n"
        "Connected from: %d.%d.%d.%d\r\n"
        "Type 'help' to see available commands.\r\n"
        "----------------------------------------\r\n",
        client_ip[0], client_ip[1], client_ip[2], client_ip[3]);
 */
    telnet_send(sn, msg);  // (uint8_t*)  , strlen(msg)
}

 void handle_command(const char *cmd, int sn) {
    // int sn = TCP_CLI_SOCKET;

    if (strcmp(cmd, "help") == 0) {
        telnet_send(sn, help_msg);    // (uint8_t*)  , strlen(help_msg)
    }
    else if (strcmp(cmd, "info") == 0) {
        const char *msg = "Board: RP2350\r\nFW: 1.0.6\r\n";
        telnet_send(sn, msg);   // , strlen(msg)
        read_boot_info();
    }
    else if (strcmp(cmd, "part") == 0) {
        char msg[800];     // current use 407 bytes
        int len = partition_info(msg, sizeof(msg));
        printf("Telnet sent %d bytes to console\r\n", len);
        telnet_send(sn, msg);
    }

    else if (strncmp(cmd, "flash_erase", 11) == 0) {
        uint32_t offs = 0, size = 4096;
        if (sscanf(cmd + 11, "%x %u", &offs, &size) >= 1) {
            printf("[TELNET] Erasing flash offset 0x%08x, size %u\n", offs, size);
            // Make sure offset is within range
            if ((offs + size) <= (PICO_FLASH_SIZE_BYTES/2)) {
                util_flash_erase(offs, size);
                printf("[TELNET] Done.\n");
            } else {
                printf("[TELNET] Range out of flash.\n");
            }
        } else {
            printf("Usage: flash_erase <hex_offset> [size]\n");
        }
    }

    else if (strncmp(cmd, "set", 3) == 0) {
        int pattern = -1;
        uint8_t ret_pattern;

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

    // else if (strcmp(cmd, "40") == 0) {
    //     char msg[32];
    //     gpio_put(PIN_TEST_14, 0);
    //     snprintf(msg, sizeof(msg),
    //                 "Off pin 14\r\n");
    //         telnet_send(sn, msg);
    // }
    // else if (strcmp(cmd, "41") == 0) {
    //     char msg[32];
    //     gpio_put(PIN_TEST_14, 1);
    //     snprintf(msg, sizeof(msg),
    //                 "On pin 14\r\n");
    //         telnet_send(sn, msg);
    // }
    // else if (strcmp(cmd, "50") == 0) {
    //     char msg[32];
    //     gpio_put(PIN_TEST_15, 0);
    //     snprintf(msg, sizeof(msg),
    //                 "Off pin 15\r\n");
    //         telnet_send(sn, msg);
    // }
    // else if (strcmp(cmd, "51") == 0) {
    //     char msg[32];
    //     gpio_put(PIN_TEST_15, 1);
    //     snprintf(msg, sizeof(msg),
    //                 "On pin 15\r\n");
    //         telnet_send(sn, msg);
    // }


    else if (strcmp(cmd, "get") == 0) {
        uint8_t ret_pattern;
        char msg[64];

        ret_pattern = get_pattern_index();

        snprintf(msg, sizeof(msg),
                "Pattern index: %d\r\n", ret_pattern);
        telnet_send(sn, msg);
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
    else if (strcmp(cmd, "exit") == 0) {
        const char *msg="Closing connection...\r\n";
        send(sn, (uint8_t *)msg, strlen(msg));
        sleep_ms(2);
        // Graceful disconnect, better to use telnet
        disconnect(sn);
        // close(sn);      // option, telnet has problems
        printf("[CLI] Socket %d disconnected by user\r\n", sn);

    }
    else {
        const char *msg = "Unknown command\r\n";
        telnet_send(sn, msg);   // , strlen(msg)
    }
}
