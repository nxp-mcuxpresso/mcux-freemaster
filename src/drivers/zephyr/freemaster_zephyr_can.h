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
 * FreeMASTER Communication Driver - CAN low-level driver
 */

#ifndef __FREEMASTER_ZEPHYR_CAN_H
#define __FREEMASTER_ZEPHYR_CAN_H

/******************************************************************************
 * Required header files include check
 ******************************************************************************/
#ifndef __FREEMASTER_H
#error Please include the freemaster.h master header file before the freemaster_zephyr_can.h
#endif

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Global API functions
 ******************************************************************************/

const struct device* FMSTR_CanGetDevice();
void FMSTR_CanSetDevice(const struct device * dev);

#ifdef __cplusplus
}
#endif

#endif /* __FREEMASTER_ZEPHYR_CAN_H */