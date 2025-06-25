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
 * FreeMASTER Communication Driver - Zephyr-specific specific code
 */

#ifndef _FREEMASTER_ZEPHYR_H
#define _FREEMASTER_ZEPHYR_H

#include <string.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/autoconf.h>

/******************************************************************************
 * platform-specific default configuration
 ******************************************************************************/

/* Zephyr autoconf-derived values */
#if !(defined(CONFIG_SOC_FAMILY_NXP_MCX) && CONFIG_SOC_FAMILY_NXP_MCX) && \
    !(defined(CONFIG_SOC_FAMILY_NXP_IMXRT) && CONFIG_SOC_FAMILY_NXP_IMXRT) && \
    !(defined(CONFIG_SOC_FAMILY_NXP_RW) && CONFIG_SOC_FAMILY_NXP_RW)
#warning FreeMASTER has not been tested with Zephyr on this platform
#error FreeMASTER license only enables using it with NXP platforms
#endif

#ifdef CONFIG_LITTLE_ENDIAN
#define FMSTR_PLATFORM_BIG_ENDIAN (!(CONFIG_LITTLE_ENDIAN))
#else
#error CONFIG_LITTLE_ENDIAN is missing in autoconf.h, assuming little-endian
#define FMSTR_PLATFORM_BIG_ENDIAN 0U
#endif

#ifndef FMSTR_PLATFORM_BASE_ADDRESS
#if defined(CONFIG_CPU_CORTEX_M) && CONFIG_CPU_CORTEX_M
/* Use Cortex-M memory base address as a default for optimized access. */
#define FMSTR_PLATFORM_BASE_ADDRESS 0x20000000L
#endif
#endif

/* So far tested with 32-bit platforms only */
#define FMSTR_MEMCPY_MAX_SIZE     4U
#define FMSTR_CFG_BUS_WIDTH       1U
#define FMSTR_TSA_FLAGS           0U

#if FMSTR_USE_TSA
    /* Zephyr uses TSA user tables internally for automatic tables.. */
    #if FMSTR_TSA_USER_TABLES
    #error TSA User Tables are used internally in Zephyr.
    #else
    /* ..so user config needs to have it OFF and this forces it ON. */
    #undef FMSTR_TSA_USER_TABLES
    #define FMSTR_TSA_USER_TABLES     1U
    #endif

    /* Mark the table as 'automatic', i.e. automatically added to the
       TSA table list without adding it to FMSTR_TSA_TABLE_LIST.
       Zephyr iterable structures are used as implementation. */
    #define FMSTR_TSA_SET_AUTO_TABLE(id) \
        FMSTR_TSA_FUNC_PROTO(id);    \
        STRUCT_SECTION_ITERABLE(fmstr_tsa_table_func, fmstr_tsa_##id) = { \
            .funcptr = FMSTR_TSA_FUNC(id) \
        };
    #define FMSTR_TSA_AUTO_TABLE_BEGIN(id) \
        FMSTR_TSA_SET_AUTO_TABLE(id) \
        FMSTR_TSA_TABLE_BEGIN(id)
#else
    #define FMSTR_TSA_SET_AUTO_TABLE(id)
    #define FMSTR_TSA_AUTO_TABLE_BEGIN(id)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Types definition
 ******************************************************************************/

/* FreeMASTER types used */
typedef unsigned char *FMSTR_ADDR;  /* CPU address type (4bytes) */
typedef unsigned int FMSTR_SIZE;    /* general size type (at least 16 bits) */
typedef unsigned char FMSTR_SIZE8;  /* one-byte size value */
typedef unsigned long FMSTR_SIZE32; /* general size type (at least size of address (typicaly 32bits)) */
typedef unsigned int FMSTR_BOOL;    /* general boolean type  */

typedef unsigned char FMSTR_U8;       /* smallest memory entity */
typedef unsigned short FMSTR_U16;     /* 16bit value */
typedef unsigned long FMSTR_U32;      /* 32bit value */
typedef unsigned long long FMSTR_U64; /* 64bit value */

typedef signed char FMSTR_S8;       /* signed 8bit value */
typedef signed short FMSTR_S16;     /* signed 16bit value */
typedef signed long FMSTR_S32;      /* signed 32bit value */
typedef signed long long FMSTR_S64; /* signed 64bit value */

typedef float FMSTR_FLOAT;   /* float value */
typedef double FMSTR_DOUBLE; /* double value */

typedef unsigned char FMSTR_FLAGS; /* type to be union-ed with flags (at least 8 bits) */
typedef signed int FMSTR_INDEX;    /* general for-loop index (must be signed) */

typedef unsigned char FMSTR_BCHR;  /* type of a single character in comm.buffer */
typedef unsigned char *FMSTR_BPTR; /* pointer within a communication buffer */

typedef char FMSTR_CHAR; /* regular character, part of string */

typedef FMSTR_ADDR (tsa_table_func_t)(FMSTR_SIZE *tableSize);

struct fmstr_tsa_table_func{
    tsa_table_func_t* funcptr;
};

/*********************************************************************************
 * Platform dependent functions
 *********************************************************************************/

#if 0 /* Uncomment to take standard C functions taken from stdlib */
#define FMSTR_StrLen(str)              ((FMSTR_SIZE)strlen(str))
#define FMSTR_StrCmp(str1, str2)       ((FMSTR_INDEX)strcmp(str1, str2))
#define FMSTR_MemCmp(b1, b2, size)     ((FMSTR_INDEX)memcmp(b1, b2, size))
#define FMSTR_MemSet(dest, mask, size) memset(dest, mask, size)
#endif

/* Rand is taken from stdlib */
#define FMSTR_Rand() rand()

/*********************************************************************************
 * Zephyr-specific functions
 *********************************************************************************/

/* FreeMASTER driver synchronization events */
#define FMSTR_EVENT_DRV_INIT_DONE     0x01
#define FMSTR_EVENT_DATA_AVAILABLE    0x02
#define FMSTR_EVENT_THREAD_INIT_DONE  0x04
#define FMSTR_WAIT_FOREVER        -1


/* FreeMASTER driver initialization and communication handling */
void FMSTR_Thread();

/* Notify FreeMASTER driver initialization done */
void FMSTR_NotifyDriverInitDone();
/* Test if driver is already initialized */
bool FMSTR_IsDriverInitialized();
/* Wait for thread to finish FreeMASTER initialization */
bool FMSTR_WaitInitialized(uint32_t timeout);

/* Low-level event handling */
void FMSTR_PostEvents(uint32_t events);
uint32_t FMSTR_WaitEvents(uint32_t events, bool clear, uint32_t timeout);
bool FMSTR_TestEvents(uint32_t events);

#ifdef __cplusplus
}
#endif

#endif /* _FREEMASTER_ZEPHYR_H */
