/*
 * Shell backend over FreeMASTER pipes
 *
 * Copyright 2018 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SHELL_FREEMASTER_H_
#define SHELL_FREEMASTER_H_

#include "freemaster.h"
#include <zephyr/shell/shell.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FreeMASTER shell user configuration. */
struct shell_freemaster_config
{
    struct shell_backend_config_flags cfg_flags; /* Shell flags to apply */
    FMSTR_PIPE_PORT port; /* FreeMASTER pipe port to be used */
    char* rxbuf;          /* Pipe receive buffer */
    char* txbuf;          /* Pipe transmit buffer */
    uint32_t rxbuf_size;  /* Receive buffer size */
    uint32_t txbuf_size;  /* Transmit buffer size */
};

int FMSTR_PipeEnableShell(FMSTR_PIPE_PORT pipe_port, uint32_t log_level);
int FMSTR_PipeEnableShellEx(const struct shell_freemaster_config* config, uint32_t log_level);

#ifdef __cplusplus
}
#endif

#endif /* SHELL_FREEMASTER_H_ */
