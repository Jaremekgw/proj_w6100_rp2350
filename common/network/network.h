/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * Important WIZnet board with W6100 configuration in library ioLibrary_Driver:
 * #define _PHY_IO_MODE_                  _PHY_IO_MODE_PHYCR_
 * #define SPI_CLK  40      // remember to set in wizchip_spi.h speed 40 MHz to receive at 4 Mbps DDP
 * 
 */

#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "tcp_cli.h"
#include "wizchip_conf.h"



// --- from socket.h ---
#define SOCKET                uint8_t  ///< SOCKET type define for legacy driver

#define SOCK_OK               1        ///< Result is OK about socket process.
#define SOCK_BUSY             0        ///< Socket is busy on processing the operation. Valid only Non-block IO Mode.
#define SOCK_FATAL            -1000    ///< Result is fatal error about socket process.

#define SOCK_ERROR            0        
#define SOCKERR_SOCKNUM       (SOCK_ERROR - 1)     ///< Invalid socket number
#define SOCKERR_SOCKOPT       (SOCK_ERROR - 2)     ///< Invalid socket option
#define SOCKERR_SOCKINIT      (SOCK_ERROR - 3)     ///< Socket is not initialized or SIPR is Zero IP address when Sn_MR_TCP
#define SOCKERR_SOCKCLOSED    (SOCK_ERROR - 4)     ///< Socket unexpectedly closed.
#define SOCKERR_SOCKMODE      (SOCK_ERROR - 5)     ///< Invalid socket mode for socket operation.
#define SOCKERR_SOCKFLAG      (SOCK_ERROR - 6)     ///< Invalid socket flag
#define SOCKERR_SOCKSTATUS    (SOCK_ERROR - 7)     ///< Invalid socket status for socket operation.
#define SOCKERR_ARG           (SOCK_ERROR - 10)    ///< Invalid argument.
#define SOCKERR_PORTZERO      (SOCK_ERROR - 11)    ///< Port number is zero
#define SOCKERR_IPINVALID     (SOCK_ERROR - 12)    ///< Invalid IP address
#define SOCKERR_TIMEOUT       (SOCK_ERROR - 13)    ///< Timeout occurred
#define SOCKERR_DATALEN       (SOCK_ERROR - 14)    ///< Data length is zero or greater than buffer max size.
#define SOCKERR_BUFFER        (SOCK_ERROR - 15)    ///< Socket buffer is not enough for data communication.

#define SOCKFATAL_PACKLEN     (SOCK_FATAL - 1)     ///< Invalid packet length. Fatal Error.

typedef enum
{
   SO_FLAG,           ///< Valid only in getsockopt(), For set flag of socket refer to <I>flag</I> in @ref socket().
   SO_TTL,              ///< Set TTL. @ref Sn_TTL  ( @ref setSn_TTL(), @ref getSn_TTL() )
   SO_TOS,              ///< Set TOS. @ref Sn_TOS  ( @ref setSn_TOS(), @ref getSn_TOS() )
   SO_MSS,              ///< Set MSS. @ref Sn_MSSR ( @ref setSn_MSSR(), @ref getSn_MSSR() )
   SO_DESTIP,           ///< Set the destination IP address. @ref Sn_DIPR ( @ref setSn_DIPR(), @ref getSn_DIPR() )
   SO_DESTPORT,         ///< Set the destination Port number. @ref Sn_DPORT ( @ref setSn_DPORT(), @ref getSn_DPORT() )
#if _WIZCHIP_ != 5100
   SO_KEEPALIVESEND,    ///< Valid only in setsockopt. Manually send keep-alive packet in TCP mode, Not supported in W5100
   #if !( (_WIZCHIP_ == 5100) || (_WIZCHIP_ == 5200) )
      SO_KEEPALIVEAUTO, ///< Set/Get keep-alive auto transmission timer in TCP mode, Not supported in W5100, W5200
   #endif
#endif
   SO_SENDBUF,          ///< Valid only in getsockopt. Get the free data size of Socekt TX buffer. @ref Sn_TX_FSR, @ref getSn_TX_FSR()
   SO_RECVBUF,          ///< Valid only in getsockopt. Get the received data size in socket RX buffer. @ref Sn_RX_RSR, @ref getSn_RX_RSR()
   SO_STATUS,           ///< Valid only in getsockopt. Get the socket status. @ref Sn_SR, @ref getSn_SR()

   //teddy 240122
//#if _WIZCHIP_ == W6100 || _WIZCHIP_ == W6300
     SO_EXTSTATUS,        ///< Valid only in @ref getsockopt(). Get the extended TCP SOCKETn status. @ref getSn_ESR()
     SO_MODE,
//#endif
   SO_REMAINSIZE,       ///< Valid only in getsockopt. Get the remained packet size in other then TCP mode.
   SO_PACKINFO          ///< Valid only in getsockopt. Get the packet information as @ref PACK_FIRST, @ref PACK_REMAINED, and @ref PACK_COMPLETED in other then TCP mode.
}sockopt_type;


/////////////////////////////
// SOCKET CONTROL & OPTION //
/////////////////////////////
#define SOCK_IO_BLOCK         0  ///< Socket Block IO Mode in @ref setsockopt().
#define SOCK_IO_NONBLOCK      1  ///< Socket Non-block IO Mode in @ref setsockopt().


int8_t  getsockopt(uint8_t sn, sockopt_type sotype, void* arg);
int8_t  socket(uint8_t sn, uint8_t protocol, uint16_t port, uint8_t flag);
int8_t  close(uint8_t sn);
int8_t  listen(uint8_t sn);
int8_t  disconnect(uint8_t sn);
int32_t send(uint8_t sn, uint8_t * buf, uint16_t len);
int32_t recv(uint8_t sn, uint8_t * buf, uint16_t len);



 /**
    * @ingroup WIZnet_socket_APIs
    * @brief  by_Lihan
    */
int32_t sendto_W5x00(uint8_t sn, uint8_t * buf, uint16_t len, uint8_t * addr, uint16_t port );
 /**
    * @ingroup WIZnet_socket_APIs
    * @brief  by_Lihan
    */
int32_t sendto_W6x00(uint8_t sn, uint8_t * buf, uint16_t len, uint8_t * addr, uint16_t port,uint8_t addrlen );

                                                                                                                        #define GET_MACRO_sendto(_1, _2, _3, _4, _5 , _6, NAME, ...) NAME
#define CHOOSE_sendto_MACRO(...) GET_MACRO_sendto(__VA_ARGS__, sendto_6, sendto_5)                                      
// by_LIhan for overroading                                                                                             // NOTE_LIHAN: Some sections of this code are not yet fully defined.
//   In case of get 3 arguments - int8_t sendto_W5x00(uint8_t sn, uint8_t * addr, uint16_t port  );
//   In case of get 4 arguments - int8_t sendto_W6x00(uint8_t sn, uint8_t * addr, uint16_t port,uint8_t addrlen );
#define sendto(...) CHOOSE_sendto_MACRO(__VA_ARGS__)(__VA_ARGS__)

//   In case of get 3 arguments
#define sendto_5( sn,   buf,  len,  addr,  port ) sendto_W5x00( sn,   buf,  len,  addr,  port)

//   In case of get 4 arguments
#define sendto_6( sn,   buf,  len,  addr,  port, addrlen ) sendto_W6x00( sn,   buf,  len,  addr,  port, addrlen)


 /**
    * @ingroup WIZnet_socket_APIs
    * @brief  byLihan_W5x00
    */
int32_t recvfrom_W5x00(uint8_t sn, uint8_t * buf, uint16_t len, uint8_t * addr, uint16_t *port);
 /**
    * @ingroup WIZnet_socket_APIs
    * @brief  byLihan_Wx00
    */
int32_t recvfrom_W6x00(uint8_t sn, uint8_t * buf, uint16_t len, uint8_t * addr, uint16_t *port,  uint8_t *addrlen );


#define GET_MACRO_recvfrom(_1, _2, _3, _4, _5, _6 ,NAME, ...) NAME
#define CHOOSE_recvfrom_MACRO(...) GET_MACRO_recvfrom(__VA_ARGS__, recvfrom_6, recvfrom_5)

// by_LIhanfor overroading
// In case of get 3 arguments - int8_t recvfrom_W5x00(uint8_t sn, uint8_t * addr, uint16_t port  );
// In case of get 4 arguments - int8_t recvfrom_W6x00(uint8_t sn, uint8_t * addr, uint16_t port,uint8_t addrlen );
#define recvfrom(...) CHOOSE_recvfrom_MACRO(__VA_ARGS__)(__VA_ARGS__)

//   In case of get 3 arguments
#define recvfrom_5(sn,   buf,  len,  addr,  port ) recvfrom_W5x00(sn,   buf,  len,  addr,  port)

//   In case of get 4 arguments
#define recvfrom_6(sn,   buf,  len,  addr,  port, addrlen  ) recvfrom_W6x00(sn,   buf,  len,  addr,  port, addrlen )

// --- end from socket.h ---


void network_initialize(wiz_NetInfo *net_info);
void print_network_information(void);
void print_ipv6_addr(uint8_t* name, uint8_t* ip6addr);

void wiznet_gpio_irq_init(void);
void udp_interrupts_enable(void);
// void udp_socket_init(void);
void udp_ddp_init(uint8_t sn, uint16_t port, uint8_t *buf, uint16_t buf_size);
void init_net_info(void);
void wiznet_drain_udp(void);

// CLI functions
void tcp_cli_init(uint8_t sn, uint16_t port, uint8_t *buf, uint16_t buf_size, int16_t timeout_sec);
void cli_hook_init(const tcp_cli_hooks_t *hooks);
int32_t tcp_cli_service(void);


int32_t ddp_loop(); // (uint32_t *pkt_counter, uint32_t *last_push_ms);

// void ddp_copy_payload(const uint8_t *payload, uint32_t offset, uint32_t length);
void process_ddp_udp(SOCKET s, uint32_t *pkt_counter, uint32_t *last_push_ms);

#endif /* _NETWORK_H_ */
