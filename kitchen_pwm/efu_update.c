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

#include "config_kitchen.h"
#include "utility.h"

// EFU server states
typedef enum {
    EFU_IDLE,           // 0
    EFU_LISTENING,
    EFU_HEADER,         // 2
    EFU_ERASING,
    EFU_WRITING,        // 4
    EFU_WAIT_FOR_CRC,   // 5
    EFU_DONE,
    EFU_ERROR
} efu_state_t;

typedef struct {
    efu_state_t state;
    uint32_t write_addr;        // current write address in flash
    uint32_t partition_size;    // size of target partition
    uint32_t expected_size;

    uint8_t header_buf[8];
    uint8_t header_received;

    uint8_t buf[EFU_BUF_SIZE];
    uint32_t total_written;
    bool complete;
    bool reboot_requested;
    // add for crc
    uint32_t expected_crc;
    uint32_t crc_calc;
    uint8_t crc_buf[4];
    uint8_t crc_received;
} ota_server_t;

static ota_server_t efu_srv;
uint8_t hdr[2] = {'H', 'D'};    // headeer ack
uint8_t ack[2] = {'O', 'K'};    // write ack
uint8_t err[2] = {'E', 'R'};    // error nack
uint8_t crc[2] = {'C', 'C'};    // crc ack


uint8_t get_efu_socket_status(void) {
    uint32_t sn = TCP_EFU_SOCKET;
    return getSn_SR(sn);
}

// --------------------------------------------------------------------
// Initialize OTA listener (open TCP socket)
// --------------------------------------------------------------------
void efu_server_init(void) {
    efu_srv.state = EFU_IDLE;
    efu_srv.complete = false;
    efu_srv.total_written = 0;
    efu_srv.write_addr = 0;
    efu_srv.crc_calc = 0;
    efu_srv.crc_received = 0;

    if (socket(TCP_EFU_SOCKET, Sn_MR_TCP, TCP_EFU_PORT, Sn_MR_ND) != TCP_EFU_SOCKET) {
        printf("[EFU] Socket open failed\r\n");
        efu_srv.state = EFU_ERROR;
        return;
    }
    if (listen(TCP_EFU_SOCKET) != SOCK_OK) {
        printf("[EFU] Listen failed\r\n");
        efu_srv.state = EFU_ERROR;
        return;
    }

    efu_srv.state = EFU_LISTENING;
    printf("[EFU] Listening on port %d\r\n", TCP_EFU_PORT);
}



// --------------------------------------------------------------------
// Handle OTA TCP socket — call periodically from main loop
// --------------------------------------------------------------------
void efu_server_poll(void) {
    // static partition_info_t alt_part[1];
    uint8_t sn = TCP_EFU_SOCKET;
    uint8_t destip[4];
    uint16_t destport;
    int32_t ret;
    int consumed;

    switch (getSn_SR((uint32_t)sn)) {

    case SOCK_ESTABLISHED:
        if (getSn_IR((uint32_t)sn) & Sn_IR_CON) {
            getSn_DIPR((uint32_t)sn, destip);
            destport = getSn_DPORT((uint32_t)sn);
            printf("[EFU] Client connected from %d.%d.%d.%d:%d\r\n",
                   destip[0], destip[1], destip[2], destip[3], destport);
            setSn_IR((uint32_t)sn, Sn_IR_CON);

            efu_srv.state = EFU_HEADER;
            efu_srv.header_received = 0;
            efu_srv.total_written = 0;
            efu_srv.complete = false;
        }

        uint16_t rx_size = getSn_RX_RSR(sn);
        if (!rx_size) break;    // Nothing to read this poll; let main loop continue
        if (rx_size > EFU_BUF_SIZE) rx_size = EFU_BUF_SIZE;

        ret = recv(sn, efu_srv.buf, rx_size);
        if (ret <= 0) break;
        printf("st=%d \trec:%d \tsum=%d\n", efu_srv.state, ret, efu_srv.header_received + ret);

        consumed = 0;
        // switch(efu_srv.state) {
        //     case EFU_ERROR:
        //         // Just drop data
        //         consumed = ret;
        //         break;
        //     case EFU_DONE:
        //         efu_srv.state = EFU_IDLE;
        //         break;
        // }
        // ----- HEADER STAGE -----
        if (efu_srv.state == EFU_HEADER) {
            while (efu_srv.header_received < 8 && consumed < ret) {
                efu_srv.header_buf[efu_srv.header_received++] = efu_srv.buf[consumed++];
            }

            if (efu_srv.header_received < 8) {
                // Need more incoming data
                break;
            }

            // Parse header
            uint8_t *H = efu_srv.header_buf;

            if (H[0] != 0xD1 || H[1] != 0x36 || H[2] != 0x4A) {
                printf("[EFU] Invalid header stamp: %02X %02X %02X\n", H[0], H[1], H[2]);
                efu_srv.state = EFU_ERROR;
                // Send NACK
                send(sn, err, 2);
                disconnect(sn);
                break;
            }

            uint8_t proto = H[3];
            efu_srv.expected_size = (H[4] << 24) | (H[5] << 16) | (H[6] << 8) | H[7];

            printf("[EFU] Header OK: rev.%d.%d size=%u bytes\n",
                proto/16, proto%16, efu_srv.expected_size);

            efu_srv.state = EFU_ERASING;
        }

        // ret > 0 from here on: we have data to write
        // First data packet triggers partition selection + erase
        if (efu_srv.state == EFU_ERASING) {
            boot_info_t boot_info[1];
            partition_info_t alt_part[1];

            rom_get_boot_info(boot_info);
            // ... your debug prints ...

            if (get_alt_part_addr(boot_info, alt_part) < 0) {
                #ifdef _EFU_DEBUG_
                printf("[EFU] Error: Failed to get alt partition info for current %d\r\n", boot_info->partition);
                #endif
                //ota_srv.complete = true;
                efu_srv.state = EFU_ERROR;
                // Send NACK
                send(sn, err, 2);
                disconnect(sn);
                break;
            }

            #ifdef _EFU_DEBUG_
            printf("[EFU] Target address:0x%x size:0x%x\r\n", alt_part->start_addr, alt_part->size);
            #endif
            efu_srv.write_addr = alt_part->start_addr;
            efu_srv.partition_size = alt_part->size;
            efu_srv.state = EFU_WRITING;

            #ifdef _EFU_DEBUG_
            printf("[EFU] Erasing alternate partition from @0x%08x, size %u kB\r\n",
                efu_srv.write_addr, alt_part->size / 1024);
            #endif
            // Erase alternate partition
            uint32_t save = save_and_disable_interrupts();
            flash_range_erase(efu_srv.write_addr - XIP_BASE, efu_srv.partition_size);
            restore_interrupts(save);
            printf("[EFU] Erase done, ret=%d consumed=%d\r\n", ret, consumed);

            // Send ACK for header
            send(sn, hdr, 2);
        }

        if (efu_srv.state == EFU_WRITING) {
            // Bounds check
            uint32_t base_offs   = efu_srv.write_addr - XIP_BASE;
            uint32_t flash_offs  = base_offs + efu_srv.total_written;
            uint32_t end = base_offs + efu_srv.partition_size;

            int data_len = ret - consumed;
            if (data_len <= 0) break;
            printf("write %d bytes at 0x%08x\n", data_len, flash_offs);


            if (flash_offs + (uint32_t)data_len > end) {
                printf("[EFU] Error: incoming image too large (%lu + %d > %u)\r\n",
                    efu_srv.total_written, data_len, efu_srv.partition_size);
                //ota_srv.complete = true;
                efu_srv.state = EFU_ERROR;
                send(sn, err, 2);
                disconnect(sn);
                break;
            }
            efu_srv.crc_calc = crc32_step(efu_srv.crc_calc,
                              efu_srv.buf + consumed,
                              (uint16_t)data_len);


            // printf("[EFU] Programming %d bytes at flash offset 0x%08x\n", data_len, flash_offs);
            // Program flash
            uint32_t ints = save_and_disable_interrupts();
            flash_range_program(flash_offs, efu_srv.buf + consumed, (size_t)data_len);
            restore_interrupts(ints);

            efu_srv.total_written += (uint32_t)data_len;


            if (efu_srv.total_written == efu_srv.expected_size) {
                printf("[EFU] Transfer complete (%u/%u bytes)\n", efu_srv.total_written, efu_srv.expected_size);
                
                // Send ACK back to PC after all packets received
                send(sn, ack, 2);
                printf("[EFU] Send OK\r\n");

            // efu_srv.complete = true;
                efu_srv.state = EFU_WAIT_FOR_CRC;
                efu_srv.crc_received = 0;
                // disconnect(sn);
                break;
            }
        }
        if (efu_srv.state == EFU_WAIT_FOR_CRC) {

            printf("[EFU] Wait for CRC, crc_received %u, ret %u bytes\n", efu_srv.crc_received, ret);

            while (efu_srv.crc_received < 4 && consumed < ret) {
                efu_srv.crc_buf[efu_srv.crc_received++] = efu_srv.buf[consumed++];
            }

            if (efu_srv.crc_received < 4) {
                printf("[EFU] Wait for CRC need more bytes, crc_received %u bytes\n", efu_srv.crc_received);
                // Need more bytes
                break;
            }

            efu_srv.expected_crc =
                ((uint32_t)efu_srv.crc_buf[0] << 24) |
                ((uint32_t)efu_srv.crc_buf[1] << 16) |
                ((uint32_t)efu_srv.crc_buf[2] <<  8) |
                ((uint32_t)efu_srv.crc_buf[3] <<  0);

            printf("[EFU] CRC expected: %08x, calculated: %08x\n",
                efu_srv.expected_crc, efu_srv.crc_calc);

            if (efu_srv.crc_calc != efu_srv.expected_crc) {
                printf("[EFU] CRC MISMATCH — update aborted\n");
                efu_srv.state = EFU_ERROR;
                efu_srv.complete = true;
                // Send NACK
                send(sn, err, 2);
                
                disconnect(sn);
                break;
            }

            printf("[EFU] Send CC\r\n");
            // CRC OK
            send(sn, crc, 2);

            efu_srv.state = EFU_DONE;
            efu_srv.complete = true;
            disconnect(sn);
            break;
        }

        break;

    case SOCK_CLOSE_WAIT:
        printf("[EFU] Socket closed early, transfered/expected (%lu/%lu bytes)\n", efu_srv.total_written, efu_srv.expected_size);
        // ota_srv.complete = true;
        disconnect(sn);
        efu_srv.state = EFU_ERROR;
        break;

    case SOCK_CLOSED:
        if (efu_srv.state == EFU_DONE &&
            efu_srv.complete &&
            !efu_srv.reboot_requested)
        {
            efu_srv.reboot_requested = true;
            printf("[EFU] CRC OK — rebooting for slot switch\n");
            // printf("[EFU] Rebooting to apply update...\n");

            int r = rom_reboot(REBOOT2_FLAG_REBOOT_TYPE_FLASH_UPDATE,
                            1000,
                            efu_srv.write_addr,
                            0);
            printf("[EFU] rom_reboot returned %d\n", r);

            while (1) tight_loop_contents();
        } else {

            if (efu_srv.state != EFU_ERROR) {
                printf("[EFU] Connection closed, reopening socket to listen again\r\n");
            } else {
                printf("[EFU] Error state, reopening socket to listen again\r\n");
            }
            efu_srv.state = EFU_IDLE;
            // reopen socket to listen again
            efu_server_init();
        }
        break;

    default:
        break;
    }
}
