/*
 * Copyright 2024-2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * FreeMASTER Communication Driver - Zephyr autoconf to freemaster_cfg converter
 */

#ifndef __FREEMASTER_AUTOCONF_H
#define __FREEMASTER_AUTOCONF_H

/*****************************************************************
* Define FreeMASTER configuration using Zephyr autoconf settings
*****************************************************************/

#define FMSTR_PLATFORM_ZEPHYR 1

/* Disabling the FreeMASTER functionality while leaving all API calls valid */
#ifdef CONFIG_FMSTR_DISABLE
#define FMSTR_DISABLE CONFIG_FMSTR_DISABLE
#endif

/* Interrupt or poll-driven serial communication */
#ifdef CONFIG_FMSTR_LONG_INTR
#define FMSTR_LONG_INTR   CONFIG_FMSTR_LONG_INTR
#endif
#ifdef CONFIG_FMSTR_SHORT_INTR
#define FMSTR_SHORT_INTR  CONFIG_FMSTR_SHORT_INTR
#endif
#ifdef CONFIG_FMSTR_POLL_DRIVEN
#define FMSTR_POLL_DRIVEN CONFIG_FMSTR_POLL_DRIVEN
#endif

/* Communication transport interface */

/* */
#if CONFIG_FMSTR_SERIAL_ZEPHYR_UART || CONFIG_FMSTR_SERIAL_ZEPHYR_USB
    #define FMSTR_TRANSPORT  FMSTR_SERIAL
    #define FMSTR_SERIAL_DRV FMSTR_ZEPHYR_SERIAL_DRV

    #ifdef CONFIG_FMSTR_COMM_RQUEUE_SIZE
    #define FMSTR_COMM_RQUEUE_SIZE CONFIG_FMSTR_COMM_RQUEUE_SIZE
    #endif

#elif CONFIG_FMSTR_CAN_ZEPHYR_GENERIC
    #define FMSTR_TRANSPORT  FMSTR_CAN
    #define FMSTR_CAN_DRV    FMSTR_ZEPHYR_CAN_DRV

    #ifdef CONFIG_FMSTR_CAN_CMDID
    #define FMSTR_CAN_CMDID CONFIG_FMSTR_CAN_CMDID
    #endif

    #ifdef CONFIG_FMSTR_CAN_RESPID
    #define FMSTR_CAN_RESPID CONFIG_FMSTR_CAN_RESPID
    #endif

    #ifdef CONFIG_FMSTR_CANFD_USE_BRS
    #define FMSTR_CANFD_USE_BRS CONFIG_FMSTR_CANFD_USE_BRS
    #endif

#elif CONFIG_FMSTR_NET_ZEPHYR_TCP
    #define FMSTR_TRANSPORT  FMSTR_NET
    #define FMSTR_NET_DRV    FMSTR_ZEPHYR_TCP_DRV

#elif CONFIG_FMSTR_NET_ZEPHYR_UDP
    #define FMSTR_TRANSPORT  FMSTR_NET
    #define FMSTR_NET_DRV    FMSTR_ZEPHYR_UDP_DRV

#elif CONFIG_FMSTR_SEGGER_RTT_ZEPHYR
    #define FMSTR_TRANSPORT  FMSTR_NET
    #define FMSTR_NET_DRV    FMSTR_ZEPHYR_RTT_DRV

#elif CONFIG_FMSTR_PDBDM_ZEPHYR
    #define FMSTR_TRANSPORT  FMSTR_PDBDM

#else
#error Missing Zephyr CONFIG option to select FreeMASTER communication interface
#endif

/* Network-specific communication options */
#if CONFIG_FMSTR_NET_ZEPHYR_TCP || CONFIG_FMSTR_NET_ZEPHYR_UDP
    #ifdef CONFIG_FMSTR_NET_PORT
    #define FMSTR_NET_PORT             CONFIG_FMSTR_NET_PORT             /* FreeMASTER server port number (used for both TCP or UDP) */
    #endif
    #ifdef CONFIG_FMSTR_NET_BLOCKING_TIMEOUT
    #define FMSTR_NET_BLOCKING_TIMEOUT CONFIG_FMSTR_NET_BLOCKING_TIMEOUT /* Blocking timeout (ms) of network calls used in FMSTR_Poll */
    #endif
    #ifdef CONFIG_FMSTR_SESSION_COUNT
    #define FMSTR_SESSION_COUNT        CONFIG_FMSTR_SESSION_COUNT        /* Count of allowed sessions */
    #endif
    #ifdef CONFIG_FMSTR_NET_AUTODISCOVERY
    #define FMSTR_NET_AUTODISCOVERY    CONFIG_FMSTR_NET_AUTODISCOVERY    /* Enable automatic board discovery via UDP protocol */
    #endif
#endif

/* Common communication options */
#ifdef CONFIG_FMSTR_COMM_BUFFER_SIZE
#define FMSTR_COMM_BUFFER_SIZE     CONFIG_FMSTR_COMM_BUFFER_SIZE
#else
#define FMSTR_COMM_BUFFER_SIZE     0  /* Input/output communication buffer size; 0 for "automatic" */
#endif

/* Support for Application Commands */
#ifdef CONFIG_FMSTR_USE_APPCMD
#define FMSTR_USE_APPCMD       CONFIG_FMSTR_USE_APPCMD       /* Enable/disable App.Commands support */
#endif
#ifdef CONFIG_FMSTR_APPCMD_BUFF_SIZE
#define FMSTR_APPCMD_BUFF_SIZE CONFIG_FMSTR_APPCMD_BUFF_SIZE /* App.Command data buffer size */
#endif
#ifdef CONFIG_FMSTR_MAX_APPCMD_CALLS
#define FMSTR_MAX_APPCMD_CALLS CONFIG_FMSTR_MAX_APPCMD_CALLS /* How many app.cmd callbacks? (0=disable) */
#endif


/* Oscilloscope support */
#ifdef CONFIG_FMSTR_USE_SCOPE
#define FMSTR_USE_SCOPE       CONFIG_FMSTR_USE_SCOPE       /* Number of supported oscilloscopes */
#endif
#ifdef CONFIG_FMSTR_MAX_SCOPE_VARS
#define FMSTR_MAX_SCOPE_VARS  CONFIG_FMSTR_MAX_SCOPE_VARS  /* Maximum number of scope variables per one oscilloscope */
#endif

/* Recorder support */
#ifdef CONFIG_FMSTR_USE_RECORDER
#define FMSTR_USE_RECORDER    CONFIG_FMSTR_USE_RECORDER    /* Number of supported recorders */
#endif
#ifdef CONFIG_FMSTR_REC_BUFF_SIZE
#define FMSTR_REC_BUFF_SIZE   CONFIG_FMSTR_REC_BUFF_SIZE   /* Built-in buffer size of recorder #0. Set to 0 to use runtime settings. */
#endif
#ifdef CONFIG_FMSTR_REC_TIMEBASE_us
#define FMSTR_REC_TIMEBASE    FMSTR_REC_BASE_MICROSEC(CONFIG_FMSTR_REC_TIMEBASE_us) /* 0 = "unknown" */
#endif
#ifdef CONFIG_FMSTR_REC_FLOAT_TRIG
#define FMSTR_REC_FLOAT_TRIG  CONFIG_FMSTR_REC_FLOAT_TRIG  /* Enable/disable floating point triggering */
#endif

/* Target-side address translation (TSA) */
#if defined CONFIG_FMSTR_USE_TSA
#define FMSTR_USE_TSA         CONFIG_FMSTR_USE_TSA         /* Enable TSA functionality */
#endif
#ifdef CONFIG_FMSTR_USE_TSA_INROM
#define FMSTR_USE_TSA_INROM   CONFIG_FMSTR_USE_TSA_INROM   /* TSA tables declared as const (put to ROM) */
#endif
#ifdef CONFIG_FMSTR_USE_TSA_SAFETY
#define FMSTR_USE_TSA_SAFETY  CONFIG_FMSTR_USE_TSA_SAFETY  /* Enable/Disable TSA memory protection */
#endif
#ifdef CONFIG_FMSTR_USE_TSA_DYNAMIC
#define FMSTR_USE_TSA_DYNAMIC CONFIG_FMSTR_USE_TSA_DYNAMIC /* Enable/Disable TSA entries to be added also in runtime */
#endif
#ifdef CONFIG_FMSTR_TSA_USER_TABLES
#define FMSTR_TSA_USER_TABLES CONFIG_FMSTR_TSA_USER_TABLES /* Enable/Disable TSA user tables (note: Zephyr needs it disabled)*/
#endif

/* Pipes as data streaming over FreeMASTER protocol */
#ifdef CONFIG_FMSTR_USE_PIPES
#define FMSTR_USE_PIPES       CONFIG_FMSTR_USE_PIPES       /* Number of supported pipe objects */
#endif
#ifdef CONFIG_FMSTR_USE_PIPE_PRINTF
#define FMSTR_USE_PIPE_PRINTF CONFIG_FMSTR_USE_PIPE_PRINTF /* Enable pipe printf functions */
#endif
#ifdef CONFIG_FMSTR_USE_PIPE_PRINTF_VARG
#define FMSTR_USE_PIPE_PRINTF_VARG CONFIG_FMSTR_USE_PIPE_PRINTF /* "pipe" variable-argument printf */
#endif
#ifdef CONFIG_FMSTR_PIPES_PRINTF_BUFF_SIZE
#define FMSTR_PIPES_PRINTF_BUFF_SIZE CONFIG_FMSTR_PIPES_PRINTF_BUFF_SIZE  /* pipe printf buffer size */
#endif

/* Enable/Disable read/write memory commands */
#ifdef CONFIG_FMSTR_USE_READMEM
#define FMSTR_USE_READMEM      CONFIG_FMSTR_USE_READMEM      /* Enable read memory commands */
#endif
#ifdef CONFIG_FMSTR_USE_WRITEMEM
#define FMSTR_USE_WRITEMEM     CONFIG_FMSTR_USE_WRITEMEM     /* Enable write memory commands */
#endif
#ifdef CONFIG_FMSTR_USE_WRITEMEMMASK
#define FMSTR_USE_WRITEMEMMASK CONFIG_FMSTR_USE_WRITEMEMMASK /* Enable write memory bits commands */
#endif

/* Application name, description etc.*/
#ifdef CONFIG_FMSTR_APPLICATION_STR
#define FMSTR_APPLICATION_STR CONFIG_FMSTR_APPLICATION_STR
#endif
#ifdef CONFIG_FMSTR_DESCRIPTION_STR
#define FMSTR_DESCRIPTION_STR CONFIG_FMSTR_DESCRIPTION_STR
#endif
#ifdef CONFIG_FMSTR_BUILDTIME_STR
#define FMSTR_BUILDTIME_STR CONFIG_FMSTR_BUILDTIME_STR
#endif

/* Access level passwords */
#ifdef CONFIG_FMSTR_RESTRICTED_ACCESS_R_PASSWORD
#define FMSTR_RESTRICTED_ACCESS_R_PASSWORD    CONFIG_FMSTR_RESTRICTED_ACCESS_R_PASSWORD
#endif
#ifdef CONFIG_FMSTR_RESTRICTED_ACCESS_RW_PASSWORD
#define FMSTR_RESTRICTED_ACCESS_RW_PASSWORD   CONFIG_FMSTR_RESTRICTED_ACCESS_RW_PASSWORD
#endif
#ifdef CONFIG_FMSTR_RESTRICTED_ACCESS_RWF_PASSWORD
#define FMSTR_RESTRICTED_ACCESS_RWF_PASSWORD  CONFIG_FMSTR_RESTRICTED_ACCESS_RWF_PASSWORD
#endif

#ifdef CONFIG_FMSTR_USE_HASHED_PASSWORDS
#define FMSTR_USE_HASHED_PASSWORDS CONFIG_FMSTR_USE_HASHED_PASSWORDS
#endif

/* Communication debugging */
#ifdef CONFIG_FMSTR_DEBUG_TX
#define FMSTR_DEBUG_TX CONFIG_FMSTR_DEBUG_TX
#endif

/* Debugging logging level */
#ifdef CONFIG_FMSTR_DEBUG_LEVEL
#define FMSTR_DEBUG_LEVEL CONFIG_FMSTR_DEBUG_LEVEL
#endif

/* Driver debugging (0=none, 1=errors, 2=normal, 3=verbose) */
#if defined(FMSTR_DEBUG_LEVEL) && FMSTR_DEBUG_LEVEL > 0
#include <zephyr/logging/log.h>
#define FMSTR_DEBUG_PRINTF(...) LOG_PRINTK(__VA_ARGS__)
#endif

#endif /* __FREEMASTER_AUTOCONF_H */
