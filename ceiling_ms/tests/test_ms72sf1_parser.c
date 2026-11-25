#include "ms72sf1_parser.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void put_u32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (i * 8));
}
static void put_float(uint8_t *p, float v)
{
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    put_u32(p, bits);
}
static size_t make_frame(uint8_t *b, unsigned tracks)
{
    size_t len = MS72SF1_FIXED_FRAME_SIZE + tracks * MS72SF1_TRACK_SIZE;
    memset(b, 0, len);
    for (unsigned i = 0; i < 8; ++i) b[i] = (uint8_t)(i + 1);
    put_u32(b + 8, (uint32_t)len);
    put_u32(b + 12, 42);
    put_u32(b + 16, 1);
    put_u32(b + 24, 2);
    put_u32(b + 28, tracks * 32);
    for (unsigned i = 0; i < tracks; ++i) {
        uint8_t *t = b + 32 + i * 32;
        put_u32(t, 99);
        put_u32(t + 4, i + 1000);
        put_float(t + 8, -1.25f);
        put_float(t + 12, 2.5f);
        put_float(t + 16, 3.0f);
        put_float(t + 20, -0.5f);
        put_float(t + 24, 0.25f);
        put_float(t + 28, 1.0f);
    }
    return len;
}
static unsigned feed(ms72sf1_parser_t *p, const void *data, size_t n)
{
    const uint8_t *b = data;
    unsigned events = 0;
    while (n--) events |= ms72sf1_parser_feed(p, *b++);
    return events;
}
int main(void)
{
    ms72sf1_parser_t p;
    uint8_t b[MS72SF1_MAX_FRAME_SIZE];
    ms72sf1_parser_init(&p);
    assert(feed(&p, "AT+OK\r\n", 7) == MS72SF1_EVENT_TEXT);
    assert(strcmp(p.last_line, "AT+OK") == 0);
    size_t n = make_frame(b, 2);
    assert(feed(&p, b, 11) == 0);
    assert(feed(&p, b + 11, n - 11) == MS72SF1_EVENT_FRAME);
    assert(p.frame.frame_number == 42 && p.frame.track_count == 2);
    assert(p.frame.tracks[1].id == 1001 && p.frame.tracks[1].reserved == 99);
    assert(p.frame.tracks[1].x_m == -1.25f && p.frame.tracks[1].vy_mps == 0.25f);
    n = make_frame(b, 0);
    assert(feed(&p, b, n) == MS72SF1_EVENT_FRAME && p.frame.track_count == 0);
    n = make_frame(b, 10);
    assert(feed(&p, b, n) == MS72SF1_EVENT_FRAME && p.frame.track_count == 10);
    n = make_frame(b, 1);
    put_u32(b + 28, 33); /* malformed track length */
    assert(feed(&p, b, n) == 0 && p.invalid_frames == 1);
    put_u32(b + 8, 9999); /* oversized frame */
    assert(feed(&p, b, 12) == 0 && p.invalid_frames == 2);
    n = make_frame(b, 1);
    assert(feed(&p, b, n) == MS72SF1_EVENT_FRAME && p.frames == 4);
    /* Recover a complete header nested in an invalid-length candidate. */
    uint8_t noise[] = {1,2,3,4,5,6,7,8, 1,2,3,4,5,6,7,8};
    assert(feed(&p, noise, sizeof(noise)) == 0);
    assert(feed(&p, b + 8, n - 8) == MS72SF1_EVENT_FRAME);
    for (unsigned i = 0; i < 160; ++i) ms72sf1_parser_feed(&p, 'a');
    assert(ms72sf1_parser_feed(&p, '\n') == 0);
    assert(feed(&p, "OK\n", 3) == MS72SF1_EVENT_TEXT);
    assert(strcmp(p.last_line, "OK") == 0);
    puts("MS72SF1 parser tests passed");
    return 0;
}
