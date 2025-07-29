/*
 * Copyright 2025 NXP
 *
 * License: NXP LA_OPT_Online Code Hosting NXP_Software_License
 *
 * NXP Proprietary. This software is owned or controlled by NXP and may
 * only be used strictly in accordance with the applicable license terms.
 * By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that
 * you have read, and that you agree to comply with and are bound by,
 * such license terms.  If you do not agree to be bound by the applicable
 * license terms, then you may not retain, install, activate or otherwise
 * use the software.
 *
 * FreeMASTER Communication Driver - RTT (Real Time Transfer) driver
 */

#include "freemaster.h"
#include "freemaster_private.h"
#include "freemaster_net.h"

#include "freemaster_zephyr_rtt.h"
#include "SEGGER_RTT.h"

/******************************************************************************
 * Check configuration
 ******************************************************************************/

#if FMSTR_DISABLE == 0

#ifndef FMSTR_NET_SEGGER_RTT_BUFFER_INDEX
#define FMSTR_NET_SEGGER_RTT_BUFFER_INDEX 0
#endif

#if (defined(FMSTR_SHORT_INTR) && FMSTR_SHORT_INTR) || (defined(FMSTR_LONG_INTR) && FMSTR_LONG_INTR)
#warning "The FreeMASTER RTT driver does not support interrupt mode."
#endif

#if !(defined CONFIG_USE_SEGGER_RTT) || CONFIG_USE_SEGGER_RTT == 0
#error FreeMASTER Zephyr RTT driver requires CONFIG_USE_SEGGER_RTT feature enabled in Kconfig.
#endif

/******************************************************************************
 * Local types
 ******************************************************************************/

/******************************************************************************
 * Local functions
 ******************************************************************************/

static FMSTR_BOOL _FMSTR_RttInit(void);
static void _FMSTR_RttPoll(void);
static FMSTR_S32 _FMSTR_RttRecv(FMSTR_BPTR msgBuff,
                                FMSTR_SIZE msgMaxSize,
                                FMSTR_NET_ADDR *recvAddr,
                                FMSTR_BOOL *isBroadcast);
static FMSTR_S32 _FMSTR_RttSend(FMSTR_NET_ADDR *sendAddr, FMSTR_BPTR msgBuff, FMSTR_SIZE msgSize);
static void _FMSTR_RttClose(FMSTR_NET_ADDR *addr);
static void _FMSTR_RttGetCaps(FMSTR_NET_IF_CAPS *caps);
static void _FMSTR_RttGetCaps(FMSTR_NET_IF_CAPS *caps);
static void _FMSTR_RttTimerHandler(struct k_timer *timer);

/******************************************************************************
 * Local variables
 ******************************************************************************/

static const FMSTR_NET_ADDR fmstr_rttDummyAddress = {
    .type = FMSTR_NET_ADDR_TYPE_V4, .port = 0, .addr.v4 = {0, 0, 0, 0}};

K_TIMER_DEFINE(fmstr_rttTimer, _FMSTR_RttTimerHandler, NULL);

/******************************************************************************
 * Driver interface
 ******************************************************************************/
/* Interface of this network TCP driver */
const FMSTR_NET_DRV_INTF FMSTR_ZEPHYR_RTT_DRV = {
    .Init    = _FMSTR_RttInit,
    .Poll    = _FMSTR_RttPoll,
    .Recv    = _FMSTR_RttRecv,
    .Send    = _FMSTR_RttSend,
    .Close   = _FMSTR_RttClose,
    .GetCaps = _FMSTR_RttGetCaps,
};

#define FMSTR_RTT_BUFF_SIZE (FMSTR_COMM_BUFFER_SIZE + 8)
#if FMSTR_NET_SEGGER_RTT_BUFFER_INDEX > 0
static FMSTR_U8 fmstr_rttBuffer[FMSTR_RTT_BUFF_SIZE];
#else
#if (FMSTR_RTT_BUFF_SIZE > BUFFER_SIZE_UP) || (FMSTR_RTT_BUFF_SIZE > BUFFER_SIZE_DOWN)
#error RTT buffer size configured too small! FreeMASTER frame would not fit.
#endif
#endif
/******************************************************************************
 * Implementation
 ******************************************************************************/

void _FMSTR_RttTimerHandler(struct k_timer *timer)
{
    if(SEGGER_RTT_HasData(FMSTR_NET_SEGGER_RTT_BUFFER_INDEX))
    {
        /* Notify new data is available */
        FMSTR_PostEvents(FMSTR_EVENT_DATA_AVAILABLE);
    }
}

static FMSTR_BOOL _FMSTR_RttInit(void)
{
    SEGGER_RTT_Init();
    SEGGER_RTT_SetTerminal(FMSTR_NET_SEGGER_RTT_BUFFER_INDEX);

#if FMSTR_NET_SEGGER_RTT_BUFFER_INDEX > 0
    SEGGER_RTT_ConfigUpBuffer(FMSTR_NET_SEGGER_RTT_BUFFER_INDEX, NULL, fmstr_rttBuffer, FMSTR_RTT_BUFF_SIZE,
                              SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    SEGGER_RTT_ConfigDownBuffer(FMSTR_NET_SEGGER_RTT_BUFFER_INDEX, NULL, fmstr_rttBuffer, FMSTR_RTT_BUFF_SIZE, 0);
#else
    SEGGER_RTT_ConfigUpBuffer(FMSTR_NET_SEGGER_RTT_BUFFER_INDEX, NULL, NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    SEGGER_RTT_ConfigDownBuffer(FMSTR_NET_SEGGER_RTT_BUFFER_INDEX, NULL, NULL, 0, 0);
#endif

    /* Start timer with specified period */
    k_timer_start(&fmstr_rttTimer,
        K_USEC(CONFIG_FMSTR_RTT_RX_POLL_PERIOD_us),
        K_USEC(CONFIG_FMSTR_RTT_RX_POLL_PERIOD_us));

    return FMSTR_TRUE;
}

static void _FMSTR_RttPoll(void)
{
    /* Wait (10ms) for new data to be processed */
    FMSTR_WaitEvents(FMSTR_EVENT_DATA_AVAILABLE, true, 10);
}

static FMSTR_S32 _FMSTR_RttRecv(FMSTR_BPTR msgBuff,
                                FMSTR_SIZE msgMaxSize,
                                FMSTR_NET_ADDR *recvAddr,
                                FMSTR_BOOL *isBroadcast)
{
    FMSTR_ASSERT(msgBuff != NULL);
    FMSTR_ASSERT(recvAddr != NULL);

    FMSTR_MemCpy(recvAddr, &fmstr_rttDummyAddress, sizeof(FMSTR_NET_ADDR));

    return SEGGER_RTT_Read(FMSTR_NET_SEGGER_RTT_BUFFER_INDEX, msgBuff, msgMaxSize);
}

static FMSTR_S32 _FMSTR_RttSend(FMSTR_NET_ADDR *sendAddr, FMSTR_BPTR msgBuff, FMSTR_SIZE msgSize)
{
    FMSTR_ASSERT(msgBuff != NULL);
    FMSTR_ASSERT(sendAddr != NULL);

    return SEGGER_RTT_Write(FMSTR_NET_SEGGER_RTT_BUFFER_INDEX, msgBuff, msgSize);
}

static void _FMSTR_RttClose(FMSTR_NET_ADDR *addr)
{
}

static void _FMSTR_RttGetCaps(FMSTR_NET_IF_CAPS *caps)
{
    FMSTR_ASSERT(caps != NULL);

    caps->flags |= FMSTR_NET_IF_CAPS_FLAG_RTT;
}

#endif /* (!(FMSTR_DISABLE)) */