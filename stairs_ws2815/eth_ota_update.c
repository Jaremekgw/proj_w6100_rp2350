/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "pico/bootrom.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "boot/picoboot_constants.h"
#include "partition.h"

#include "config.h"

// Socket definitions
// #define TCP_OTA_SOCKET    1
// #define TCP_OTA_PORT      4242
// #define OTA_BUF_SIZE      2048

// OTA server states
typedef enum {
    OTA_IDLE,
    OTA_LISTENING,
    OTA_HEADER,
    OTA_ERASING,
    OTA_WRITING,
    OTA_DONE,
    OTA_ERROR
} ota_state_t;

typedef struct {
    ota_state_t state;
    uint32_t write_addr;        // current write address in flash
    uint32_t partition_size;    // size of target partition
    uint32_t expected_size;

    uint8_t header_buf[8];
    uint8_t header_received;

    uint8_t buf[OTA_BUF_SIZE];
    uint32_t total_written;
    bool complete;
    bool reboot_requested;
} ota_server_t;

static ota_server_t ota_srv;

// --------------------------------------------------------------------
// Initialize OTA listener (open TCP socket)
// --------------------------------------------------------------------
void ota_server_init(void) {
    ota_srv.state = OTA_IDLE;
    ota_srv.complete = false;
    ota_srv.total_written = 0;
    ota_srv.write_addr = 0;

    if (socket(TCP_OTA_SOCKET, Sn_MR_TCP, TCP_OTA_PORT, Sn_MR_ND) != TCP_OTA_SOCKET) {
        printf("[OTA] Socket open failed\r\n");
        ota_srv.state = OTA_ERROR;
        return;
    }
    if (listen(TCP_OTA_SOCKET) != SOCK_OK) {
        printf("[OTA] Listen failed\r\n");
        ota_srv.state = OTA_ERROR;
        return;
    }

    ota_srv.state = OTA_LISTENING;
    printf("[OTA] Listening on port %d\r\n", TCP_OTA_PORT);
}

// --------------------------------------------------------------------
// Handle OTA TCP socket — call periodically from main loop
// --------------------------------------------------------------------
void ota_server_poll(void) {
    // static partition_info_t alt_part[1];
    int8_t sn = TCP_OTA_SOCKET;
    uint8_t destip[4];
    uint16_t destport;
    int32_t ret;
    int consumed;

    switch (getSn_SR(sn)) {

    case SOCK_ESTABLISHED:
        if (getSn_IR(sn) & Sn_IR_CON) {
            getSn_DIPR(sn, destip);
            destport = getSn_DPORT(sn);
            printf("[OTA] Client connected from %d.%d.%d.%d:%d\r\n",
                   destip[0], destip[1], destip[2], destip[3], destport);
            setSn_IR(sn, Sn_IR_CON);

            ota_srv.state = OTA_HEADER;
            ota_srv.header_received = 0;
            ota_srv.total_written = 0;
            ota_srv.complete = false;
        }

        uint16_t rx_size = getSn_RX_RSR(sn);
        if (!rx_size) break;    // Nothing to read this poll; let main loop continue
        if (rx_size > OTA_BUF_SIZE) rx_size = OTA_BUF_SIZE;

        ret = recv(sn, ota_srv.buf, rx_size);
        // if (ret < 0) {
        //     // Socket error
        //     printf("[OTA] recv error %ld, closing\r\n", ret);
        //     ota_srv.complete = true;
        //     ota_srv.state = OTA_ERROR;
        //     disconnect(sn);
        //     break;
        // }
        // if (ret == 0) {
        //     // Peer closed connection or no more data
        //     printf("[OTA] recv returned 0, transfer complete (%lu bytes)\r\n",
        //         ota_srv.total_written);
        //     ota_srv.complete = true;
        //     ota_srv.state = OTA_DONE;
        //     disconnect(sn);
        //     break;
        // }
        if (ret <= 0) break;

        consumed = 0;
        // ----- HEADER STAGE -----
        if (ota_srv.state == OTA_HEADER) {

            // (ota_srv.header_received < 8)

            while (ota_srv.header_received < 8 && consumed < ret) {
                ota_srv.header_buf[ota_srv.header_received++] = ota_srv.buf[consumed++];
            }

            if (ota_srv.header_received < 8) {
                // Need more incoming data
                break;
            }

            // Parse header
            uint8_t *H = ota_srv.header_buf;

            if (H[0] != 0xD1 || H[1] != 0x36 || H[2] != 0x4A) {
                printf("[OTA] Invalid header stamp: %02X %02X %02X\n", H[0], H[1], H[2]);
                ota_srv.state = OTA_ERROR;
                //ota_srv.complete = true;
                disconnect(sn);
                break;
            }

            uint8_t proto = H[3];
            ota_srv.expected_size = (H[4] << 24) | (H[5] << 16) | (H[6] << 8) | H[7];

            printf("[OTA] Header OK: proto=0x%02X size=%u bytes\n",
                proto, ota_srv.expected_size);

            ota_srv.state = OTA_ERASING;

            // If no more data in this packet, stop here
            if (consumed == ret) break;
        }

        // ret > 0 from here on: we have data to write
        // First data packet triggers partition selection + erase
        if (ota_srv.state == OTA_ERASING) {
            boot_info_t boot_info[1];
            partition_info_t alt_part[1];

            rom_get_boot_info(boot_info);
            // ... your debug prints ...

            if (get_alt_part_addr(boot_info, alt_part) < 0) {
                #ifdef _OTA_DEBUG_
                printf("[OTA] Error: Failed to get alt partition info for current %d\r\n", boot_info->partition);
                #endif
                //ota_srv.complete = true;
                ota_srv.state = OTA_ERROR;
                disconnect(sn);
                break;
            }

            #ifdef _OTA_DEBUG_
            printf("[OTA] Target address:0x%x size:0x%x\r\n", alt_part->start_addr, alt_part->size);
            #endif
            ota_srv.write_addr = alt_part->start_addr;
            ota_srv.partition_size = alt_part->size;
            ota_srv.state = OTA_WRITING;

            #ifdef _OTA_DEBUG_
            printf("[OTA] Erasing alternate partition from @0x%08x, size %u kB\r\n",
                ota_srv.write_addr, alt_part->size / 1024);
            #endif
            // Erase alternate partition
            uint32_t save = save_and_disable_interrupts();
            flash_range_erase(ota_srv.write_addr - XIP_BASE, ota_srv.partition_size);
            restore_interrupts(save);
        }

        if (ota_srv.state == OTA_WRITING) {
            // Bounds check
            uint32_t base_offs   = ota_srv.write_addr - XIP_BASE;
            uint32_t flash_offs  = base_offs + ota_srv.total_written;
            uint32_t end = base_offs + ota_srv.partition_size;

            int data_len = ret - consumed;
            if (data_len <= 0) break;


            if (flash_offs + data_len > end) {
                printf("[OTA] Error: incoming image too large (%lu + %d > %u)\r\n",
                    ota_srv.total_written, data_len, ota_srv.partition_size);
                //ota_srv.complete = true;
                ota_srv.state = OTA_ERROR;
                disconnect(sn);
                break;
            }

            // printf("[OTA] Programming %d bytes at flash offset 0x%08x\n", data_len, flash_offs);
            // Program flash
            uint32_t ints = save_and_disable_interrupts();
            flash_range_program(flash_offs, ota_srv.buf + consumed, data_len);
            restore_interrupts(ints);

            ota_srv.total_written += data_len;

             // Send ACK back to PC
            uint8_t ack[2] = { 'O', 'K' };
            send(sn, ack, 2);

            // if ((ota_srv.total_written & 0xFF) == 0)
            //     printf("[OTA] Written %lu / %u bytes\n",
            //            ota_srv.total_written, ota_srv.expected_size);

            //{
            //    printf("[OTA] Written %lu bytes so far\r\n", ota_srv.total_written);
            //}
            // Completed?
            if (ota_srv.total_written == ota_srv.expected_size) {
                printf("[OTA] Transfer complete (%u/%u bytes)\n", ota_srv.total_written, ota_srv.expected_size);
                ota_srv.complete = true;
                ota_srv.state = OTA_DONE;
                disconnect(sn);
            }
        }
        break;

    case SOCK_CLOSE_WAIT:
        printf("[OTA] Transfer complete (%lu bytes)\r\n", ota_srv.total_written);
        printf("[OTA] socket closed early, transfered (%lu bytes)\n", ota_srv.total_written);
        // ota_srv.complete = true;
        disconnect(sn);
        ota_srv.state = OTA_ERROR;
        break;

    case SOCK_CLOSED:
        if (ota_srv.state == OTA_DONE &&
            ota_srv.complete &&
            !ota_srv.reboot_requested)
        {
            ota_srv.reboot_requested = true;
            printf("[OTA] Rebooting to apply update...\n");

            int r = rom_reboot(REBOOT2_FLAG_REBOOT_TYPE_FLASH_UPDATE,
                            1000,
                            ota_srv.write_addr,
                            0);
            printf("[OTA] rom_reboot returned %d\n", r);

            while (1) tight_loop_contents();
        }
        break;

    // case SOCK_CLOSED:
    //     if (ota_srv.state == OTA_DONE && ota_srv.complete) {
    //         printf("[OTA] Rebooting to apply update...\r\n");
    //         rom_reboot(REBOOT2_FLAG_REBOOT_TYPE_FLASH_UPDATE, 1000, 0, 0);
    //         sleep_ms(500);
    //     } else if (ota_srv.state != OTA_ERROR) {
    //         // reopen socket to listen again
    //         ota_server_init();
    //     }
    //     break;

    default:
        break;
    }
}
