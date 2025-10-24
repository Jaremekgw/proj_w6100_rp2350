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

// UDP receive buffer for interrupt processing
#define UDP_RING_COUNT      5
#define UDP_RX_BUF_SZ       1500
static uint8_t udp_rx_ring_buf[UDP_RING_COUNT][UDP_RX_BUF_SZ];
static volatile uint8_t udp_ring_idx = 0;
static volatile uint16_t udp_rx_buf_len[UDP_RING_COUNT] = {0,};

// // Optional: lightweight flag to defer heavy work out of ISR
// static volatile bool wiznet_rx_pending = false;
// Optional: lightweight flag to defer heavy work out of ISR
static volatile bool wiznet_rx_pending = false;
// network.h
// static inline void wiznet_service_if_needed(void) {
//     if (!wiznet_rx_pending) return;
//     wiznet_rx_pending = false;
//     wiznet_drain_udp();     // or whatever the actual implementation is
// }

static uint8_t* msg_ip_v4 = (uint8_t*)"IPv4 mode";  // the same: static const uint8_t msg_v4[]   = "IPv4 mode";
static uint8_t* msg_ip_v6 = (uint8_t*)"IPv6 mode";
static uint8_t* msg_ip_dual = (uint8_t*)"Dual IP mode";
static uint8_t loopback_mode = AS_IPV4;

// --- WIZnet W6100 interrupt pin ---
#define WIZNET_INT_PIN   21

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

/* 
void wiznet_irq_handler(void) {
    // Handle WIZnet interrupts here if needed
    // This function should be called from the main interrupt handler
    uint8_t ir;

    ir = getSn_IR(UDP_DDP_SOCKET);
    if (ir & Sn_IR_RECV) {
        // Data received on UDP DDP socket
        poll_udp_ddp_socket();
        setSn_IR(UDP_DDP_SOCKET, Sn_IR_RECV); // Clear interrupt
    }
}
 */

void wiznet_interrupts_enable(void) {
    //SOCKET ddp_sock = UDP_DDP_SOCKET;
    intr_kind imr, ir;

    // Enable socket 7 interrupt in the global mask (SIMR)
    // You can enable just the sockets you use to avoid extra wakeups.
    //imr = IK_SOCK_7;    // Interrupt mask for UDP DDP socket
    //ctlwizchip(CW_SET_INTRMASK, &imr);  // wizchip_setinterruptmask(*((intr_kind*)arg));
    // (Optional) also enable network-level interrupts if you want them:
    imr = IK_SOCK_7 | IK_NET_ALL;
    ctlwizchip(CW_SET_INTRMASK, &imr);

    // Clear ALL pending (global + per-socket + service-level) to start clean
    ir = IK_INT_ALL;
    ctlwizchip(CW_CLR_INTERRUPT, &ir);  // wizchip_clrinterrupt(*((intr_kind*)arg));
}


/* It works, but I removed it to save space
static char event_str[128];
static const char *gpio_irq_str[] = {
        "LEVEL_LOW",  // 0x1
        "LEVEL_HIGH", // 0x2
        "EDGE_FALL",  // 0x4
        "EDGE_RISE"   // 0x8
};
void gpio_event_string(char *buf, uint32_t events) {
    for (uint i = 0; i < 4; i++) {
        uint mask = (1 << i);
        if (events & mask) {
            // Copy this event string into the user string
            const char *event_str = gpio_irq_str[i];
            while (*event_str != '\0') {
                *buf++ = *event_str++;
            }
            events &= ~mask;

            // If more events add ", "
            if (events) {
                *buf++ = ',';
                *buf++ = ' ';
            }
        }
    }
    *buf++ = '\0';
}
 */

static volatile uint32_t time_irq_start = 0, time_routine_start, time_irq_duration, time_routine_duration;
static volatile uint32_t t_irq_routine_start, time_irq_routine_duration, time_irq_routine_max = 0;
static volatile uint8_t irq_loop_count = 0, drain_loop_count = 0;
static volatile uint8_t irq_packet_count = 0, irq_error_count = 0;

#define TABLE_IRQ_SIZE 12
static volatile uint32_t time_table_irq[TABLE_IRQ_SIZE];
static volatile uint8_t time_table_idx = 0;
static volatile uint32_t time_table_duration[TABLE_IRQ_SIZE];
static volatile uint8_t time_duration_idx = 0;

static volatile uint16_t received_size_table[TABLE_IRQ_SIZE];
static volatile uint8_t received_size_table_idx = 0;

// This will be called when INTn (GPIO 21) goes low
void wiznet_gpio_irq_handler_previous(uint gpio, uint32_t events) {
    // Put the GPIO event(s) that just happened into event_str
    // so we can print it
    //gpio_event_string(event_str, events);
    //printf("GPIO %d %s\n", gpio, event_str);

    // don't use it: gpio_set_irq_enabled(WIZNET_INT_PIN, GPIO_IRQ_EDGE_FALL, false);  // disable further interrupts until serviced
    time_irq_start = time_us_32();
    // Keep it tiny — just flag work for main loop
    wiznet_rx_pending = true;
}

void wiznet_gpio_irq_handler(uint gpio, uint32_t events)
{
    SOCKET ddp_sock = UDP_DDP_SOCKET;
    uint16_t recv_len;
    uint8_t srcip[16];
    uint8_t addr_len;
    uint16_t srcport;
    int32_t ret;

    t_irq_routine_start = time_us_32();
    time_table_irq[time_table_idx] = t_irq_routine_start - time_irq_start;
    time_table_idx++;
    if (time_table_idx >= TABLE_IRQ_SIZE)
        time_table_idx = 0;
    time_irq_start = t_irq_routine_start;

    // Loop while data waiting (drain completely to re-arm INTn)
    //while (1) {
    irq_loop_count++;
    getsockopt(ddp_sock, SO_RECVBUF, &recv_len);
    received_size_table[received_size_table_idx++] = recv_len;
    if (received_size_table_idx >= TABLE_IRQ_SIZE)
        received_size_table_idx = 0;

    if (recv_len > 0) {    // if (recv_len == 0) break;  // no more packets pending

        irq_packet_count++;

        if (recv_len > UDP_RX_BUF_SZ)
            recv_len = UDP_RX_BUF_SZ; // clamp to our ring slot size

        // --- Copy packet directly from W6100 RX buffer into ring slot ---
        ret = recvfrom(ddp_sock,
                       udp_rx_ring_buf[udp_ring_idx],
                       recv_len,
                       (uint8_t *)&srcip[0],
                       (uint16_t *)&srcport,
                       &addr_len);

        if (ret <= 0) {
            irq_error_count++;
            // break;  // error or empty
        } else {
            // Successful receive
            // store packet length for later processing
            udp_rx_buf_len[udp_ring_idx] = (uint16_t)ret;

            // advance ring index (wrap around)
            udp_ring_idx++;
            if (udp_ring_idx >= UDP_RING_COUNT)
                udp_ring_idx = 0;
        }
    } else {
        irq_error_count++;
    }
    //} // end while

    time_irq_routine_duration = time_us_32() - t_irq_routine_start;
    if (time_irq_routine_duration > time_irq_routine_max)
        time_irq_routine_max = time_irq_routine_duration;
    time_table_duration[time_duration_idx++] = time_irq_routine_duration;
    if (time_duration_idx >= TABLE_IRQ_SIZE)
        time_duration_idx = 0;

    // --- Clear interrupts *after* draining RX buffer ---
    setSn_IR(ddp_sock, Sn_IR_RECV);     // clear socket RECV flag
    wizchip_clrinterrupt(IK_SOCK_7);    // clear global summary

    // mark that data exists for main loop
    wiznet_rx_pending = true;
}




void wiznet_gpio_irq_init(void) {
    gpio_init(WIZNET_INT_PIN);
    gpio_set_dir(WIZNET_INT_PIN, GPIO_IN);
    gpio_pull_up(WIZNET_INT_PIN);  // INTn is active low
    // Register the handler
    gpio_set_irq_enabled_with_callback(
        WIZNET_INT_PIN,
        GPIO_IRQ_EDGE_FALL,
        true,
        &wiznet_gpio_irq_handler
    );

    // // lambda version (C++11 style) - does not compile in C11
    // gpio_set_irq_enabled_with_callback(
    //     WIZNET_INT_PIN,
    //     GPIO_IRQ_EDGE_FALL,
    //     true,
    //     [](uint gpio, uint32_t events){
    //         // Minimal work in ISR: mark pending and return fast.
    //         wiznet_rx_pending = true;
    //     }
    // );
}


void udp_socket_init(void) {
    uint16_t port = UDP_DDP_PORT;
    SOCKET ddp_sock = UDP_DDP_SOCKET;
    uint8_t* mode_msg;

    check_loopback_mode_W6x00();    // as default set to AS_IPV4

    if(loopback_mode == AS_IPV4)
        mode_msg = msg_ip_v4;
    else if(loopback_mode == AS_IPV6)
        mode_msg = msg_ip_v6;
    else
        mode_msg = msg_ip_dual;


    SOCKET sn = ddp_sock;
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
            printf("%d : Fail to create socket.\r\n", ddp_sock);
        #endif
        return; // SOCKERR_SOCKNUM;
    }
    #ifdef _UDP_DEBUG_
        printf("%d:Socket UDP opened, port [%d] as %s\r\n",ddp_sock, port, mode_msg);   // getSn_SR(ddp_sock)
        // printf("%d:Opened, UDP loopback, port [%d] as %s\r\n", ddp_sock, port, mode_msg);
    #endif

    // Per-socket: enable only RECV interrupt
    // Sn_IMR bits: Sn_IR_SENDOK(0x10), Sn_IR_TIMEOUT(0x08), Sn_IR_RECV(0x04), ...
    setSn_IMR(ddp_sock, Sn_IR_RECV);     // mask bit RECV=1
    // Clear any pending per-socket interrupts
    setSn_IR(ddp_sock, 0xFF);
}



void process_udp_ring(void) {

    uint8_t rd_idx = (udp_ring_idx + 1) % UDP_RING_COUNT;

    for (uint8_t id = 0; id < UDP_RING_COUNT; id++) {
        // printf("UDP ring slot %d: len=%d\n", id, udp_rx_buf_len[id]);
        if (udp_rx_buf_len[rd_idx] != 0) {
            drain_loop_count++;
            process_ddp_packet(udp_rx_ring_buf[rd_idx], udp_rx_buf_len[rd_idx]);
            udp_rx_buf_len[rd_idx] = 0;  // mark slot free
        }
        rd_idx = (rd_idx + 1) % UDP_RING_COUNT;
    }
}



void wiznet_drain_udp_previous(void) {
    SOCKET ddp_sock = UDP_DDP_SOCKET;
    uint16_t received_size;
    uint8_t addr_len;
    uint8_t *rx_buf = ddp_ethernet_buf;

    // Check which interrupts are pending globally
    intr_kind pending = (intr_kind)0;
    pending = wizchip_getinterrupt();

    // If our socket signaled, service it
    if (pending & IK_SOCK_7) {
        // Per-socket IR tells us the reasons
        uint8_t sn_ir = getSn_IR(ddp_sock);

        if (sn_ir & Sn_IR_RECV) {
            // Clear socket RECV latch before draining to re-arm INTn fast
            // 1. clear per-socket RECV latch first
            //setSn_IR(ddp_sock, Sn_IR_RECV);
            // 2. then clear the global interrupt bit
            //wizchip_clrinterrupt(IK_SOCK_7);    // setSn_IRCLR(i,0xFF); setSLIRCLR(slir);
            // 3. finally re-enable GPIO interrupt if you disable it in the handler

            // Drain all queued UDP datagrams
            while (1) {
                // uint16_t avail = 0;
                // getsockopt(ddp_sock, SO_RECVBUF, &avail);
                // drain_loop_count++;
                getsockopt(ddp_sock, SO_RECVBUF, &received_size);   // = getSn_RX_RSR(ddp_sock);
                if (!received_size) break;
                // test_loop_counter++;
                if(received_size > DATA_BUF_SIZE) received_size = DATA_BUF_SIZE;

                uint8_t  srcip[4];
                uint16_t srcport;
                int32_t ret;
                
                // ret = recvfrom(ddp_sock, rx_buf, RXBUF_SZ, srcip, &srcport);
                ret = recvfrom(ddp_sock, rx_buf, received_size, (uint8_t *)&srcip[0], (uint16_t *)&srcport, &addr_len);


                if(ret <= 0) {
                    printf("DDP recvfrom error: %ld\r\n", ret);
                    break;
                }
                #ifdef _UDP_DEBUG_
                printf("UDP packet received: %ld bytes from %d.%d.%d.%d:%d\r\n", ret,
                    srcip[0], srcip[1], srcip[2], srcip[3], srcport);
                #endif 

                if (ret > 0) {
                    // Your existing DDP parse:
                    // process_ddp_packet(rx_buf, (uint16_t)n);
                    process_ddp_packet(rx_buf, (uint16_t)ret);
                }
            }
        //} else {
        //    // Clear the global bit for this socket
        //    wizchip_clrinterrupt(IK_SOCK_7);    // setSn_IRCLR(i,0xFF); setSLIRCLR(slir);
        }
        // Clear socket RECV latch before draining to re-arm INTn fast
        // 1. clear per-socket RECV latch first
        setSn_IR(ddp_sock, Sn_IR_RECV);
        // 2. then clear the global interrupt bit
        wizchip_clrinterrupt(IK_SOCK_7);    // setSn_IRCLR(i,0xFF); setSLIRCLR(slir);
        // 3. finally re-enable GPIO interrupt if you disable it in the handler

        // Clear the global bit for this socket
        // wizchip_clrinterrupt(IK_SOCK_7);    // setSn_IRCLR(i,0xFF); setSLIRCLR(slir);
    }
}

static inline void wiznet_service_if_needed(void) {
    if (!wiznet_rx_pending) return;
    wiznet_rx_pending = false;

    time_irq_duration = time_us_32() - time_irq_start;
    time_routine_start = time_us_32();
    // printf("WIZnet IRQ serviced in %u us\n", time_irq_duration);
    // wiznet_drain_udp();
    process_udp_ring();
    time_routine_duration = time_us_32() - time_routine_start;
    printf("WIZnet service, irq_count:%d, pck_cnt:%d, err_cnt:%d,  irq_last:%u, irq_max:%u, pending:%u, routine:%u us, drain_count:%d\n", \
           irq_loop_count, irq_packet_count, irq_error_count,
           time_irq_routine_duration, time_irq_routine_max, time_irq_duration, time_routine_duration, drain_loop_count);
    // don't use it: gpio_set_irq_enabled(WIZNET_INT_PIN, GPIO_IRQ_EDGE_FALL, true);  // re-enable interrupts
    irq_loop_count = 0;
    drain_loop_count = 0;
    irq_packet_count = 0;
    irq_error_count = 0;
                //     static volatile uint32_t time_table_irq[TABLE_IRQ_SIZE];
                // static volatile uint8_t time_table_idx = 0;

                // static volatile uint16_t received_size_table[TABLE_IRQ_SIZE];
                // static volatile uint8_t received_size_table_idx = 0;
    for (uint8_t i = 0; i < time_table_idx; i++) {
        printf("    IRQ time[%d]: %u us\n", i, time_table_irq[i]);
    }
    //  time_table_duration[TABLE_IRQ_SIZE];
//  static volatile uint8_t time_duration_idx = 0;
    for (uint8_t i = 0; i < time_duration_idx; i++) {
        printf("    IRQ duration[%d]: %u us\n", i, time_table_duration[i]);
    }
    time_duration_idx = 0;
    for (uint8_t i = 0; i < received_size_table_idx; i++) {
        printf("    IRQ RX size[%d]: %u bytes\n", i, received_size_table[i]);
    }
    received_size_table_idx = 0;
    time_table_idx = 0;
}



/**
 * UDP DDP server loop
 */
// int32_t ddp_loop(uint32_t *pkt_counter, uint32_t *last_push_ms) {
int32_t ddp_loop(void) {

    wiznet_service_if_needed();
    return 0;
/* 
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
            setSn_IR(ddp_sock, Sn_IR_RECV); // Clear RECV interrupt

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
                //(*pkt_counter)++;
                // simplified time tag for last push instead of full timestamp when ws2815_show(rx_fb);
                //*last_push_ms = to_ms_since_boot(get_absolute_time());

                process_ddp_packet(rx_buf, (uint16_t)ret);
                // old: process_ddp_udp(ddp_sock, pkt_counter, last_push_ms);

                // Send back "OK" response, for testing purposes
                // sendto(ddp_sock, (uint8_t*)"OK", 2, srcip, srcport, addr_len);
            }
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
        if(sn != ddp_sock){    // reinitialize the socket
            #ifdef _UDP_DEBUG_
                printf("%d : Fail to create socket.\r\n",ddp_sock);
            #endif
            return SOCKERR_SOCKNUM;
        }
        #ifdef _UDP_DEBUG_
            printf("%d:Socket UDP opened, port [%d] as %s\r\n",ddp_sock, port, mode_msg);   // getSn_SR(ddp_sock)
            // printf("%d:Opened, UDP loopback, port [%d] as %s\r\n", ddp_sock, port, mode_msg);
        #endif

        // Per-socket: enable only RECV interrupt
        // Sn_IMR bits: Sn_IR_SENDOK(0x10), Sn_IR_TIMEOUT(0x08), Sn_IR_RECV(0x04), ...
        setSn_IMR(ddp_sock, Sn_IR_RECV);     // mask bit RECV=1
        // Clear any pending per-socket interrupts
        setSn_IR(ddp_sock, 0xFF);
        break;

    }

    return 0;
 */
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
