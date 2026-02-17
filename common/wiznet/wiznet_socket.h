/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * 
 * Wrapper for the WIZnet socket library. This is needed to suppress warnings about unused functions in the socket library, which is a third-party library that we don't want to modify directly. By including the socket library in this wrapper header, we can suppress the -Wunused-function warning for this specific target without affecting other targets that might use the same library.
 */

 #pragma once

 #if defined(__GNUC__) || defined(__clang__)
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wunused-function"
#endif

#include "socket.h"

#if defined(__GNUC__) || defined(__clang__)
  #pragma GCC diagnostic pop
#endif