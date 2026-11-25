/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ms72sf1_cli.h"
#include "ms72sf1_api.h"
#include "ms72sf1_drv.h"

#include <stdio.h>
#include <string.h>

/* Replace these with your actual CLI registration hooks */
extern void cli_register(const char *name,
                         const char *help,
                         int (*handler)(int argc, char **argv));

static bool s_dump_continuous = false;
static bool s_dump_once = false;
static bool s_dump_raw = false;

static void print_state(const ms72sf1_state_t *st)
{
    printf("[MS72SF1] t=%lu ms presence=%d\n",
           (unsigned long)st->rx_time_ms, st->presence ? 1 : 0);

    for (int i = 0; i < MS72SF1_TRACKS; i++)
    {
        const ms72sf1_track_t *tr = &st->track[i];
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

static int cmd_ms72sf1(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("usage: ms72sf1 status | dump on|off|once|raw\n");
        return 0;
    }

    if (strcmp(argv[1], "status") == 0)
    {
        ms72sf1_state_t st;
        if (ms72sf1_api_get_state(&st))
            print_state(&st);
        else
            printf("[MS72SF1] no data yet\n");
        return 0;
    }

    if (strcmp(argv[1], "dump") == 0)
    {
        if (argc < 3)
        {
            printf("usage: ms72sf1 dump on|off|once|raw\n");
            return 0;
        }

        if (strcmp(argv[2], "on") == 0)
        {
            s_dump_continuous = true;
            printf("[MS72SF1] dump on\n");
        }
        else if (strcmp(argv[2], "off") == 0)
        {
            s_dump_continuous = false;
            s_dump_once = false;
            s_dump_raw = false;
            printf("[MS72SF1] dump off\n");
        }
        else if (strcmp(argv[2], "once") == 0)
        {
            s_dump_once = true;
            printf("[MS72SF1] dump once (next frame)\n");
        }
        else if (strcmp(argv[2], "raw") == 0)
        {
            s_dump_raw = true;
            printf("[MS72SF1] raw dump enabled (will consume next raw frame)\n");
        }
        else
        {
            printf("usage: ms72sf1 dump on|off|once|raw\n");
        }
        return 0;
    }

    printf("unknown subcommand\n");
    return 0;
}

void ms72sf1_cli_register(void)
{
    cli_register("ms72sf1", "MS72SF1 radar commands (legacy protocol)", cmd_ms72sf1);
}

/* Call this from your main loop periodically after ms72sf1_api_poll() */
void ms72sf1_cli_tick(void)
{
    if (!s_dump_continuous && !s_dump_once && !s_dump_raw)
        return;

    if (s_dump_raw)
    {
        // dump_raw_frame_if_needed();
        s_dump_raw = false;
        return;
    }

    ms72sf1_state_t st;
    if (ms72sf1_api_get_state(&st))
    {
        print_state(&st);
        if (s_dump_once)
            s_dump_once = false;
    }
}

bool ms72sf1_cli_change_dump_continuous(void)
{
    s_dump_continuous = !s_dump_continuous;
    return s_dump_continuous;
}

void ms72sf1_cli_print_state(void)
{
    ms72sf1_state_t st;
    if (ms72sf1_api_get_state(&st))
        print_state(&st);
    else
        printf("[MS72SF1] no data yet\n");
}

