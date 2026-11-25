#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "hardware/uart.h"
#include "ms72sf1_parser.h"

typedef struct {
    uart_inst_t *instance;
    uint8_t func;
    uint rx_pin;
    uint tx_pin;
    uint32_t baudrate;
} ms72sf1_uart_cfg_t;

typedef struct {
    uint32_t frames;
    uint32_t invalid_frames;
    uint32_t text_lines;
    uint32_t rx_overflows;
    char last_line[128];
} ms72sf1_diagnostics_t;

bool ms72sf1_drv_init(const ms72sf1_uart_cfg_t *cfg);
void ms72sf1_drv_poll(void);
bool ms72sf1_drv_get_ready(ms72sf1_frame_t *out);
void ms72sf1_drv_get_diagnostics(ms72sf1_diagnostics_t *out);
/* Send a single printable AT+ command, appending the required LF. */
bool ms72sf1_drv_send_at(const char *command);
