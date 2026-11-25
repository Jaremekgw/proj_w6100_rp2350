#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Handle 'radar ...' commands; return false for unrelated commands. */
bool ms72sf1_cli_handle(const char *cmd, uint8_t socket);
