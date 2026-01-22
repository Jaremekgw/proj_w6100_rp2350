/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "hardware/dma.h"
#include "hardware/regs/dma.h"
#include "pico/stdlib.h"
#include "pico/time.h"   // provides __time_critical_func

#include "utility.h"

#define CRC32_POLY 0xEDB88320u

uint32_t crc32_step(uint32_t crc, const uint8_t *buf, uint32_t len) {
    crc = crc ^ 0xFFFFFFFFu;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t b = buf[i];
        crc = crc ^ b;
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1));
    }
    return crc ^ 0xFFFFFFFFu;
}

// // Standard reflected CRC32 (IEEE 802.3)
// // __attribute__((noinline, section(".time_critical"))) 
// __attribute__((noinline, section(".time_critical." "config_crc32")))
uint32_t config_crc32(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;

    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (CRC32_POLY & mask);
        }
    }

    return ~crc;
}


/*
Hardware-accelerated CRC32 (RP2350, DMA Sniffer)
The sniffer is downstream of a DMA channel, so we perform:
 1. Configure DMA channel
 2. Enable sniffer mode = CRC-32
 3. Seed accumulator
 4. Run DMA from source buffer to a dummy sink
 5. Read accumulator
 6. Invert output (standard CRC32 final XOR)
*/

/**
 * Hardware-accelerated CRC32 (RP2350, DMA Sniffer)
 * The sniffer is downstream of a DMA channel.
 * 
 */
// __not_in_flash_func(uint32_t config_crc32_hw)(const void *data, size_t len)
// {
//     if (len == 0)
//         return 0xFFFFFFFFu;

//     uint channel = dma_claim_unused_channel(true);
//     dma_channel_config c = dma_channel_get_default_config(channel);

//     // We read from memory but discard output.
//     // Use a one-word dummy.
//     static uint32_t dummy_sink __attribute__((aligned(4)));

//     channel_config_set_transfer_data_size(&c, DMA_SIZE_8);     // Byte-granular CRC
//     channel_config_set_read_increment(&c, true);
//     channel_config_set_write_increment(&c, false);

//     dma_channel_configure(
//         channel,
//         &c,
//         &dummy_sink,               // write address (ignored)
//         data,                      // read address
//         len,                       // number of bytes
//         false                      // don't start yet
//     );

//     // ---- Configure Sniffer ----
//     //
//     // Mode 0 = CRC-32 (IEEE 802.3 polynomial)
//     // Seed = 0xFFFFFFFF (standard init)
//     // Result is bit-inverted by hardware after final XOR if we enable invert.
//     //
//     // SDK ref:
//     //   dma_sniffer_enable()              :contentReference[oaicite:2]{index=2}
//     //   dma_sniffer_set_data_accumulator  :contentReference[oaicite:3]{index=3}
//     //   dma_sniffer_set_output_invert_enabled
//     //   dma_sniffer_set_output_reverse_enabled
//     //
//     dma_sniffer_set_data_accumulator(0xFFFFFFFFu);
//     dma_sniffer_set_byte_swap_enabled(false);
//     dma_sniffer_set_output_reverse_enabled(false);
//     dma_sniffer_set_output_invert_enabled(false);

//     // Enable sniffer on this channel
//     dma_sniffer_enable(channel, 0x0 /* CRC32 */, true);

//     // ---- Start DMA ----
//     dma_channel_start(channel);

//     // Wait for completion
//     dma_channel_wait_for_finish_blocking(channel);

//     // Read HW accumulator
//     uint32_t crc = dma_sniffer_get_data_accumulator();

//     // Disable sniffer + release channel
//     dma_sniffer_disable();
//     dma_channel_unclaim(channel);

//     // Standard final XOR for CRC32-IEEE
//     return ~crc;
// }



/**
 * Helper to pass prints to buffer for sending data to socket
 */
int msg_printf(char **cursor, size_t *remaining, const char *fmt, ...) {
    if (*remaining == 0) return 0;

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(*cursor, *remaining, fmt, args);
    va_end(args);

    if (written < 0) return 0;

    if ((size_t)written >= *remaining) {
        // truncate
        *cursor += *remaining - 1;
        **cursor = '\0';
        *remaining = 0;
        return written;
    }

    *cursor += written;
    *remaining -= written;
    return written;
}

