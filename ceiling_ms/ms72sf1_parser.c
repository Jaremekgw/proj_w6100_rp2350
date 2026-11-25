#include "ms72sf1_parser.h"
#include <string.h>

static const uint8_t header[MS72SF1_HEADER_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8};
_Static_assert(sizeof(float) == 4, "MS72SF1 requires 32-bit float");

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static float read_float(const uint8_t *p)
{
    uint32_t bits = read_u32(p);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool decode_frame(ms72sf1_parser_t *p)
{
    const uint8_t *b = p->buffer;
    uint32_t track_len = read_u32(b + 28);
    if (read_u32(b + 16) != MS72SF1_POINT_TLV || read_u32(b + 20) != 0 ||
        read_u32(b + 24) != MS72SF1_TRACK_TLV ||
        track_len % MS72SF1_TRACK_SIZE != 0 ||
        track_len > MS72SF1_MAX_TRACKS * MS72SF1_TRACK_SIZE ||
        p->expected != MS72SF1_FIXED_FRAME_SIZE + track_len)
        return false;

    ms72sf1_frame_t frame = {0};
    frame.frame_number = read_u32(b + 12);
    frame.track_count = (uint8_t)(track_len / MS72SF1_TRACK_SIZE);
    for (size_t i = 0; i < frame.track_count; ++i) {
        const uint8_t *t = b + MS72SF1_FIXED_FRAME_SIZE + i * MS72SF1_TRACK_SIZE;
        ms72sf1_track_t *out = &frame.tracks[i];
        out->reserved = read_u32(t);
        out->id = read_u32(t + 4);
        out->x_m = read_float(t + 8);
        out->y_m = read_float(t + 12);
        out->z_m = read_float(t + 16);
        out->vx_mps = read_float(t + 20);
        out->vy_mps = read_float(t + 24);
        out->vz_mps = read_float(t + 28);
    }
    p->frame = frame;
    ++p->frames;
    return true;
}

/* After a malformed frame, retain a possible embedded next header or prefix. */
static void resync(ms72sf1_parser_t *p)
{
    for (size_t i = 1; i < p->used; ++i) {
        size_t n = p->used - i;
        size_t compare = n < sizeof(header) ? n : sizeof(header);
        if (memcmp(p->buffer + i, header, compare) == 0) {
            memmove(p->buffer, p->buffer + i, n);
            p->used = n;
            p->expected = 0;
            return;
        }
    }
    p->used = 0;
    p->expected = 0;
}

void ms72sf1_parser_init(ms72sf1_parser_t *p)
{
    memset(p, 0, sizeof(*p));
}

unsigned ms72sf1_parser_feed(ms72sf1_parser_t *p, uint8_t byte)
{
    if (p->used == 0 && byte != header[0]) {
        if (byte == '\n') {
            if (p->line_overflow) {
                p->line_used = 0;
                p->line_overflow = false;
                return 0;
            }
            if (p->line_used && p->line[p->line_used - 1] == '\r')
                --p->line_used;
            p->line[p->line_used] = '\0';
            p->line_used = 0;
            if (p->line[0] != '\0') {
                memcpy(p->last_line, p->line, strlen(p->line) + 1);
                ++p->text_lines;
                return MS72SF1_EVENT_TEXT;
            }
        } else if ((byte >= 32 && byte <= 126) || byte == '\r') {
            if (p->line_used < sizeof(p->line) - 1)
                p->line[p->line_used++] = (char)byte;
            else
                p->line_overflow = true;
        } else {
            p->line_used = 0;
            p->line_overflow = false;
        }
        return 0;
    }

    if (p->used == 0) {
        p->line_used = 0;
        p->line_overflow = false;
    }
    p->buffer[p->used++] = byte;
    unsigned events = 0;
    for (;;) {
        size_t prefix = p->used < sizeof(header) ? p->used : sizeof(header);
        if (memcmp(p->buffer, header, prefix) != 0) {
            resync(p);
            if (!p->used) break;
            continue;
        }
        if (p->used < 12) break;
        if (!p->expected) {
            uint32_t len = read_u32(p->buffer + 8);
            if (len < MS72SF1_FIXED_FRAME_SIZE || len > MS72SF1_MAX_FRAME_SIZE) {
                ++p->invalid_frames;
                resync(p);
                if (!p->used) break;
                continue;
            }
            p->expected = len;
        }
        if (p->used < p->expected) break;
        if (decode_frame(p)) {
            events |= MS72SF1_EVENT_FRAME;
            /* Normally used == expected; resync can retain additional bytes. */
            p->used -= p->expected;
            memmove(p->buffer, p->buffer + p->expected, p->used);
            p->expected = 0;
        } else {
            ++p->invalid_frames;
            resync(p);
        }
        if (!p->used) break;
    }
    return events;
}
