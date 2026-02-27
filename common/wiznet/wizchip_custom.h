/**
    Copyright (c) 2022 WIZnet Co.,Ltd

    SPDX-License-Identifier: BSD-3-Clause
*/

#pragma once

#include "network.h"
// typedef   uint16_t  datasize_t;

// void wizchip_init_nonblocking(void);
void wizchip_custom_init(const network_t *net_info);
