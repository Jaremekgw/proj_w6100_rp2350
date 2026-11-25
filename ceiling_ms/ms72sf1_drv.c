#include "ms72sf1_drv.h"
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

#define RX_RING_SIZE 4096u
static uart_inst_t *s_uart;
static ms72sf1_parser_t s_parser;
static bool s_ready;
/* Single producer (UART ISR), single consumer (main loop). */
static uint8_t s_rx[RX_RING_SIZE];
static volatile uint32_t s_head, s_tail, s_overflows;
static uint32_t s_seen_overflows;

static void uart_rx_irq(void)
{
    while (uart_is_readable(s_uart)) {
        uint8_t byte = (uint8_t)uart_getc(s_uart);
        uint32_t next = (s_head + 1u) & (RX_RING_SIZE - 1u);
        if (next == s_tail) {
            ++s_overflows;
        } else {
            s_rx[s_head] = byte;
            __compiler_memory_barrier();
            s_head = next;
        }
    }
}

bool ms72sf1_drv_init(const ms72sf1_uart_cfg_t *cfg)
{
    if (!cfg || !cfg->instance || !cfg->baudrate) return false;
    s_uart = cfg->instance;
    ms72sf1_parser_init(&s_parser);
    s_ready = false;
    s_head = s_tail = s_overflows = s_seen_overflows = 0;
    uart_init(s_uart, cfg->baudrate);
    gpio_set_function(cfg->tx_pin, cfg->func);
    gpio_set_function(cfg->rx_pin, cfg->func);
    uart_set_format(s_uart, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(s_uart, false, false);
    uart_set_fifo_enabled(s_uart, true);
    uint irq = s_uart == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(irq, uart_rx_irq);
    irq_set_enabled(irq, true);
    uart_set_irq_enables(s_uart, true, false);
    return true;
}

void ms72sf1_drv_poll(void)
{
    if (!s_uart) return;
    for (;;) {
        if (s_seen_overflows != s_overflows) {
            /* Dropped bytes make the partial record unreliable. Discard backlog. */
            s_seen_overflows = s_overflows;
            s_tail = s_head;
            s_ready = false;
            s_parser.used = s_parser.expected = s_parser.line_used = 0;
            s_parser.line_overflow = false;
        }
        if (s_tail == s_head) break;
        __compiler_memory_barrier();
        uint8_t byte = s_rx[s_tail];
        s_tail = (s_tail + 1u) & (RX_RING_SIZE - 1u);
        unsigned event = ms72sf1_parser_feed(&s_parser, byte);
        if ((event & MS72SF1_EVENT_FRAME) && s_seen_overflows == s_overflows) {
            s_parser.frame.rx_time_ms = to_ms_since_boot(get_absolute_time());
            s_ready = true;
        }
    }
}

bool ms72sf1_drv_get_ready(ms72sf1_frame_t *out)
{
    if (!out || !s_ready) return false;
    *out = s_parser.frame;
    s_ready = false;
    return true;
}

void ms72sf1_drv_get_diagnostics(ms72sf1_diagnostics_t *out)
{
    if (!out) return;
    out->frames = s_parser.frames;
    out->invalid_frames = s_parser.invalid_frames;
    out->text_lines = s_parser.text_lines;
    out->rx_overflows = s_overflows;
    memcpy(out->last_line, s_parser.last_line, sizeof(out->last_line));
}

bool ms72sf1_drv_send_at(const char *command)
{
    if (!s_uart || !command || strncmp(command, "AT+", 3) != 0) return false;
    size_t len = 0;
    while (command[len]) {
        if (len >= 100 || (unsigned char)command[len] < 32 ||
            (unsigned char)command[len] > 126) return false;
        ++len;
    }
    if (len <= 3) return false;
    uart_write_blocking(s_uart, (const uint8_t *)command, len);
    const uint8_t lf = '\n';
    uart_write_blocking(s_uart, &lf, 1);
    return true;
}
