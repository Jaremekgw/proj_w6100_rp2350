#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ms72sf1_protocol.h"

typedef struct {
    uint32_t reserved;
    uint32_t id;
    float x_m, y_m, z_m;
    float vx_mps, vy_mps, vz_mps;
} ms72sf1_track_t;

typedef struct {
    uint32_t frame_number;
    uint32_t rx_time_ms;
    uint8_t track_count;
    ms72sf1_track_t tracks[MS72SF1_MAX_TRACKS];
} ms72sf1_frame_t;

/* Parser owns its storage; feed bytes in arrival order from one context. */
typedef struct {
    uint8_t buffer[MS72SF1_MAX_FRAME_SIZE];
    size_t used;
    size_t expected;
    char line[128];
    size_t line_used;
    bool line_overflow;
    char last_line[128];
    ms72sf1_frame_t frame;
    uint32_t frames;
    uint32_t invalid_frames;
    uint32_t text_lines;
} ms72sf1_parser_t;

enum { MS72SF1_EVENT_FRAME = 1, MS72SF1_EVENT_TEXT = 2 };
void ms72sf1_parser_init(ms72sf1_parser_t *parser);
/* Return a bitmask of MS72SF1_EVENT_* for completed records. */
unsigned ms72sf1_parser_feed(ms72sf1_parser_t *parser, uint8_t byte);
