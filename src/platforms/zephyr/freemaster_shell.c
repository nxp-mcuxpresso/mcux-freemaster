/*
 * Shell backend over FreeMASTER pipes
 *
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/shell/shell.h>
#include <zephyr/init.h>

#include "freemaster.h"
#include "freemaster_private.h"
#include "freemaster_shell.h"

#if FMSTR_DISABLE == 0

#if !CONFIG_SHELL || !CONFIG_SHELL_BACKENDS
#error FreeMASTER shell requires CONFIG_SHELL and CONFIG_SHELL_BACKENDS enabled
#endif

/******************************************************************************
 * FreeMASTER Shell - local private declarations
 */

/* FreeMASTER shell context structure */
struct shell_freemaster {
    struct shell_freemaster_config config;  /* Copy of initial user configuration */
    shell_transport_handler_t evt_handler;  /* Event handler to call upon activity */
    void* evt_handler_param;                /* Event handler parameter */
    bool initialized;     /* Already initialized? */
    FMSTR_HPIPE hpipe;    /* FreeMASTER pipe handle*/
};

/* Callback invoked upon pipe activity */
static void _shell_freemaster_pipe_handler(FMSTR_HPIPE hpipe);

/******************************************************************************
 * FreeMASTER Shell transport API
 */

static int _shell_fmstr_init(const struct shell_transport *transport,
        const void *config,
        shell_transport_handler_t evt_handler,
        void *context)
{
    struct shell_freemaster *ctx = (struct shell_freemaster *)transport->ctx;

    if (ctx == NULL || ctx->initialized) {
        return -EINVAL;
    }

    /* Take configuration */
    ctx->config = *(const struct shell_freemaster_config *)config;

    /* Open pipe. The first pipe at port 1 */
    ctx->hpipe = FMSTR_PipeOpen(ctx->config.port,
        (FMSTR_PPIPEFUNC)_shell_freemaster_pipe_handler,
        (FMSTR_ADDR)ctx->config.rxbuf, ctx->config.rxbuf_size,
        (FMSTR_ADDR)ctx->config.txbuf, ctx->config.txbuf_size,
        FMSTR_PIPE_TYPE_ANSI_TERMINAL, "Zephyr shell pipe");

    if (ctx->hpipe == NULL) {
        return -ENODEV;
    }

    ctx->evt_handler =  evt_handler;
    ctx->evt_handler_param =  context;
    ctx->initialized = true;
    return 0;
}

static int _shell_fmstr_uninit(const struct shell_transport *transport)
{
    struct shell_freemaster *ctx = (struct shell_freemaster *)transport->ctx;

    if (ctx == NULL || !ctx->initialized) {
        return -ENODEV;
    }

    /* Do not let to de-initialize */
    return -EBUSY;
}

static int _shell_fmstr_enable(const struct shell_transport *transport, bool blocking)
{
    struct shell_freemaster *ctx = (struct shell_freemaster *)transport->ctx;

    if (ctx == NULL || !ctx->initialized) {
        return -ENODEV;
    }

    return 0;
}

static int _shell_fmstr_write(const struct shell_transport *transport,
         const void *data, size_t length, size_t *cnt)
{
    struct shell_freemaster *ctx = (struct shell_freemaster *)transport->ctx;
    size_t store_cnt;

    if (ctx == NULL || !ctx->initialized || ctx->hpipe == NULL) {
        *cnt = 0;
        return -ENODEV;
    }

    if(length > UINT16_MAX)
        length = UINT16_MAX;

    store_cnt = (size_t) FMSTR_PipeWrite(ctx->hpipe, (FMSTR_ADDR)data, (FMSTR_PIPE_SIZE)length, 1);
    *cnt = store_cnt;
    return 0;
}

static int _shell_fmstr_read(const struct shell_transport *transport,
        void *data, size_t length, size_t *cnt)
{
    struct shell_freemaster *ctx = (struct shell_freemaster *)transport->ctx;
    size_t read_cnt;

    if (ctx == NULL || !ctx->initialized || ctx->hpipe == NULL) {
        *cnt = 0;
        return -ENODEV;
    }

    if(length > UINT16_MAX)
        length = UINT16_MAX;

    read_cnt = (size_t) FMSTR_PipeRead(ctx->hpipe, (FMSTR_ADDR)data, (FMSTR_PIPE_SIZE)length, 1);
    *cnt = read_cnt;

    return 0;
}

/******************************************************************************
 * FreeMASTER Shell data
 */

static const struct shell_transport_api shell_freemaster_transport_api = {
    .init = _shell_fmstr_init,
    .uninit = _shell_fmstr_uninit,
    .enable = _shell_fmstr_enable,
    .write = _shell_fmstr_write,
    .read = _shell_fmstr_read
};

static struct shell_freemaster shell_freemaster_ctx;

static struct shell_transport shell_freemaster_transport =
{
    .api = &shell_freemaster_transport_api,
    .ctx = &shell_freemaster_ctx,
};

SHELL_DEFINE(shell_freemaster_inst, CONFIG_FMSTR_SHELL_PROMPT,
    &shell_freemaster_transport, 256,
    0, SHELL_FLAG_OLF_CRLF);


/******************************************************************************
 * FreeMASTER Shell internal code
 */

/* Callback invoked upon pipe activity */
static void _shell_freemaster_pipe_handler(FMSTR_HPIPE hpipe)
{
    const struct shell_transport *transport = shell_freemaster_inst.iface;

    if(transport != NULL)
    {
        struct shell_freemaster *ctx = (struct shell_freemaster *)transport->ctx;

        /* Notify shell thread about receive event and transmit opportunity */
        if(ctx->evt_handler != NULL)
        {
            ctx->evt_handler(SHELL_TRANSPORT_EVT_RX_RDY, ctx->evt_handler_param);
            ctx->evt_handler(SHELL_TRANSPORT_EVT_TX_RDY, ctx->evt_handler_param);
        }
    }
}

/******************************************************************************
 * FreeMASTER Shell public API
 */

int FMSTR_PipeEnableShell(FMSTR_PIPE_PORT pipe_port, uint32_t log_level)
{
    static char rxbuf[CONFIG_FMSTR_SHELL_PIPE_RXBUF_SIZE];
    static char txbuf[CONFIG_FMSTR_SHELL_PIPE_TXBUF_SIZE];

    /* FreeMASTER pipe console is quite primitive text view. No support for colors
       or VT100 control characters. A more advanced terminal can be used via
       telnet to localhost FreeMASTER pipe. See pipe properties in the
       FreeMASTER application.
    */
    static const struct shell_backend_config_flags cfg_flags =
    {
        .insert_mode = 0,
        .echo        = 1,
        .obscure     = 0,
        .mode_delete = 0,
        .use_colors  = 0,
        .use_vt100   = 0,
    };

    static struct shell_freemaster_config config =
    {
        .cfg_flags = cfg_flags,
        .rxbuf = rxbuf,
        .txbuf = txbuf,
        .rxbuf_size = sizeof(rxbuf),
        .txbuf_size = sizeof(txbuf),
    };

    config.port = pipe_port;

    return FMSTR_PipeEnableShellEx(&config, log_level);
}

int FMSTR_PipeEnableShellEx(const struct shell_freemaster_config* config, uint32_t log_level)
{
    bool log_backend = log_level > 0;

    if (log_level > CONFIG_LOG_MAX_LEVEL)
        log_level = CONFIG_LOG_MAX_LEVEL;

    return shell_init(&shell_freemaster_inst, config, config->cfg_flags, log_backend, log_level);
}

#else /* FMSTR_DISABLE */

int FMSTR_PipeEnableShell(FMSTR_PIPE_PORT pipe_port, uint32_t log_level)
{
    FMSTR_UNUSED(pipe_port);
    FMSTR_UNUSED(log_level);

    return 0;
}

int FMSTR_PipeEnableShellEx(const struct shell_freemaster_config* config, uint32_t log_level)
{
    FMSTR_UNUSED(config);
    FMSTR_UNUSED(log_level);

    return 0;
}

#endif /* FMSTR_DISABLE */
