/*
 * Copyright 2024 NXP
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

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

#include "freemaster.h"
#include "freemaster_private.h"
#include "freemaster_serial.h"
#include "freemaster_zephyr_serial.h"

/******************************************************************************
 * Check configuration
 ******************************************************************************/

#if FMSTR_DISABLE == 0

#if defined(FMSTR_POLL_DRIVEN) && FMSTR_POLL_DRIVEN
#error FreeMASTER Zephyr driver does not support the poll-driven mode.
#endif

#if !(defined CONFIG_SERIAL) || CONFIG_SERIAL == 0
#error FreeMASTER Zephyr serial driver requires CONFIG_SERIAL feature enabled in Kconfig.
#endif

#if !(defined CONFIG_UART_INTERRUPT_DRIVEN) || CONFIG_UART_INTERRUPT_DRIVEN == 0
#error FreeMASTER Zephyr serial driver requires CONFIG_UART_INTERRUPT_DRIVEN feature enabled in Kconfig.
#endif

#if defined(CONFIG_FMSTR_SERIAL_ZEPHYR_USB) && CONFIG_FMSTR_SERIAL_ZEPHYR_USB != 0
#if (!defined(CONFIG_USB_DEVICE_STACK) || CONFIG_USB_DEVICE_STACK == 0)
#error FreeMASTER Zephyr USB driver requires CONFIG_USB_DEVICE_STACK feature enabled in Kconfig.
#endif
#endif

#if defined(CONFIG_FMSTR_SERIAL_ZEPHYR_USB_NEXT) && CONFIG_FMSTR_SERIAL_ZEPHYR_USB_NEXT != 0
#if (!defined(CONFIG_USB_DEVICE_STACK_NEXT) || CONFIG_USB_DEVICE_STACK_NEXT == 0)
#error FreeMASTER Zephyr USB driver requires CONFIG_USB_DEVICE_STACK_NEXT feature enabled in Kconfig.
#endif
#endif


/******************************************************************************
 * Adapter configuration
 ******************************************************************************/

 /* USB stack */
#if defined(CONFIG_FMSTR_SERIAL_ZEPHYR_USB) && CONFIG_FMSTR_SERIAL_ZEPHYR_USB != 0

/* Get USB peripheral to use. User defines chosen freemaster,usb or alias in DTS/overlay. */
#if DT_NODE_EXISTS(DT_CHOSEN(freemaster_usb))
#define DEV_FREEMASTER_SERIAL DEVICE_DT_GET(DT_CHOSEN(freemaster_usb))
#elif DT_NODE_EXISTS(DT_ALIAS(freemaster_usb))
#define DEV_FREEMASTER_SERIAL DEVICE_DT_GET(DT_ALIAS(freemaster_usb))
#else
#define DEV_FREEMASTER_SERIAL DEVICE_DT_GET(zephyr_udc0)
#endif

/* USB device VID must be NXP */
#if CONFIG_USB_DEVICE_VID != 0x1FC9
#error FreeMASTER USB can only talk to NXP devices identified with CONFIG_USB_DEVICE_VID=0x1FC9
#endif

#endif /* CONFIG_FMSTR_SERIAL_ZEPHYR_USB */

/* USB stack NEXT */
#if defined(CONFIG_FMSTR_SERIAL_ZEPHYR_USB_NEXT) && CONFIG_FMSTR_SERIAL_ZEPHYR_USB_NEXT != 0

/* Get USB peripheral to use. User defines chosen freemaster,usb or alias in DTS/overlay. */
#if DT_NODE_EXISTS(DT_CHOSEN(freemaster_usb))
#define DEV_FREEMASTER_SERIAL DEVICE_DT_GET(DT_CHOSEN(freemaster_usb))
#elif DT_NODE_EXISTS(DT_ALIAS(freemaster_usb))
#define DEV_FREEMASTER_SERIAL DEVICE_DT_GET(DT_ALIAS(freemaster_usb))
#else
#define DEV_FREEMASTER_SERIAL DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart)
#endif

#endif /* CONFIG_FMSTR_SERIAL_ZEPHYR_USB_NEXT */

#if defined(CONFIG_FMSTR_SERIAL_ZEPHYR_UART) && CONFIG_FMSTR_SERIAL_ZEPHYR_UART != 0

/* Get UART peripheral to use. User defines chosen freemaster,uart or alias in DTS/overlay. */
#if DT_NODE_EXISTS(DT_CHOSEN(freemaster_uart))
#define DEV_FREEMASTER_SERIAL DEVICE_DT_GET(DT_CHOSEN(freemaster_uart))
#elif DT_NODE_EXISTS(DT_ALIAS(freemaster_uart))
#define DEV_FREEMASTER_SERIAL DEVICE_DT_GET(DT_ALIAS(freemaster_uart))
/* If not defined in DTS, FreeMASTER may also use a Zephyr console port for its communication. */
#else
#define DEV_FREEMASTER_SERIAL DEVICE_DT_GET(DT_CHOSEN(zephyr_console))
#if CONFIG_UART_CONSOLE == 0
#warning When using FreeMASTER over default Zephyr console, it must be set as UART type. Please set CONFIG_UART_CONSOLE.
#elif (CONFIG_PRINTK && !CONFIG_LOG_PRINTK) || (CONFIG_SHELL && CONFIG_SHELL_BACKEND_SERIAL) || (CONFIG_LOG && CONFIG_LOG_BACKEND_UART)
#warning FreeMASTER Zephyr serial driver will share UART console with Zephyr printk, log or shell, this will most likely NOT WORK.
#endif
#endif

#endif /* CONFIG_FMSTR_SERIAL_ZEPHYR_UART */

/***********************************
 *  Local variables
 ***********************************/

/* CAN bus to use */
#ifdef DEV_FREEMASTER_SERIAL
static const struct device *fmstr_serialDev = DEV_FREEMASTER_SERIAL;
#else
static const struct device *fmstr_serialDev = NULL;
#endif

/* Buffer for received byte */
static FMSTR_BCHR fmstr_rxChar;
/* True if one byte was received and was not read yet */
static FMSTR_BOOL fmstr_rxCharValid;

/***********************************
 *  Local function prototypes
 ***********************************/

/* Interface functions */
static FMSTR_BOOL _FMSTR_SerialZephyrInit(void);
static void _FMSTR_SerialZephyrEnableTransmit(FMSTR_BOOL enable);
static void _FMSTR_SerialZephyrEnableReceive(FMSTR_BOOL enable);
static void _FMSTR_SerialZephyrEnableTransmitInterrupt(FMSTR_BOOL enable);
static void _FMSTR_SerialZephyrEnableTransmitCompleteInterrupt(FMSTR_BOOL enable);
static void _FMSTR_SerialZephyrEnableReceiveInterrupt(FMSTR_BOOL enable);
static FMSTR_BOOL _FMSTR_SerialZephyrIsTransmitRegEmpty(void);
static FMSTR_BOOL _FMSTR_SerialZephyrIsReceiveRegFull(void);
static FMSTR_BOOL _FMSTR_SerialZephyrIsTransmitterActive(void);
static void _FMSTR_SerialZephyrPutChar(FMSTR_BCHR ch);
static FMSTR_BCHR _FMSTR_SerialZephyrGetChar(void);
static void _FMSTR_SerialZephyrFlush(void);
static void _FMSTR_SerialZephyrPoll(void);

/* IRQ callback */
static void _FMSTR_SerialZephyrIsr(const struct device *dev, void *user_data);

/******************************************************************************
 * Global API functions
 ******************************************************************************/

const struct device* FMSTR_SerialGetDevice()
{
    return fmstr_serialDev;
}

void FMSTR_SerialSetDevice(const struct device * dev)
{
    fmstr_serialDev = dev;
}

/***********************************
 *  Driver interface
 ***********************************/

const FMSTR_SERIAL_DRV_INTF FMSTR_ZEPHYR_SERIAL_DRV = {
    .Init                            = _FMSTR_SerialZephyrInit,
    .EnableTransmit                  = _FMSTR_SerialZephyrEnableTransmit,
    .EnableReceive                   = _FMSTR_SerialZephyrEnableReceive,
    .EnableTransmitInterrupt         = _FMSTR_SerialZephyrEnableTransmitInterrupt,
    .EnableTransmitCompleteInterrupt = _FMSTR_SerialZephyrEnableTransmitCompleteInterrupt,
    .EnableReceiveInterrupt          = _FMSTR_SerialZephyrEnableReceiveInterrupt,
    .IsTransmitRegEmpty              = _FMSTR_SerialZephyrIsTransmitRegEmpty,
    .IsReceiveRegFull                = _FMSTR_SerialZephyrIsReceiveRegFull,
    .IsTransmitterActive             = _FMSTR_SerialZephyrIsTransmitterActive,
    .PutChar                         = _FMSTR_SerialZephyrPutChar,
    .GetChar                         = _FMSTR_SerialZephyrGetChar,
    .Flush                           = _FMSTR_SerialZephyrFlush,
    .Poll                            = _FMSTR_SerialZephyrPoll,

};

/******************************************************************************
 * Implementation
 ******************************************************************************/

/******************************************************************************
 *
 * @brief    Serial communication initialization
 *
 ******************************************************************************/

static FMSTR_BOOL _FMSTR_SerialZephyrInit(void)
{
    /* Do not allow second initialization of the driver */
    if (FMSTR_IsDriverInitialized())
        return FMSTR_FALSE;

    /* Initialize flags */
    fmstr_rxCharValid = FMSTR_FALSE;

    FMSTR_BOOL ok = (fmstr_serialDev != NULL) ? FMSTR_TRUE : FMSTR_FALSE;
    if (ok)
    {
        /* Check serial device state */
        while(!device_is_ready(fmstr_serialDev));

#if (defined(CONFIG_FMSTR_SERIAL_ZEPHYR_USB) && CONFIG_FMSTR_SERIAL_ZEPHYR_USB != 0) \
        /* Enable USB, this step was only required in legacy USB stack,
        not needed in the latest Zephyr (4.2 and later) */
        int err = usb_enable(NULL);
        if (err < 0 && err != -EALREADY)
            ok = FMSTR_FALSE;
#endif

#if FMSTR_SHORT_INTR || FMSTR_LONG_INTR
        /* Set the IRQ callback function pointer */
        if (uart_irq_callback_user_data_set(fmstr_serialDev, _FMSTR_SerialZephyrIsr, NULL) != 0)
            ok = FMSTR_FALSE;

        /* Enable UART interrupts. */
        uart_irq_rx_enable(fmstr_serialDev);
#endif
    }

    /* Notify finished driver initialization,
       communication and protocol processing can be started */
    FMSTR_NotifyDriverInitDone();

    return ok;
}

/******************************************************************************
 *
 * @brief    Enable/Disable Serial transmitter
 *
 ******************************************************************************/

static void _FMSTR_SerialZephyrEnableTransmit(FMSTR_BOOL enable)
{
    FMSTR_UNUSED(enable);
}

/******************************************************************************
 *
 * @brief    Enable/Disable Serial receiver
 *
 ******************************************************************************/

static void _FMSTR_SerialZephyrEnableReceive(FMSTR_BOOL enable)
{
    FMSTR_UNUSED(enable);
}

/******************************************************************************
 *
 * @brief    Enable/Disable interrupt from transmit register empty event
 *
 ******************************************************************************/

static void _FMSTR_SerialZephyrEnableTransmitInterrupt(FMSTR_BOOL enable)
{
    if (enable != FMSTR_FALSE)
    {
        uart_irq_tx_enable(fmstr_serialDev);
    }
    else
    {
        uart_irq_tx_disable(fmstr_serialDev);
    }
}

/******************************************************************************
 *
 * @brief    Enable/Disable interrupt from transmit complete event
 *
 ******************************************************************************/

static void _FMSTR_SerialZephyrEnableTransmitCompleteInterrupt(FMSTR_BOOL enable)
{
    FMSTR_UNUSED(enable);
}

/******************************************************************************
 *
 * @brief    Enable/Disable interrupt from receive register full event
 *
 ******************************************************************************/

static void _FMSTR_SerialZephyrEnableReceiveInterrupt(FMSTR_BOOL enable)
{
    if (enable != FMSTR_FALSE)
    {
        uart_irq_rx_enable(fmstr_serialDev);
    }
    else
    {
        uart_irq_rx_disable(fmstr_serialDev);
    }
}

/******************************************************************************
 *
 * @brief    Returns TRUE if the transmit register is empty, and it's possible to put next char
 *
 ******************************************************************************/

static FMSTR_BOOL _FMSTR_SerialZephyrIsTransmitRegEmpty(void)
{
    return (FMSTR_BOOL)uart_irq_tx_ready(fmstr_serialDev);
}

/******************************************************************************
 *
 * @brief    Returns TRUE if the receive register is full, and it's possible to get received char
 *
 ******************************************************************************/

static FMSTR_BOOL _FMSTR_SerialZephyrIsReceiveRegFull(void)
{
    if (!fmstr_rxCharValid && uart_irq_rx_ready(fmstr_serialDev))
    {
        if (uart_fifo_read(fmstr_serialDev, &fmstr_rxChar, 1) > 0)
        {
            fmstr_rxCharValid = FMSTR_TRUE;
        }
    }

    return fmstr_rxCharValid;
}

/******************************************************************************
 *
 * @brief    Returns TRUE if the transmitter is still active
 *
 ******************************************************************************/

static FMSTR_BOOL _FMSTR_SerialZephyrIsTransmitterActive(void)
{
   // The method _FMSTR_SerialZephyrEnableTransmitCompleteInterrupt() is not implemented
   // so we suppose that all TX data is transmitted.
   return FMSTR_FALSE;
}

/******************************************************************************
 *
 * @brief    The function puts the char for transmit
 *
 ******************************************************************************/

static void _FMSTR_SerialZephyrPutChar(FMSTR_BCHR ch)
{
    uart_fifo_fill(fmstr_serialDev, &ch, 1);
}

/******************************************************************************
 *
 * @brief    The function gets the received char
 *
 ******************************************************************************/
static FMSTR_BCHR _FMSTR_SerialZephyrGetChar(void)
{
    fmstr_rxCharValid = FMSTR_FALSE;
    return fmstr_rxChar;
}

/******************************************************************************
 *
 * @brief    The function sends buffered data
 *
 ******************************************************************************/
static void _FMSTR_SerialZephyrFlush(void)
{
}

/******************************************************************************
 *
 * @brief    Poll function waits for interrupt (long ISR) or for
 * new data event from ISR callback function (short ISR).
 *
 ******************************************************************************/
static void _FMSTR_SerialZephyrPoll(void)
{
#if FMSTR_LONG_INTR > 0
    /* Put calling thread to sleep for 10ms.
       Cannot sleep forever, the debug transmission can follow in upper layer. */
    k_sleep(K_MSEC(10));
#endif

#if FMSTR_SHORT_INTR > 0
    /* Wait (10ms) for new data to be processed
       Cannot sleep forever, the debug transmission can follow in upper layer. */
    FMSTR_WaitEvents(FMSTR_EVENT_DATA_AVAILABLE, true, 10);
#endif
}

/******************************************************************************
 *
 * @brief Interrupt Service Routine handles the UART  interrupts for the FreeMASTER
 * driver.
 *
 ******************************************************************************/

void _FMSTR_SerialZephyrIsr(const struct device *dev, void *user_data)
{
#if FMSTR_LONG_INTR > 0 || FMSTR_SHORT_INTR > 0
    /* Start interrupts processing */
    uart_irq_update(dev);

    /* Process received or just-transmitted byte. */
    FMSTR_ProcessSerial();
#endif

#if FMSTR_SHORT_INTR > 0
    /* Notify thread processing data*/
    FMSTR_PostEvents(FMSTR_EVENT_DATA_AVAILABLE);
#endif
}

#endif /* (!(FMSTR_DISABLE)) */
