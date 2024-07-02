/*
 * Copyright (c) 2007-2015 Freescale Semiconductor, Inc.
 * Copyright 2018-2019, 2024 NXP
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
 * FreeMASTER Communication Driver - Serial low-level driver
 */

#include "freemaster.h"
#include "freemaster_private.h"

/* Compile this code only if the SERIAL driver is selected in freemaster_cfg.h. */
#if defined(FMSTR_SERIAL_DRV) && (FMSTR_MK_IDSTR(FMSTR_SERIAL_DRV) == FMSTR_SERIAL_DREG_KXX_UART_ID)
#if !(FMSTR_DISABLE) 

#include "freemaster_protocol.h"
#include "freemaster_serial.h"
#include "freemaster_serial_uart.h"

/***********************************
*  local variables
***********************************/

/* Serial base address */
#ifdef FMSTR_SERIAL_BASE
    static FMSTR_UART_Type *fmstr_serialBaseAddr = FMSTR_SERIAL_BASE;
#else
    static FMSTR_UART_Type *fmstr_serialBaseAddr = NULL;
#endif

/***********************************
*  local function prototypes
***********************************/

/* Interface function - Initialization of serial UART driver adapter */
static FMSTR_BOOL _FMSTR_SerialUartInit(void);
static void _FMSTR_SerialUartEnableTransmit(FMSTR_BOOL enable);
static void _FMSTR_SerialUartEnableReceive(FMSTR_BOOL enable);
static void _FMSTR_SerialUartEnableTransmitInterrupt(FMSTR_BOOL enable);
static void _FMSTR_SerialUartEnableTransmitCompleteInterrupt(FMSTR_BOOL enable);
static void _FMSTR_SerialUartEnableReceiveInterrupt(FMSTR_BOOL enable);
static FMSTR_BOOL _FMSTR_SerialUartIsTransmitRegEmpty(void);
static FMSTR_BOOL _FMSTR_SerialUartIsReceiveRegFull(void);
static FMSTR_BOOL _FMSTR_SerialUartIsTransmitterActive(void);
static void _FMSTR_SerialUartPutChar(FMSTR_BCHR  ch);
static FMSTR_BCHR _FMSTR_SerialUartGetChar(void);
static void _FMSTR_SerialUartFlush(void);

/***********************************
*  global variables
***********************************/
/* Interface of this serial UART driver */
const FMSTR_SERIAL_DRV_INTF FMSTR_SERIAL_DREG_KXX_UART =
{
    .Init                           = _FMSTR_SerialUartInit,
    .EnableTransmit                 = _FMSTR_SerialUartEnableTransmit,
    .EnableReceive                  = _FMSTR_SerialUartEnableReceive,
    .EnableTransmitInterrupt        = _FMSTR_SerialUartEnableTransmitInterrupt,
    .EnableTransmitCompleteInterrupt= _FMSTR_SerialUartEnableTransmitCompleteInterrupt,
    .EnableReceiveInterrupt         = _FMSTR_SerialUartEnableReceiveInterrupt,
    .IsTransmitRegEmpty             = _FMSTR_SerialUartIsTransmitRegEmpty,
    .IsReceiveRegFull               = _FMSTR_SerialUartIsReceiveRegFull,
    .IsTransmitterActive            = _FMSTR_SerialUartIsTransmitterActive,
    .PutChar                        = _FMSTR_SerialUartPutChar,
    .GetChar                        = _FMSTR_SerialUartGetChar,
    .Flush                          = _FMSTR_SerialUartFlush,

};

/******************************************************************************
*
* @brief    Serial communication initialization
*
******************************************************************************/
static FMSTR_BOOL _FMSTR_SerialUartInit(void)
{
    /* Valid runtime module address must be assigned */
    if(!fmstr_serialBaseAddr)
        return FMSTR_FALSE;
    
    /* WARNING: make sure you define the FSL_FEATURE_UART_HAS_FIFO macro when the 
       peripheral has the hardware FIFO buffer (or include the MCUXpresso SDK's "fsl_common.h").  */
#if defined(FSL_FEATURE_UART_HAS_FIFO) && FSL_FEATURE_UART_HAS_FIFO
    /* The current version can not work with enabled FIFO. Disable it. */
    fmstr_serialBaseAddr->PFIFO &= ~(UART_PFIFO_TXFE_MASK | UART_PFIFO_RXFE_MASK);
#endif

#if FMSTR_SERIAL_SINGLEWIRE
    /* Enable single wire mode and force TX and RX to be enabled all the time. */
    fmstr_serialBaseAddr->C1 |= FMSTR_UART_C1_LOOPS_MASK | FMSTR_UART_C1_RSRC_MASK;
    _FMSTR_SerialUartEnableTransmit(1);
    _FMSTR_SerialUartEnableReceive(1);
#endif

    return FMSTR_TRUE;
}

/******************************************************************************
*
* @brief    Enable/Disable Serial transmitter
*
******************************************************************************/

static void _FMSTR_SerialUartEnableTransmit(FMSTR_BOOL enable)
{
#if FMSTR_SERIAL_SINGLEWIRE
    /* In single-wire mode, the Transmitter is never physically disabled since it was
       enabled in the Init() call. The TX pin direction is being changed only. */
    if (enable)
        fmstr_serialBaseAddr->C3 |= FMSTR_UART_C3_TXDIR_MASK;
    else
        fmstr_serialBaseAddr->C3 &= ~FMSTR_UART_C3_TXDIR_MASK;
#else
    /* In normal mode (or "external" single-wire mode) the Transmitter is controlled. */
    if (enable)
        fmstr_serialBaseAddr->C2 |= FMSTR_UART_C2_TE_MASK;
    else
        fmstr_serialBaseAddr->C2 &= ~FMSTR_UART_C2_TE_MASK;
#endif
}

/******************************************************************************
*
* @brief    Enable/Disable Serial receiver
*
******************************************************************************/

static void _FMSTR_SerialUartEnableReceive(FMSTR_BOOL enable)
{
#if FMSTR_SERIAL_SINGLEWIRE
    /* In single-wire mode, the Receiver is never physically disabled since it was
       enabled in the Init() call. The TX pin direction is being changed only. */
    FMSTR_UNUSED(enable);
#else
    /* In normal mode (or "external" single-wire mode) the Receiver is controlled. */
    if (enable)
        fmstr_serialBaseAddr->C2 |= FMSTR_UART_C2_RE_MASK;
    else
        fmstr_serialBaseAddr->C2 &= ~FMSTR_UART_C2_RE_MASK;
#endif    
}

/******************************************************************************
*
* @brief    Enable/Disable interrupt from transmit register empty event
*
******************************************************************************/

static void _FMSTR_SerialUartEnableTransmitInterrupt(FMSTR_BOOL enable)
{
    if(enable)
        fmstr_serialBaseAddr->C2 |= FMSTR_UART_C2_TIE_MASK;
    else
        fmstr_serialBaseAddr->C2 &= ~FMSTR_UART_C2_TIE_MASK;
}

/******************************************************************************
*
* @brief    Enable/Disable interrupt from transmit complete event
*
******************************************************************************/

static void _FMSTR_SerialUartEnableTransmitCompleteInterrupt(FMSTR_BOOL enable)
{
    if(enable)
        fmstr_serialBaseAddr->C2 |= FMSTR_UART_C2_TCIE_MASK;
    else
        fmstr_serialBaseAddr->C2 &= ~FMSTR_UART_C2_TCIE_MASK;
}

/******************************************************************************
*
* @brief    Enable/Disable interrupt from receive register full event
*
******************************************************************************/

static void _FMSTR_SerialUartEnableReceiveInterrupt(FMSTR_BOOL enable)
{
    if(enable)
        fmstr_serialBaseAddr->C2 |= FMSTR_UART_C2_RIE_MASK;
    else
        fmstr_serialBaseAddr->C2 &= ~FMSTR_UART_C2_RIE_MASK;
}

/******************************************************************************
*
* @brief    Returns TRUE if the transmit register is empty, and it's possible to put next char
*
******************************************************************************/

static FMSTR_BOOL _FMSTR_SerialUartIsTransmitRegEmpty(void)
{
    return fmstr_serialBaseAddr->S1 & FMSTR_UART_S1_TDRE_MASK;
}

/******************************************************************************
*
* @brief    Returns TRUE if the receive register is full, and it's possible to get received char
*
******************************************************************************/

static FMSTR_BOOL _FMSTR_SerialUartIsReceiveRegFull(void)
{
    return fmstr_serialBaseAddr->S1 & FMSTR_UART_S1_RDRF_MASK;
}

/******************************************************************************
*
* @brief    Returns TRUE if the transmitter is still active
*
******************************************************************************/

static FMSTR_BOOL _FMSTR_SerialUartIsTransmitterActive(void)
{
    return !(fmstr_serialBaseAddr->S1 & FMSTR_UART_S1_TC_MASK);
}

/******************************************************************************
*
* @brief    The function puts the char for transmit
*
******************************************************************************/

static void _FMSTR_SerialUartPutChar(FMSTR_BCHR  ch)
{
    fmstr_serialBaseAddr->D = ch;
}

/******************************************************************************
*
* @brief    The function gets the received char
*
******************************************************************************/
static FMSTR_BCHR _FMSTR_SerialUartGetChar(void)
{
    return fmstr_serialBaseAddr->D;
}

/******************************************************************************
*
* @brief    The function sends buffered data
*
******************************************************************************/
static void _FMSTR_SerialUartFlush(void)
{
}

/******************************************************************************
*
* @brief    Assigning FreeMASTER communication module base address
*
******************************************************************************/
void FMSTR_SerialSetBaseAddress(void *base)
{
    fmstr_serialBaseAddr = (FMSTR_UART_Type*)base;
}

/******************************************************************************
*
* @brief    API: Interrupt handler call
*
* This Interrupt Service Routine handles the UART  interrupts for the FreeMASTER
* driver. In case you want to handle the interrupt in the application yourselves,
* call the FMSTR_ProcessSerial function which
* does the same job but is not compiled as an Interrupt Service Routine.
*
* In poll-driven mode (FMSTR_POLL_DRIVEN) this function does nothing.
*
******************************************************************************/

void FMSTR_SerialIsr(void)
{
    /* process incomming or just transmitted byte */
    #if (FMSTR_LONG_INTR) || (FMSTR_SHORT_INTR)
        FMSTR_ProcessSerial();
    #endif

}

#else /* !(FMSTR_DISABLE) */

void FMSTR_SerialSetBaseAddress(void *base)
{
    FMSTR_UNUSED(base);
}

void FMSTR_SerialIsr(void)
{
}

#endif /* !(FMSTR_DISABLE) */
#endif /* defined(FMSTR_SERIAL_DRV) && (FMSTR_MK_IDSTR(FMSTR_SERIAL_DRV) == FMSTR_SERIAL_DREG_KXX_UART_ID) */