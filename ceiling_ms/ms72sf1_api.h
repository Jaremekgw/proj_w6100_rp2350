#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "ms72sf1_parser.h"

/* Presence means at least one person in the latest frame; no timeout/filter. */
typedef struct {
    ms72sf1_frame_t frame;
    bool presence;
} ms72sf1_state_t;

bool ms72sf1_api_init(void);
void ms72sf1_api_poll(void);
bool ms72sf1_api_get_state(ms72sf1_state_t *out);
int ms72sf1_api_print_state(char *msg, size_t msg_max_sz);
