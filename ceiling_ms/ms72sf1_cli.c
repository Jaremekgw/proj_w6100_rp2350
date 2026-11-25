#include "ms72sf1_cli.h"
#include "ms72sf1_api.h"
#include "ms72sf1_drv.h"
#include "tcp_cli.h"
#include <string.h>

bool ms72sf1_cli_handle(const char *cmd, uint8_t socket)
{
    if (strcmp(cmd, "radar") != 0 && strncmp(cmd, "radar ", 6) != 0)
        return false;
    if (strcmp(cmd, "radar status") == 0) {
        char msg[1800];
        ms72sf1_api_print_state(msg, sizeof(msg));
        cli_flush(socket, msg);
        return true;
    }
    const char *at = NULL;
    if (strcmp(cmd, "radar start") == 0) at = "AT+START";
    else if (strcmp(cmd, "radar stop") == 0) at = "AT+STOP";
    else if (strcmp(cmd, "radar read") == 0) at = "AT+READ";
    else if (strcmp(cmd, "radar binary") == 0) at = "AT+DEBUG=3";
    else if (strncmp(cmd, "radar at ", 9) == 0) at = cmd + 9;
    if (!at) {
        cli_flush(socket, "Usage: radar status|binary|start|stop|read|at AT+...\r\n");
    } else if (ms72sf1_drv_send_at(at)) {
        cli_flush(socket, "Command sent; check 'radar status' for the latest response.\r\n");
    } else {
        cli_flush(socket, "Invalid AT command or UART unavailable.\r\n");
    }
    return true;
}
