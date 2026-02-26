/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * Disable sign-conversion warnings locally from WIZnet code
 * 
 * Or you can wrap function getSn_SR:
 * static inline uint32_t wiz_get_sn_sr(uint8_t sn)
 *   {
 *       return (uint32_t)getSn_SR(sn);
 *   }
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

#include <stdio.h>  
#include <string.h>
#include "network.h"


//#include <port_common.h>
#include "wizchip_conf.h"
#include "wizchip_spi.h"
#include "loopback.h"
#include "pico/time.h"
#include "hardware/gpio.h"

#include "pico/unique_id.h"
#include "wizchip_conf.h"
#include "flash_cfg.h"


#define _TIME_DEBUG_

// Do not cross this value: _WIZCHIP_SOCK_NUM_ = 8
//#define TCP_LOOPBACK_SOCKET  0      // with port TCP_LOOPBACK_PORT 8000
//#define UDP_DDP_SOCKET       5      // with port UDP_DDP_PORT 4048
// #define UDP_SOCKET_COUNT 3       // number of UDP sockets for DDP reception

// --- DDP constants ---
#define DDP_FLAGS1_PUSH   0x01    // bit0 = PUSH (render immediately)
#define DDP_HEADER_LEN    10      // bytes in DDP header
#define MAX_DDP_PAYLOAD   (NUM_STRIPS * NUM_PIXELS * NUM_CHANNELS)
#define DDP_DATA_BUF_SIZE     (DDP_HEADER_LEN + MAX_DDP_PAYLOAD)  // clamp to your RAM


// --- CLI Variables ---
// Buffer used for telnet
// static uint8_t ethernet_cli_buf[ETHERNET_BUF_MAX_SIZE + 1];
// static uint8_t *ethernet_cli_buf;

static uint8_t *cli_buf_rx = NULL;  // separate pointer for CLI RX buffer, can be used for other purposes if needed
 // ETHERNET_BUF_MAX_SIZE
static uint16_t cli_buf_size = 0;  // size of the CLI buffer, set during initialization, should not exceed
 // CLI_TIMEOUT_MS
static int64_t cli_timeout_ms = 30000;  // 30 seconds timeout for idle CLI connections
static uint8_t cli_sn = 0xff;
static uint16_t cli_port = 0;
static tcp_cli_hooks_t cli_hooks = {0}; // struct for CLI hooks, set during initialization


// --- DDP Variables ---
static uint8_t *ddp_buf_frame; // pointer to DDP payload area in the buffer for receiving DDP packets
static uint16_t ddp_buf_size = 0; // size of the DDP buffer, set during initialization, should not exceed DDP_DATA_BUF_SIZE
static uint8_t ddp_sn = 0xff; // UDP_DDP_SOCKET
static uint16_t ddp_port = 0;

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

#if _UDP_DEBUG_
static uint8_t* msg_ip_v4 = (uint8_t*)"IPv4 mode";  // the same: static const uint8_t msg_v4[]   = "IPv4 mode";
static uint8_t* msg_ip_v6 = (uint8_t*)"IPv6 mode";
static uint8_t* msg_ip_dual = (uint8_t*)"Dual IP mode";
#endif
static uint8_t loopback_mode = AS_IPV4;

// --- WIZnet W6100 interrupt pin ---
#define WIZNET_INT_PIN   21

// --- Framebuffer for patterns ---
// uint8_t framebuf[NUM_STRIPS][NUM_PIXELS][NUM_CHANNELS];   // fb[strip][pixel][channel], channel = G,R,B
// uint8_t rx_fb[NUM_PIXELS*NUM_CHANNELS]; // flat rx buffer for UDP DDP packets
// uint8_t rx_fb[NUM_STRIPS*NUM_PIXELS*NUM_CHANNELS]; // flat rx buffer for UDP DDP packets


// --- Unique ID ---
// pico_unique_board_id_t id;


// --- Functions ---
void init_net_info(void) {
    network_initialize(config_get_net_info()); // configures IP address etc.
    print_network_information(); // Read back the configuration information and print it
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
/* int32_t loopback_loop(uint8_t *msg) {
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
} */



/**
 * Initialize TCP CLI server parameters
 * @param sn Socket number to use for CLI (must be 0-7 and not used by other services)
 * @param port TCP port to listen on (e.g. 8000)
 * @param buf Pointer to buffer for receiving CLI data (must be allocated by caller)
 * @param buf_size Size of the CLI buffer (must be sufficient for expected command length, e.g. 256 bytes)
 * @param timeout_sec Timeout for idle CLI connections in seconds (e.g. 30 seconds)
 */
void tcp_cli_init(uint8_t sn, uint16_t port,
                  uint8_t *buf, uint16_t buf_size, int16_t timeout_sec) {
    cli_sn = sn;
    cli_port = port;
    cli_buf_rx = buf;
    cli_buf_size = buf_size;
    cli_timeout_ms = timeout_sec * 1000;  // convert to milliseconds
}

void cli_hook_init(const tcp_cli_hooks_t *hooks) {
    // This function can be called during initialization to set up CLI hooks
    if (hooks)
        cli_hooks = *hooks;  // copy hooks struct if provided
    else
        cli_hooks = (tcp_cli_hooks_t){0}; // initialize to zero if no hooks provided

}

/**
 * TCP CLI service loop on one socket
 */
int32_t tcp_cli_service(void) {
    static absolute_time_t last_rx_time = {0};   // Timestamp of last received byte
    int32_t ret;
    uint16_t size;
    uint8_t destip[4];
    uint16_t destport;

    switch (getSn_SR((uint32_t)cli_sn)) {

    case SOCK_ESTABLISHED:
        // On new connection
        if (getSn_IR((uint32_t)cli_sn) & Sn_IR_CON) {
            getSn_DIPR((uint32_t)cli_sn, destip);
            destport = getSn_DPORT((uint32_t)cli_sn);

            printf("[CLI] Socket %d connected from %d.%d.%d.%d:%d\r\n",
                   cli_sn, destip[0], destip[1], destip[2], destip[3], destport);

            setSn_IR((uint32_t)cli_sn, Sn_IR_CON);

            // Reset timeout timer
            last_rx_time = get_absolute_time();
            printf("[CLI] established, current time: %lld\r\n", last_rx_time);

            // telnet_greeting(cli_sn, destip);
            if (cli_hooks.on_connect) {
                cli_hooks.on_connect(cli_sn, destip);
            }
        }

        // Handle received data
        size = getSn_RX_RSR(cli_sn);
        if (size > 0) {
            if (size >= cli_buf_size) {
                size = cli_buf_size - 1;  // clamp to buffer size, leaving space for null terminator
            }

            ret = recv(cli_sn, cli_buf_rx, size);
            if (ret <= 0) return ret;

            cli_buf_rx[ret] = 0;

            // Update timeout timestamp
            last_rx_time = get_absolute_time();

            // Trim CR/LF
            char *cmd = (char*)cli_buf_rx;
            char *newline = strpbrk(cmd, "\r\n");
            if (newline) *newline = 0;

            printf("[CLI] Command received: '%s'\r\n", cmd);
            if (cli_hooks.handle_command) {
                cli_hooks.handle_command(cmd, cli_sn);
            } else {
                // Default handling if no hook provided
                const char *msg = "No command handler\r\n";
                send(cli_sn, (uint8_t*)msg, (uint16_t)strlen(msg));
            }
            // handle_command(cmd, cli_sn);
        } else {
            // No data – check timeout
            if (absolute_time_diff_us(last_rx_time, get_absolute_time()) >= (cli_timeout_ms * 1000)) {
                const char *msg = "timeout\r\n";
                send(cli_sn, (uint8_t*)msg, (uint16_t)strlen(msg));
                printf("[CLI] Idle timeout, closing socket %d\r\n", cli_sn);
                disconnect(cli_sn);
                return 0;
            }
        }
        break;

    case SOCK_CLOSE_WAIT:
        printf("[CLI] Close wait, closing socket %d\r\n", cli_sn);
        if ((ret = disconnect(cli_sn)) != SOCK_OK) return ret;
        break;

    case SOCK_INIT:
        if ((ret = listen(cli_sn)) != SOCK_OK) {
            printf("[CLI] Listen error %d\r\n", ret);
            return ret;
        }
        printf("[CLI] Listening on port %d\r\n", cli_port);
        break;

    case SOCK_CLOSED:
        if ((ret = socket(cli_sn, Sn_MR_TCP, cli_port, Sn_MR_ND)) != cli_sn) {
            printf("[CLI] Socket open error %d\r\n", ret);
            return ret;
        }
        printf("[CLI] Socket %d opened on port %d\r\n", cli_sn, cli_port);
        break;

    default:
        break;
    }
    return 1;
}


void udp_interrupts_enable(void) {
    SOCKET ddp_sock = ddp_sn;
    intr_kind imr = 0, ir;

    // You can enable just the sockets you use to avoid extra wakeups. In the global mask (SIMR).
    imr = (intr_kind)(IK_SOCK_0 << ddp_sock);  // Interrupt mask for UDP DDP sockets
    // Or enable all socket interrupts:
    // imr |= IK_SOCK_ALL;
    // (Optional) also enable network-level interrupts if you want them:
    // imr |= IK_NET_ALL;
    ctlwizchip(CW_SET_INTRMASK, &imr);  // wizchip_setinterruptmask(*((intr_kind*)arg));

    // Clear ALL pending (global + per-socket + service-level) to start clean
    ir = IK_INT_ALL;
    ctlwizchip(CW_CLR_INTERRUPT, &ir);  // wizchip_clrinterrupt(*((intr_kind*)arg));
}

#ifdef _TIME_DEBUG_
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

static volatile uint32_t ik_pending_table[TABLE_IRQ_SIZE];
static volatile uint8_t ik_pending_table_idx = 0;
#endif

// This will be called when INTn (GPIO 21) goes low
/* void wiznet_gpio_irq_handler_single_socket(uint gpio, uint32_t events)
{
    SOCKET ddp_sock = ddp_sn;  // UDP_DDP_SOCKET;
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
}  */

/**
 * WIZnet GPIO IRQ handler for multiple sockets
 * but used only for UDP DDP socket here
 */
void wiznet_gpio_irq_handler(uint gpio, uint32_t events)
{
    (void)gpio;
    (void)events;

    int32_t ret;
    // intr_kind pending;  // 
    intr_kind sock_bit = (IK_SOCK_0 << ddp_sn);
    uint16_t recv_len;
    uint16_t srcport;
    uint8_t srcip[16];
    uint8_t addr_len;
    // uint8_t sn_ir;
    SOCKET sock_num = ddp_sn;

#ifdef _TIME_DEBUG_
    t_irq_routine_start = time_us_32();
    time_table_irq[time_table_idx] = t_irq_routine_start - time_irq_start;
    time_table_idx++;
    if (time_table_idx >= TABLE_IRQ_SIZE)
        time_table_idx = 0;
    time_irq_start = t_irq_routine_start;

    ik_pending_table[ik_pending_table_idx++] = wizchip_getinterrupt();
    if (ik_pending_table_idx >= TABLE_IRQ_SIZE)
        ik_pending_table_idx = 0;
#endif
    // for all sockets, check which have pending interrupts
    // this part is simplified to just one socket here
    //
    // pending = wizchip_getinterrupt(); // ret = (((uint32_t)getSLIR())<<16) | ret;
    // for (sock_num = 0; sock_num < _WIZCHIP_SOCK_NUM_; sock_num++) {
    //     sock_bit = (IK_SOCK_0 << sock_num);
    //     if (pending & sock_bit) {
    //         sn_ir = getSn_IR(sock_num);
    //         if (sn_ir & Sn_IR_RECV) {
                getsockopt(sock_num, SO_RECVBUF, &recv_len);
                if (recv_len) {
                    //if (recv_len > UDP_RX_BUF_SZ)
                    //    recv_len = UDP_RX_BUF_SZ;
    
                    ret = recvfrom(sock_num,
                                   udp_rx_ring_buf[udp_ring_idx],
                                   recv_len,
                                   (uint8_t *)&srcip[0],
                                   (uint16_t *)&srcport,
                                   &addr_len);
                    if (ret > 0) {
                        udp_rx_buf_len[udp_ring_idx++] = (uint16_t)ret;
                        udp_ring_idx %= UDP_RING_COUNT;
                    } else {
                        // recv error
                        // irq_error_count++;
                    }
                }
                setSn_IR((uint32_t)sock_num, Sn_IR_RECV);      // clear socket latch
    //         } // if (sn_ir & Sn_IR_RECV)
            ctlwizchip(CW_CLR_INTERRUPT, &sock_bit);    //  wizchip_clrinterrupt(sock_bit);   // clear global bit
    //     } // if (pending & sock_bit)
    // } // for all sockets

#ifdef _TIME_DEBUG_
    time_irq_routine_duration = time_us_32() - t_irq_routine_start;
    if (time_irq_routine_duration > time_irq_routine_max)
        time_irq_routine_max = time_irq_routine_duration;
    time_table_duration[time_duration_idx++] = time_irq_routine_duration;
    if (time_duration_idx >= TABLE_IRQ_SIZE)
        time_duration_idx = 0;
#endif
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

// was: udp_socket_init(void)
// new: void tcp_cli_init(uint8_t sn, uint16_t port, uint8_t *buf, uint16_t buf_size, int16_t timeout_sec);

void udp_ddp_init(uint8_t sn, uint16_t port, uint8_t *buf, uint16_t buf_size) {
    ddp_sn = sn;
    ddp_port = port;
    ddp_buf_frame = buf;
    ddp_buf_size = buf_size;

    // uint16_t port = ddp_port;
    // SOCKET ddp_sock = ddp_sn;
    // SOCKET sn;
    uint8_t protocol;
    #ifdef _UDP_DEBUG_
    uint8_t* mode_msg;
    #endif

    check_loopback_mode_W6x00();    // as default set to AS_IPV4

    switch(loopback_mode) {
    case AS_IPV4:
        protocol = Sn_MR_UDP4;
        #ifdef _UDP_DEBUG_
        mode_msg = msg_ip_v4;
        #endif
        break;
    case AS_IPV6:
        protocol = Sn_MR_UDP6;
        #ifdef _UDP_DEBUG_
        mode_msg = msg_ip_v6;
        #endif
        break;
    case AS_IPDUAL:
    default:
        protocol = Sn_MR_UDPD;
        #ifdef _UDP_DEBUG_
        mode_msg = msg_ip_dual;
        #endif
        break;
    }

    intr_kind sock_bit = (IK_SOCK_0 << ddp_sn);
    int8_t rc = socket((uint32_t)ddp_sn, protocol, ddp_port, SOCK_IO_NONBLOCK);
    if (rc < 0) return;
    sn = (typeof(sn))rc;

    if(sn != ddp_sn){    /* reinitialize the socket */
        #ifdef _UDP_DEBUG_
            printf("%d : Fail to create socket.\r\n", ddp_sn);
        #endif
        return; // SOCKERR_SOCKNUM;
    }
    #ifdef _UDP_DEBUG_
        printf("%d:Socket UDP opened, port [%d] as %s\r\n", ddp_sn, ddp_port, mode_msg);   // getSn_SR(ddp_sock)
    #endif

    // Per-socket: enable only RECV interrupt
    // Sn_IMR bits: Sn_IR_SENDOK(0x10), Sn_IR_TIMEOUT(0x08), Sn_IR_RECV(0x04), ...
    setSn_IMR(ddp_sn, Sn_IR_RECV);     // mask bit RECV=1
    // Clear any pending per-socket interrupts
    ctlwizchip(CW_CLR_INTERRUPT, &sock_bit);  // wizchip_clrinterrupt(sock_bit);  // clear any pending per-socket interrupts  // setSn_IR(ddp_sock, 0xFF);
}



void process_udp_ring(void) {

    int tmp = (udp_ring_idx + 1) % UDP_RING_COUNT;
    uint8_t rd_idx = (uint8_t)tmp;  // (udp_ring_idx + 1) % UDP_RING_COUNT;

    for (uint8_t id = 0; id < UDP_RING_COUNT; id++) {
        // printf("UDP ring slot %d: len=%d\n", id, udp_rx_buf_len[id]);
        if (udp_rx_buf_len[rd_idx] != 0) {
            drain_loop_count++;
            process_ddp_packet(udp_rx_ring_buf[rd_idx], udp_rx_buf_len[rd_idx]);
            udp_rx_buf_len[rd_idx] = 0;  // mark slot free
        }
        tmp = (rd_idx + 1) % UDP_RING_COUNT;
        // rd_idx = (rd_idx + 1) % UDP_RING_COUNT;
        rd_idx = (uint8_t)tmp;
    }
}


// static inline void wiznet_service_if_needed(void) {
// }



/**
 * UDP DDP server loop
 */
// int32_t ddp_loop(uint32_t *pkt_counter, uint32_t *last_push_ms) {
int32_t ddp_loop(void) {
    // wiznet_service_if_needed();

    if (!wiznet_rx_pending) return 0;
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
    for (uint8_t i = 0; i < ik_pending_table_idx; i++) {
        printf("    IRQ pending[%d]: 0x%08X\n", i, ik_pending_table[i]);
    }
    ik_pending_table_idx = 0;

    return 0;
}



/**
 * Copy helper: write DDP payload into framebuffer
 */

static void ddp_copy_payload(const uint8_t *payload, uint32_t offset, uint32_t length)
{
    uint32_t in, out;

    for(in = 0, out = offset; in < length; in++, out++) {
        ddp_buf_frame[out] = payload[in];
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
            ws2815_show(ddp_buf_frame);   // push frame to LEDs (pass flat uint8_t pointer)
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
        printf("DDP - push to ws2815, blocked for a while.\r\n");
        // ws2815_show(rx_fb);   // push frame to LEDs (pass flat uint8_t pointer)
    }
}

/**
 * Re-enable sign-conversion warnings
 */
#pragma GCC diagnostic pop
