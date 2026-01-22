/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rd03d_cli.h"
#include "rd03d_api.h"
#include "rd03d_drv.h"

#include <stdio.h>
#include <string.h>

/* Replace these with your actual CLI registration hooks */
extern void cli_register(const char *name,
                         const char *help,
                         int (*handler)(int argc, char **argv));

static bool s_dump_continuous = false;
static bool s_dump_once = false;
static bool s_dump_raw = false;

static void print_state(const rd03d_state_t *st)
{
    printf("[RD03D] t=%lu ms presence=%d\n",
           (unsigned long)st->rx_time_ms, st->presence ? 1 : 0);

    for (int i = 0; i < RD03D_TRACKS; i++)
    {
        const rd03d_track_t *tr = &st->track[i];
        printf("  track[%d] valid=%d conf=%u x=%dmm y=%dmm v=%dcm/s d=%umm age=%lums\n",
               i,
               tr->valid ? 1 : 0,
               (unsigned)tr->confidence,
               (int)tr->x_mm,
               (int)tr->y_mm,
               (int)tr->speed_cms,
               (unsigned)tr->distance_mm,
               (unsigned long)(st->rx_time_ms - tr->last_seen_ms));
    }
}

static void dump_raw_frame_if_needed(void)
{
    if (!s_dump_raw)
        return;

    rd03d_frame_t f;
    if (!rd03d_drv_get_frame(&f))
        return;

    /* Re-emit the raw report bytes for debugging */
    const uint8_t *p = (const uint8_t *)&f.report;
    printf("[RD03D][RAW] AA FF 03 00 ");
    for (unsigned i = 0; i < sizeof(f.report); i++)
        printf("%02X ", p[i]);
    printf("55 CC\n");
}

static int cmd_rd03d(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("usage: rd03d status | dump on|off|once|raw\n");
        return 0;
    }

    if (strcmp(argv[1], "status") == 0)
    {
        rd03d_state_t st;
        if (rd03d_api_get_state(&st))
            print_state(&st);
        else
            printf("[RD03D] no data yet\n");
        return 0;
    }

    if (strcmp(argv[1], "dump") == 0)
    {
        if (argc < 3)
        {
            printf("usage: rd03d dump on|off|once|raw\n");
            return 0;
        }

        if (strcmp(argv[2], "on") == 0)
        {
            s_dump_continuous = true;
            printf("[RD03D] dump on\n");
        }
        else if (strcmp(argv[2], "off") == 0)
        {
            s_dump_continuous = false;
            s_dump_once = false;
            s_dump_raw = false;
            printf("[RD03D] dump off\n");
        }
        else if (strcmp(argv[2], "once") == 0)
        {
            s_dump_once = true;
            printf("[RD03D] dump once (next frame)\n");
        }
        else if (strcmp(argv[2], "raw") == 0)
        {
            s_dump_raw = true;
            printf("[RD03D] raw dump enabled (will consume next raw frame)\n");
        }
        else
        {
            printf("usage: rd03d dump on|off|once|raw\n");
        }
        return 0;
    }

    printf("unknown subcommand\n");
    return 0;
}

void rd03d_cli_register(void)
{
    cli_register("rd03d", "RD-03D radar commands", cmd_rd03d);
}

/* Call this from your main loop periodically after rd03d_api_poll() */
void rd03d_cli_tick(void)
{
    if (!s_dump_continuous && !s_dump_once && !s_dump_raw)
        return;

    if (s_dump_raw)
    {
        dump_raw_frame_if_needed();
        s_dump_raw = false;
        return;
    }

    rd03d_state_t st;
    if (rd03d_api_get_state(&st))
    {
        print_state(&st);
        if (s_dump_once)
            s_dump_once = false;
    }
}
