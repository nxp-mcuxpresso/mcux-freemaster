/*
 * Copyright 2024-2025 NXP
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
 * FreeMASTER Communication Driver - CAN low-level driver
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>

#include "freemaster.h"
#include "freemaster_private.h"
#include "freemaster_can.h"
#include "freemaster_zephyr_can.h"

/******************************************************************************
 * Check configuration
 ******************************************************************************/

#if FMSTR_DISABLE == 0

#if defined(FMSTR_POLL_DRIVEN) && FMSTR_POLL_DRIVEN != 0
#error The Zephyr driver does not support poll driven mode.
#endif

#if (!defined CONFIG_CAN) || CONFIG_CAN == 0
#error FreeMASTER Zephyr CAN driver requires CONFIG_CAN feature enabled in Kconfig.
#endif

/******************************************************************************
 * Adapter configuration
 ******************************************************************************/

/* Get CAN peripheral to use. User defines chosen freemaster,can or alias in DTS/overlay. */
#if DT_NODE_EXISTS(DT_CHOSEN(freemaster_can))
#define DEV_FREEMASTER_CAN DEVICE_DT_GET(DT_CHOSEN(freemaster_can))
#elif DT_NODE_EXISTS(DT_ALIAS(freemaster_can))
#define DEV_FREEMASTER_CAN DEVICE_DT_GET(DT_ALIAS(freemaster_can))
#else
#define DEV_FREEMASTER_CAN DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus))
#endif

/******************************************************************************
 * Local variables
 ******************************************************************************/

/* CAN bus to use */
#ifdef DEV_FREEMASTER_CAN
static const struct device *fmstr_canDev = DEV_FREEMASTER_CAN;
#else
static const struct device *fmstr_canDev = NULL;
#endif

/* CAN mode */
static can_mode_t fmstr_canmode = CAN_MODE_NORMAL;
/* Rx filter for fmstr messages */
static struct can_filter fmstr_rxfilter;
/* Frame for fmstr messages to be sent */
static struct can_frame fmstr_txmsg;
/* Received frame buffer, valid when dlc > 0 */
static struct can_frame fmstr_rxmsg;

/******************************************************************************
 * Local functions prototypes
 ******************************************************************************/

/* Interface functions */
static FMSTR_BOOL _FMSTR_CanZephyrInit(FMSTR_U32 idRx,
                                       FMSTR_U32 idTx);              /* Initialize CAN module on a given base address. */
static void _FMSTR_CanZephyrEnableTxInterrupt(FMSTR_BOOL enable);    /* Enable CAN Transmit interrupt. */
static void _FMSTR_CanZephyrEnableRxInterrupt(FMSTR_BOOL enable);    /* Enable CAN Receive interrupt. */
static void _FMSTR_CanZephyrEnableRx(void);                          /* Enable/re-initialize Receiver buffer. */
static FMSTR_SIZE8 _FMSTR_CanZephyrGetRxFrameLen(void);              /* Return size of received CAN frame. */
static FMSTR_BCHR _FMSTR_CanZephyrGetRxFrameByte(FMSTR_SIZE8 index); /* Get data byte at index (0..8). */
static void _FMSTR_CanZephyrAckRxFrame(void);                        /* Discard received frame and enable receiving a next one. */
static FMSTR_BOOL _FMSTR_CanZephyrPrepareTxFrame(void);              /* Initialize transmit buffer. */
static void _FMSTR_CanZephyrPutTxFrameByte(FMSTR_SIZE8 index, FMSTR_BCHR data); /* Fill one byte of transmit data. */
static void _FMSTR_CanZephyrSendTxFrame(FMSTR_SIZE8 len);            /* Send the Tx buffer. */
static void _FMSTR_CanZephyrPoll(void);                              /* General poll call (optional) */
static void _FMSTR_CanZephyrGetCaps(FMSTR_CAN_IF_CAPS *caps);        /* Get driver capabilities (optional) */

/* IRQ callbacks */
static void _FMSTR_CanZephyrRxCallback(const struct device *dev, struct can_frame *frame, void *user_data); /* CAN Rx data callback */
static void _FMSTR_CanZephyrTxCallback(const struct device *dev, int error, void *user_data); /* CAN Tx data callback */

/******************************************************************************
 * Global API functions
 ******************************************************************************/

const struct device* FMSTR_CanGetDevice()
{
    return fmstr_canDev;
}

void FMSTR_CanSetDevice(const struct device * dev)
{
    fmstr_canDev = dev;
}

/***********************************
 *  Driver interface
 ***********************************/

const FMSTR_CAN_DRV_INTF FMSTR_ZEPHYR_CAN_DRV = {
    .Init              = _FMSTR_CanZephyrInit,
    .EnableTxInterrupt = _FMSTR_CanZephyrEnableTxInterrupt,
    .EnableRxInterrupt = _FMSTR_CanZephyrEnableRxInterrupt,
    .EnableRx          = _FMSTR_CanZephyrEnableRx,
    .GetRxFrameLen     = _FMSTR_CanZephyrGetRxFrameLen,
    .GetRxFrameByte    = _FMSTR_CanZephyrGetRxFrameByte,
    .AckRxFrame        = _FMSTR_CanZephyrAckRxFrame,
    .PrepareTxFrame    = _FMSTR_CanZephyrPrepareTxFrame,
    .PutTxFrameByte    = _FMSTR_CanZephyrPutTxFrameByte,
    .SendTxFrame       = _FMSTR_CanZephyrSendTxFrame,
    .Poll              = _FMSTR_CanZephyrPoll,
    .GetCaps           = _FMSTR_CanZephyrGetCaps
};

/******************************************************************************
 * Implementation
 ******************************************************************************/

/******************************************************************************
 *
 * @brief    CAN communication initialization
 *
 ******************************************************************************/
static FMSTR_BOOL _FMSTR_CanZephyrInit(FMSTR_U32 idRx, FMSTR_U32 idTx)
{
    /* Do not allow second initialization of the driver */
    if (FMSTR_IsDriverInitialized())
        return FMSTR_FALSE;

    FMSTR_MemSet(&fmstr_rxmsg, 0, sizeof(fmstr_rxmsg));
    FMSTR_MemSet(&fmstr_txmsg, 0, sizeof(fmstr_txmsg));

    /* Initialize Rx filter for fmstr messages */
    fmstr_rxfilter.id = idRx & CAN_EXT_ID_MASK;
    fmstr_rxfilter.flags = (idRx & FMSTR_CAN_EXTID) != 0U ? CAN_FILTER_IDE : 0U;
    fmstr_rxfilter.mask = (idRx & FMSTR_CAN_EXTID) != 0U ? CAN_EXT_ID_MASK : CAN_STD_ID_MASK;

    /* Initialize frame for fmstr messages to be sent */
    fmstr_txmsg.id = idTx & CAN_EXT_ID_MASK;
    fmstr_txmsg.flags = (idTx & FMSTR_CAN_EXTID) != 0U ? CAN_FILTER_IDE : 0U;

    FMSTR_BOOL ok = (fmstr_canDev != NULL) ? FMSTR_TRUE : FMSTR_FALSE;
    if (ok)
    {
        /* Check CAN bus */
        while(!device_is_ready(fmstr_canDev));

        /* Get mode of the CAN controller */
        can_get_capabilities(fmstr_canDev, &fmstr_canmode);

        /* Start the CAN controller */
        int err = can_start(fmstr_canDev);
        if (err < 0 && err != -EALREADY)
            ok = FMSTR_FALSE;

        /* Add Rx filter */
        if (can_add_rx_filter(fmstr_canDev, _FMSTR_CanZephyrRxCallback, NULL, &fmstr_rxfilter) < 0)
            ok = FMSTR_FALSE;
    }

    /* Notify finished driver initialization,
       communication and protocol processing can be started */
    FMSTR_NotifyDriverInitDone();

    return ok;
}

/******************************************************************************
 *
 * @brief    Enable/Disable interrupt from transmit register
 *
 ******************************************************************************/

static void _FMSTR_CanZephyrEnableTxInterrupt(FMSTR_BOOL enable)
{
    FMSTR_UNUSED(enable);
}

/******************************************************************************
 *
 * @brief    Enable/Disable interrupt from receive register
 *
 ******************************************************************************/

static void _FMSTR_CanZephyrEnableRxInterrupt(FMSTR_BOOL enable)
{
    FMSTR_UNUSED(enable);
}

/******************************************************************************
 *
 * @brief    Enable/Disable CAN receiver
 *
 ******************************************************************************/

static void _FMSTR_CanZephyrEnableRx(void)
{
}

/******************************************************************************
 *
 * @brief    Return size of received CAN frame, or 0 if no Rx frame is available
 *
 ******************************************************************************/

static FMSTR_SIZE8 _FMSTR_CanZephyrGetRxFrameLen(void)
{
    return (FMSTR_SIZE8)can_dlc_to_bytes(fmstr_rxmsg.dlc);
}

/******************************************************************************
 *
 * @brief    Get data byte at index (0..8)
 *
 ******************************************************************************/

static FMSTR_BCHR _FMSTR_CanZephyrGetRxFrameByte(FMSTR_SIZE8 index)
{
    return (FMSTR_BCHR)fmstr_rxmsg.data[index];
}

/******************************************************************************
 *
 * @brief   Discard received frame and enable receiving a next one
 *
 ******************************************************************************/

static void _FMSTR_CanZephyrAckRxFrame(void)
{
    fmstr_rxmsg.dlc = 0U;
}

/******************************************************************************
 *
 * @brief   Check transmit frame; return false when transmit frame is not available.
 *
 ******************************************************************************/

static FMSTR_BOOL _FMSTR_CanZephyrPrepareTxFrame(void)
{
    if (fmstr_txmsg.dlc > 0U)
    {
        return FMSTR_FALSE;
    }

    return FMSTR_TRUE;
}

/******************************************************************************
 *
 * @brief   Fill one byte of transmit data
 *
 ******************************************************************************/

static void _FMSTR_CanZephyrPutTxFrameByte(FMSTR_SIZE8 index, FMSTR_BCHR data)
{
    fmstr_txmsg.data[index] = data;
}

/******************************************************************************
 *
 * @brief   Send transmit data
 *
 ******************************************************************************/

static void _FMSTR_CanZephyrSendTxFrame(FMSTR_SIZE8 len)
{
    fmstr_txmsg.dlc = can_bytes_to_dlc(len);

    if (fmstr_canmode & CAN_MODE_FD)
    {
        fmstr_txmsg.flags |= CAN_FRAME_FDF;

#if defined(FMSTR_CANFD_USE_BRS) && FMSTR_CANFD_USE_BRS != 0
        fmstr_txmsg.flags |= CAN_FRAME_BRS;
#endif
    }

    can_send(fmstr_canDev, &fmstr_txmsg, K_MSEC(10), _FMSTR_CanZephyrTxCallback, NULL);
}

/******************************************************************************
 *
 * @brief    Poll function waits for interrupt (long ISR) or for
 * new data event from interrupt receive data callback function (short ISR).
 *
 ******************************************************************************/

static void _FMSTR_CanZephyrPoll(void)
{
#if FMSTR_LONG_INTR > 0
    /* Put calling thread to sleep for 10ms.
       Cannot sleep forever, the debug transmission can follow in upper layer. */
    k_sleep(K_MSEC(10));
#endif

#if FMSTR_SHORT_INTR > 0
    /* Wait (10ms) for new data to be processed.
       Cannot sleep forever, the debug transmission can follow in upper layer. */
    FMSTR_WaitEvents(FMSTR_EVENT_DATA_AVAILABLE, true, 10);
#endif
}

/******************************************************************************
 *
 * @brief    Get CAN driver capabilities.
 *
 ******************************************************************************/
static void _FMSTR_CanZephyrGetCaps(FMSTR_CAN_IF_CAPS *caps)
{
    FMSTR_ASSERT(caps != NULL);

    caps->flags = 0;

    if ((fmstr_canmode & CAN_MODE_FD) != 0)
    {
        caps->flags |= FMSTR_CAN_IF_CAPS_FLAG_FD;
    }
}

/******************************************************************************
 *
 * @brief IRQ callback function for received data
 *
 ******************************************************************************/

static void _FMSTR_CanZephyrRxCallback(const struct device *dev, struct can_frame *frame, void *user_data)
{
    fmstr_rxmsg = *frame;

#if FMSTR_LONG_INTR > 0 || FMSTR_SHORT_INTR > 0
    /* Process received frame */
    FMSTR_ProcessCanRx();
#endif

#if FMSTR_SHORT_INTR > 0
    /* Notify thread processing data*/
    FMSTR_PostEvents(FMSTR_EVENT_DATA_AVAILABLE);
#endif
}

/******************************************************************************
 *
 * @brief IRQ callback function for transmitted data
 *
 ******************************************************************************/

static void _FMSTR_CanZephyrTxCallback(const struct device *dev, int error, void *user_data)
{
    fmstr_txmsg.dlc = 0U;

    FMSTR_ProcessCanTx();
}

#endif /* (!(FMSTR_DISABLE)) */