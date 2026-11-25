#include "ms72sf1_api.h"
#include "ms72sf1_drv.h"
#include "config.h"
#include "hardware/gpio.h"
#include <stdio.h>
#include <stdarg.h>

static ms72sf1_state_t s_state;
static bool s_valid;

bool ms72sf1_api_init(void)
{
    const ms72sf1_uart_cfg_t cfg = {
        .instance = MS72SF1_UART_INST,
        .func = MS72SF1_UART_FUNC,
        .rx_pin = MS72SF1_UART_RX_PIN,
        .tx_pin = MS72SF1_UART_TX_PIN,
        .baudrate = MS72SF1_BAUDRATE
    };
    s_valid = false;
    return ms72sf1_drv_init(&cfg);
}

void ms72sf1_api_poll(void)
{
    ms72sf1_frame_t frame;
    if (ms72sf1_drv_get_ready(&frame)) {
        s_state.frame = frame;
        s_state.presence = frame.track_count != 0;
        s_valid = true;
    }
}

bool ms72sf1_api_get_state(ms72sf1_state_t *out)
{
    if (!out || !s_valid) return false;
    *out = s_state;
    return true;
}

static void append(char *msg, size_t size, size_t *used, const char *fmt, ...)
{
    if (*used >= size) return;
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(msg + *used, size - *used, fmt, args);
    va_end(args);
    if (n < 0) return;
    size_t written = (size_t)n;
    *used += written < size - *used ? written : size - *used - 1;
}

int ms72sf1_api_print_state(char *msg, size_t size)
{
    if (!msg || !size) return 0;
    size_t used = 0;
    msg[0] = '\0';
    ms72sf1_diagnostics_t diag;
    ms72sf1_drv_get_diagnostics(&diag);
    append(msg, size, &used, "MS72SF1 frames=%lu invalid=%lu rx_dropped=%lu lines=%lu\r\nLast response: %s\r\n",
           (unsigned long)diag.frames, (unsigned long)diag.invalid_frames,
           (unsigned long)diag.rx_overflows, (unsigned long)diag.text_lines,
           diag.last_line[0] ? diag.last_line : "(none)");
    if (!s_valid) {
        append(msg, size, &used, "No tracking frame received.\r\n");
    } else {
        const ms72sf1_frame_t *f = &s_state.frame;
        append(msg, size, &used, "Frame %lu at %lu ms, people=%u presence=%u\r\n",
               (unsigned long)f->frame_number, (unsigned long)f->rx_time_ms,
               (unsigned)f->track_count, s_state.presence ? 1u : 0u);
        for (size_t i = 0; i < f->track_count; ++i) {
            const ms72sf1_track_t *t = &f->tracks[i];
            append(msg, size, &used,
                   "id=%lu q=%lu pos=(%.2f, %.2f, %.2f) m vel=(%.2f, %.2f, %.2f) m/s\r\n",
                   (unsigned long)t->id, (unsigned long)t->reserved,
                   (double)t->x_m, (double)t->y_m, (double)t->z_m,
                   (double)t->vx_mps, (double)t->vy_mps, (double)t->vz_mps);
        }
    }
    return (int)used;
}
