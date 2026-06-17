/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * 
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "network.h"
#include "time.h"

#include "wizchip_conf.h"

#define _UDP_DEBUG_

// --- DBG Variables ---
static uint8_t dbg_sn = 0xff; // socket number for debug over UDP, set during initialization
static uint16_t dbg_port = 0; // port number for debug over UDP, set during initialization
// Buffer used for debug UDP packets
static uint8_t *dbg_buf_frame = NULL;  // separate pointer for debug buffer, can be used for other purposes if needed
static uint16_t dbg_buf_size = 0;  // size of the debug buffer, set during initialization, should not exceed

// --- Data from client ---
static uint8_t dbg_client_ip[16];
static uint16_t dbg_client_port;
static uint32_t dbg_client_last_seen;
static uint8_t dbg_client_valid = 0;
#define DBG_CLIENT_TIMEOUT_MS 10000

/* ---------- helpers ---------- */
static inline uint32_t now_ms(void)
{
    return (uint32_t)to_ms_since_boot(get_absolute_time());
}

/**
 * Initialize UDP for DEBUG protocol
 */
void dbg_server_init(uint8_t sn, uint16_t port, uint8_t *buf, uint16_t buf_size) {
    uint8_t protocol;
    // intr_kind sock_bit;
    int8_t rc;

    // set global variables for DDP socket and buffer
    dbg_sn = sn;
    dbg_port = port;
    dbg_buf_frame = buf;
    dbg_buf_size = buf_size;

    // #ifdef _UDP_DEBUG_
    uint8_t* mode_msg;
    // #endif

    protocol = get_loopback_mode(&mode_msg);

    // sock_bit = (IK_SOCK_0 << dbg_sn);
    rc = socket((uint32_t)dbg_sn, protocol, dbg_port, SOCK_IO_NONBLOCK);
    if (rc < 0) return;
    sn = (typeof(sn))rc;

    if(sn != dbg_sn){    /* reinitialize the socket */
        #ifdef _UDP_DEBUG_
            printf("[DBG] Fail to create socket: %d\r\n", dbg_sn);
        #endif
        return; // SOCKERR_SOCKNUM;
    }
    #ifdef _UDP_DEBUG_
        printf("[DBG] Socket %d UDP opened, port [%d] as %s\r\n", dbg_sn, dbg_port, mode_msg);   // getSn_SR(ddp_sock)
    #endif

}

/**
 * Poll the debug UDP socket for incoming messages, print them, and return status
 */
int dbg_server_poll(void)
{
    uint8_t sn = dbg_sn;
    uint16_t rx_size = getSn_RX_RSR(sn);

    if (rx_size == 0)
        return 0;

    if (rx_size >= dbg_buf_size)
        rx_size = dbg_buf_size - 1;

    uint8_t src_ip[16];
    uint16_t src_port;

    // recv() cannot give sender address, so we use recvfrom() instead for debug socket
    // int32_t ret = recv(sn, dbg_buf_frame, rx_size);
    int32_t ret = recvfrom(sn, dbg_buf_frame, rx_size, src_ip, &src_port);
    if (ret <= 0)
        return ret;

    // if (ret < dbg_buf_size)
    //      dbg_buf_frame[ret] = 0; // null-terminate for safe printing
    // else
    //     dbg_buf_frame[dbg_buf_size - 1] = 0; // ensure null-termination if buffer is full

    /* register debug client */
    memcpy(dbg_client_ip, src_ip, 4);
    dbg_client_port = src_port;
    dbg_client_last_seen = now_ms();
    dbg_client_valid = 1;

#ifdef _UDP_DEBUG_
    printf("[DBG] Client registered: %d.%d.%d.%d:%d\n",
           src_ip[0], src_ip[1], src_ip[2], src_ip[3], src_port);
#endif

    return 1;
}

static void dbg_check_timeout(uint32_t now_ms)
{
    if (!dbg_client_valid)
        return;

    if ((now_ms - dbg_client_last_seen) > DBG_CLIENT_TIMEOUT_MS) {
        dbg_client_valid = 0;

#ifdef _UDP_DEBUG_
        printf("[DBG] Client timeout\n");
#endif
    }
}

int dbg_send_sensor_data(uint32_t now_ms,
                         const void *data,
                         uint16_t size)
{
    uint8_t sn = dbg_sn;

    dbg_check_timeout(now_ms);

    if (!dbg_client_valid)
        return 0;

    int32_t ret = sendto(sn,
                         (uint8_t*)data,
                         size,
                         dbg_client_ip,
                         dbg_client_port);

    if (ret < 0) {
#ifdef _UDP_DEBUG_
        printf("[DBG] sendto failed: %ld\n", ret);
#endif
        return ret;
    }

    return ret;
}

