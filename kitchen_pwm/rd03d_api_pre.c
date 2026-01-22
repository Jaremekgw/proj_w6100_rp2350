/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include "rd03d_api.h"
#include "rd03d_drv.h"
#include "rd03d_protocol.h"

static rd03d_state_t state;
static bool state_valid;

bool rd03d_api_init(void)
{
    state_valid = false;
    return rd03d_drv_init();
}

void rd03d_api_poll(void)
{
    rd03d_drv_poll();

    rd03d_frame_t f;
    if (!rd03d_drv_get_frame(&f))
        return;

    switch (f.msg_id)
    {
        case RD03D_MSG_DISTANCE:
            state.distance_mm =
                ((uint16_t)f.payload[0] << 8) | f.payload[1];
            state.confidence = f.payload[2];
            state.presence   = state.confidence > 0;
            state_valid = true;
            break;

        default:
            break;
    }
}

bool rd03d_api_get_state(rd03d_state_t *out)
{
    if (!state_valid)
        return false;

    *out = state;
    return true;
}


