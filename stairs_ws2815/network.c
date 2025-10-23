/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "network.h"


#include <port_common.h>
#include "wizchip_conf.h"
#include "wizchip_spi.h"
#include "loopback.h"
#include "socket.h"


#include "wizchip_conf.h"
#include "config.h"
#include "ws2815_control_dma_parallel.h"

// --- Network Configuration ---
static wiz_NetInfo g_net_info;

#define ETHERNET_BUF_MAX_SIZE (1024 * 2) // Send and receive cache size
#define _LOOPBACK_DEBUG_    // Enable LOOPBACK debug messages on USB
#define _DDP_DEBUG_         // Enable DDP debug messages on USB
#define _UDP_DEBUG_         // Enable UDP debug messages on USB

// Do not cross this value: _WIZCHIP_SOCK_NUM_ = 8
#define TCP_LOOPBACK_SOCKET  0
#define UDP_DDP_SOCKET   7

// --- DDP constants ---
#define DDP_FLAGS1_PUSH   0x01    // bit0 = PUSH (render immediately)
#define DDP_HEADER_LEN    10      // bytes in DDP header
#define MAX_DDP_PAYLOAD   (NUM_STRIPS * NUM_PIXELS * NUM_CHANNELS)
#define DDP_DATA_BUF_SIZE     (DDP_HEADER_LEN + MAX_DDP_PAYLOAD)  // clamp to your RAM


// --- Variables for Network ---
// TCP loopback buffer
static uint8_t ethernet_buf[ETHERNET_BUF_MAX_SIZE] = { 0, };
// DDP receive buffer
static uint8_t ddp_ethernet_buf[DDP_DATA_BUF_SIZE] = { 0, };


static uint8_t* msg_ip_v4 = (uint8_t*)"IPv4 mode";  // the same: static const uint8_t msg_v4[]   = "IPv4 mode";
static uint8_t* msg_ip_v6 = (uint8_t*)"IPv6 mode";
static uint8_t* msg_ip_dual = (uint8_t*)"Dual IP mode";
static uint8_t loopback_mode = AS_IPV4;

// ------------------- Framebuffer for patterns ------------------
// uint8_t framebuf[NUM_STRIPS][NUM_PIXELS][NUM_CHANNELS];   // fb[strip][pixel][channel], channel = G,R,B
uint8_t rx_fb[NUM_STRIPS*NUM_PIXELS*NUM_CHANNELS]; // flat rx buffer for UDP DDP packets

void init_net_info(void) {
    uint8_t mac[6] = NETINFO_MAC;
    uint8_t ip[4]  = NETINFO_IP;
    uint8_t sn[4]  = NETINFO_SN;
    uint8_t gw[4]  = NETINFO_GW;
    uint8_t dns[4] = NETINFO_DNS;

    memcpy(g_net_info.mac, mac, sizeof(mac));
    memcpy(g_net_info.ip, ip, sizeof(ip));
    memcpy(g_net_info.sn, sn, sizeof(sn));
    memcpy(g_net_info.gw, gw, sizeof(gw));
    memcpy(g_net_info.dns, dns, sizeof(dns));

#if _WIZCHIP_ > W5500
    uint8_t lla[16] = NETINFO_LLA;
    uint8_t gua[16] = NETINFO_GUA;
    uint8_t sn6[16] = NETINFO_SN6;
    uint8_t gw6[16] = NETINFO_GW6;
    uint8_t dns6[16] = NETINFO_DNS6;

    memcpy(g_net_info.lla, lla, sizeof(lla));
    memcpy(g_net_info.gua, gua, sizeof(gua));
    memcpy(g_net_info.sn6, sn6, sizeof(sn6));
    memcpy(g_net_info.gw6, gw6, sizeof(gw6));
    memcpy(g_net_info.dns6, dns6, sizeof(dns6));
    g_net_info.ipmode = NETINFO_IPMODE;
#else
    g_net_info.dhcp = NETINFO_DHCP;
#endif

    network_initialize(g_net_info); // configures IP address etc.
    print_network_information(g_net_info); // Read back the configuration information and print it
}

static void process_ddp_packet(uint8_t *buf, uint16_t recv_len);

/*
    How it works:
    1. Create one listening socket on port 8000:
       socket(0, Sn_MR_TCP, 8000, 0);
       listen(0);
    2. Periodically poll socket status with getSn_SR()
        Depending on socket status:
       - If socket is in ESTABLISHED state, check for incoming data:
         recv() data into buffer, then send() it back to sender
       - If socket is in CLOSE_WAIT state, call disconnect()
       - If socket is in INIT state, call listen()
       - If socket is in CLOSED state, call socket() to open it again
    3. To create multi-client support on one port see example in multi_client
*/

/**
 * TCP loopback server on multiple sockets
 */
// int32_t loopback_tcps_multi_socket(uint8_t *msg) {
int32_t loopback_loop(uint8_t *msg) {
    uint8_t *buf = ethernet_buf;
    uint16_t port = TCP_LOOPBACK_PORT;
    int8_t sn = TCP_LOOPBACK_SOCKET;
    int32_t ret;
    uint16_t size = 0, sentsize = 0;
    uint8_t destip[4];
    uint16_t destport;
    // uint16_t multi_socket_port = port + sn ;     // I removed multi_socket working on different ports for simplicity
    // printf("socket %d : status %d\r\n",sn,getSn_SR(sn));
    switch (getSn_SR(sn)) {
    case SOCK_ESTABLISHED:
        if (getSn_IR(sn) & Sn_IR_CON) {
            getSn_DIPR(sn, destip);
            destport = getSn_DPORT(sn);

            printf("%d:Connected - %d.%d.%d.%d : %d\r\n", sn, destip[0], destip[1], destip[2], destip[3], destport);
            setSn_IR(sn, Sn_IR_CON);
        }
        if ((size = getSn_RX_RSR(sn)) > 0) { // Don't need to check SOCKERR_BUSY because it doesn't not occur.
            if (size > ETHERNET_BUF_MAX_SIZE) {
                size = ETHERNET_BUF_MAX_SIZE;
            }
            ret = recv(sn, buf, size);
            if (ret <= 0) {
                return ret;    // check SOCKERR_BUSY & SOCKERR_XXX. For showing the occurrence of SOCKERR_BUSY.
            }
            size = (uint16_t)ret;
            buf[size] = 0x00;

            sentsize = 0;
            while (size != sentsize) {
                ret = send(sn, buf + sentsize, size - sentsize);
                if (ret < 0) {
                    close(sn);
                    return ret;
                }
                sentsize += ret; // Don't care SOCKERR_BUSY, because it is zero.
            }
            getSn_DIPR(sn, destip);
            destport = getSn_DPORT(sn);
            *msg = *buf;    // transfer first byte of message for external use
            printf("socket%d from:%d.%d.%d.%d port: %d  message:%s\r\n", sn, destip[0], destip[1], destip[2], destip[3], destport, buf);
        }
        break;
    case SOCK_CLOSE_WAIT:
        #ifdef _LOOPBACK_DEBUG_
        printf("%d:CloseWait\r\n",sn);
        #endif
        if ((ret = disconnect(sn)) != SOCK_OK) {
            return ret;
        }
        #ifdef _LOOPBACK_DEBUG_
        printf("%d:Socket Closed\r\n", sn);
        #endif
        break;
    case SOCK_INIT:
        #ifdef _LOOPBACK_DEBUG_
        printf("%d:Listen, TCP server loopback, port [%d]\r\n", sn, port);
        #endif
        if ((ret = listen(sn)) != SOCK_OK) {
            return ret;
        }
        break;
    case SOCK_CLOSED:
        #ifdef _LOOPBACK_DEBUG_
        printf("%d:TCP server loopback start\r\n",sn);
        #endif
        if ((ret = socket(sn, Sn_MR_TCP, port, Sn_MR_ND)) != sn) {
            return ret;
        }
        #ifdef _LOOPBACK_DEBUG_
        printf("%d:Socket opened\r\n",sn);
        #endif
        break;
    default:
        break;
    }

    // if (sn >= (_WIZCHIP_SOCK_NUM_  - 1)) {
    //     sn = 0;
    // } else {
    //     sn ++;
    // }

    return 1;
}


/*
    How TCP loopback works:
0:TCP server loopback start
0:Socket opened
0:Listen, TCP server loopback, port [8000]
0:Connected - 192.168.178.98 : 54201
Pattern 6=Jaremek dir:(forward)
0:CloseWait
0:Socket Closed
0:TCP server loopback start
0:Socket opened
0:Listen, TCP server loopback, port [8000]

0:Connected - 192.168.178.98 : 60796
Pattern 6=Jaremek dir:(backward)
socket0 from:192.168.178.98 port: 60796  message:e
socket0 from:192.168.178.98 port: 60796  message:w
0:CloseWait
0:Socket Closed
0:TCP server loopback start
0:Socket opened
0:Listen, TCP server loopback, port [8000]


With UDP:

WS2815 initialized.
0:TCP server loopback start
0:Socket opened
7:UDP server DDP start
7:Socket opened
0:Listen, TCP server loopback, port [8000]
7:UDP socket data processing
-- blocked

*/

/* 
    // --- Open UDP socket for DDP ---
    SOCKET ddp_sock = 0;
    uint8_t status;
    if (socket(ddp_sock, Sn_MR_UDP, UDP_DDP_PORT, 0) != ddp_sock) {
        printf("UDP socket open failed\n");
        while (1) tight_loop_contents();
    }
    printf("DDP UDP listening on port %d\n", UDP_DDP_PORT);




        // Poll the UDP socket
        status = getSn_SR(ddp_sock);
        if (status == SOCK_UDP) {
            process_ddp_udp(ddp_sock, &pkt_counter, &last_push_ms);
        } else if (status == SOCK_CLOSED) {
            // re-open if closed
            socket(ddp_sock, Sn_MR_UDP, UDP_DDP_PORT, 0);
        }
 */

/**
 * UDP DDP server loop
 */
int32_t ddp_loop(uint32_t *pkt_counter, uint32_t *last_push_ms) {
    // int32_t loopback_udps(uint8_t sn, uint8_t* buf, uint16_t port)
    check_loopback_mode_W6x00();    // as default set to AS_IPV4
    uint8_t *rx_buf = ddp_ethernet_buf;
    uint16_t port = UDP_DDP_PORT;
    SOCKET ddp_sock = UDP_DDP_SOCKET;

    uint8_t status;
    static uint8_t srcip[16] = {0,};    // originally destip[16]
    static uint16_t srcport;           // originally destport
    // uint8_t pack_info;
    uint8_t addr_len;
    int32_t ret;
    uint16_t received_size;
    // uint16_t size, sentsize;
    uint8_t* mode_msg;



    if(loopback_mode == AS_IPV4)
        mode_msg = msg_ip_v4;
    else if(loopback_mode == AS_IPV6)
        mode_msg = msg_ip_v6;
    else
        mode_msg = msg_ip_dual;

    // Query socket status the WIZnet way
    getsockopt(ddp_sock, SO_STATUS, &status);
    switch(status)
    {
    case SOCK_UDP:
        uint16_t test_loop_counter = 0;
        do {
            getsockopt(ddp_sock, SO_RECVBUF, &received_size);   // = getSn_RX_RSR(ddp_sock);
            if(received_size == 0) break;
            test_loop_counter++;
            if(received_size > DATA_BUF_SIZE) received_size = DATA_BUF_SIZE;
        
            // int32_t n = recvfrom(ddp_sock, rx_buf, received_size, srcip, &srcport, &addr_len);
            // ret = recvfrom(ddp_sock, rx_buf, received_size, (uint8_t*)&destip, (uint16_t*)&destport, &addr_len);
            ret = recvfrom(ddp_sock, rx_buf, received_size, (uint8_t *)&srcip[0], (uint16_t *)&srcport, &addr_len);

            if(ret <= 0) {
                printf("DDP recvfrom error: %ld\r\n", ret);
                return ret;
            }
            #ifdef _UDP_DEBUG_
            printf("UDP packet received: %ld bytes from %d.%d.%d.%d:%d\r\n", ret,
                srcip[0], srcip[1], srcip[2], srcip[3], srcport);
            #endif 

            if (ret > 0) {
                // simplified calculation without checking for DDP header correctness
                (*pkt_counter)++;
                // simplified time tag for last push instead of full timestamp when ws2815_show(rx_fb);
                *last_push_ms = to_ms_since_boot(get_absolute_time());

                process_ddp_packet(rx_buf, (uint16_t)ret);
                // old: process_ddp_udp(ddp_sock, pkt_counter, last_push_ms);

                // Send back "OK" response, for testing purposes
                // sendto(ddp_sock, (uint8_t*)"OK", 2, srcip, srcport, addr_len);
            }

            // --- part for sending back received data - not needed for DDP ---
            // received_size = (uint16_t) ret;
            // sentsize = 0;
            // while(sentsize != received_size){
            //     ret = sendto(ddp_sock, buf+sentsize, received_size-sentsize, destip, destport, addr_len);

            //     if(ret < 0) return ret;

            //     sentsize += ret; // Don't care SOCKERR_BUSY, because it is zero.
            //  }

        // } while(getSn_RX_RSR(ddp_sock) > 0);
        } while(0);

        if (test_loop_counter > 0) {
            #ifdef _DDP_DEBUG_
            printf("DDP packets processed in loop: %d.\r\n", test_loop_counter);
            #endif
        }  
        break;

    case SOCK_CLOSED:
        SOCKET sn;
        switch(loopback_mode)
        {
        case AS_IPV4:
           sn = socket(ddp_sock, Sn_MR_UDP4, port, SOCK_IO_NONBLOCK);
           break;
        case AS_IPV6:
           sn = socket(ddp_sock, Sn_MR_UDP6, port, SOCK_IO_NONBLOCK);
           break;
        case AS_IPDUAL:
           sn = socket(ddp_sock, Sn_MR_UDPD, port, SOCK_IO_NONBLOCK);
            break;
        }
        if(sn != ddp_sock){    /* reinitialize the socket */
            #ifdef _UDP_DEBUG_
                printf("%d : Fail to create socket.\r\n",ddp_sock);
            #endif
            return SOCKERR_SOCKNUM;
        }
        #ifdef _UDP_DEBUG_
            printf("%d:Socket UDP opened, port [%d] as %s\r\n",ddp_sock, port, mode_msg);   // getSn_SR(ddp_sock)
            // printf("%d:Opened, UDP loopback, port [%d] as %s\r\n", ddp_sock, port, mode_msg);
        #endif

    }

    return 0;
}



/**
 * Copy helper: write DDP payload into framebuffer
 */

static void ddp_copy_payload(const uint8_t *payload, uint32_t offset, uint32_t length)
{
    int32_t in, out;

    for(in = 0, out = offset; in < length; in++, out++) {
        rx_fb[out] = payload[in];
    }

    // uint32_t led_start = offset / NUM_CHANNELS;
    // uint32_t led_end   = (offset + length) / NUM_CHANNELS;
    // if (led_end > NUM_STRIPS * NUM_PIXELS)
    //     led_end = NUM_STRIPS * NUM_PIXELS;

    // uint32_t dst = led_start;
    // for (uint32_t i = 0; i < length && dst < led_end; i += NUM_CHANNELS, dst++) {
    //     uint32_t strip = dst / NUM_PIXELS;
    //     uint32_t pixel = dst % NUM_PIXELS;
    //     for (int c = 0; c < NUM_CHANNELS; c++)
    //         framebuf[strip][pixel][c] = payload[i + c];
    // }

    // Option 2
    // uint32_t led_start = offset / NUM_CHANNELS;
    // uint32_t led_end   = (offset + length) / NUM_CHANNELS;
    // if (led_end > NUM_STRIPS * NUM_PIXELS) led_end = NUM_STRIPS * NUM_PIXELS;

    // uint32_t dst = led_start;
    // for (uint32_t i = 0; i + NUM_CHANNELS <= length && dst < led_end; i += NUM_CHANNELS, dst++) {
    //     uint32_t strip = dst / NUM_PIXELS;
    //     uint32_t pixel = dst % NUM_PIXELS;
    //     for (int c = 0; c < NUM_CHANNELS; c++)
    //         framebuf[strip][pixel][c] = payload[i + c];
    // }
}



/**
 * UDP receive and process DDP packets (DDP parser)
 */
/* 
void process_ddp_udp(SOCKET s, uint32_t *pkt_counter, uint32_t *last_push_ms)
{
    uint8_t buf[MAX_DDP_PAYLOAD + DDP_HEADER_LEN];
    uint16_t recv_len = 0;
    uint8_t  srcip[4];
    uint16_t srcport;

    printf("DDP UDP packet processing...\r\n");
    if ((recv_len = recvfrom(s, buf, sizeof(buf), srcip, &srcport)) > DDP_HEADER_LEN) {
        (*pkt_counter)++;
        printf("DDP Packet #%lu from %d.%d.%d.%d:%d, length %d bytes\r\n",
               *pkt_counter,
               srcip[0], srcip[1], srcip[2], srcip[3],
               srcport,
               recv_len);

        // --- Parse header ---
        // Byte layout (big-endian):
        //  0: flags1
        //  1: flags2
        //  2-3: data type (usually 0x00)
        //  4-7: data offset (uint32)
        //  8-9: data length (uint16)
        uint8_t  flags1 = buf[0];
        uint32_t offset = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
        uint16_t length = (buf[8] << 8) | buf[9];

        if (length > recv_len - DDP_HEADER_LEN)
            length = recv_len - DDP_HEADER_LEN;   // safety

        ddp_copy_payload(&buf[DDP_HEADER_LEN], offset, length);

        // if PUSH bit set → render
        if (flags1 & DDP_FLAGS1_PUSH) {
            ws2815_show(rx_fb);   // push frame to LEDs (pass flat uint8_t pointer)
            *last_push_ms = to_ms_since_boot(get_absolute_time());
        }
    }
}
 */

static void process_ddp_packet(uint8_t *buf, uint16_t recv_len)
{
    if (recv_len <= DDP_HEADER_LEN) return;

    uint8_t  flags1 = buf[0];
    uint32_t offset = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
                      ((uint32_t)buf[6] << 8)  |  (uint32_t)buf[7];
    uint16_t length = ((uint16_t)buf[8] << 8) | (uint16_t)buf[9];

    // Clamp to actual received payload
    uint16_t payload_len = recv_len - DDP_HEADER_LEN;
    if (length > payload_len) length = payload_len;

    ddp_copy_payload(&buf[DDP_HEADER_LEN], offset, length);

    if (flags1 & DDP_FLAGS1_PUSH) {
        // ws2815_show((uint8_t*)framebuf);  // push to LEDs
        ws2815_show(rx_fb);   // push frame to LEDs (pass flat uint8_t pointer)
    }
}
