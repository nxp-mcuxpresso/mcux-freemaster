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

#include <zephyr/autoconf.h>
#include <zephyr/kernel.h>
#include <zephyr/version.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel/thread.h>

#include "freemaster.h"
#include "freemaster_tsa.h"
#include "freemaster_zephyr.h"
#include "freemaster_shell.h"

#if FMSTR_DISABLE == 0

/* The NXP license only allows use of FreeMASTER code with NXP MCUs */
#if !( (defined(CONFIG_SOC_FAMILY_NXP_MCX) && CONFIG_SOC_FAMILY_NXP_MCX) || \
       (defined(CONFIG_SOC_FAMILY_NXP_IMXRT) && CONFIG_SOC_FAMILY_NXP_IMXRT) || \
       (defined(CONFIG_SOC_FAMILY_NXP_RW) && CONFIG_SOC_FAMILY_NXP_RW)  || \
       (defined(CONFIG_SOC_FAMILY_NXP_S32) && CONFIG_SOC_FAMILY_NXP_S32) || \
       (defined(CONFIG_SOC_FAMILY_LPC) && CONFIG_SOC_FAMILY_LPC) \
       )
#warning FreeMASTER has not been tested with Zephyr on this platform
#error FreeMASTER license only enables using it with NXP platforms
#endif

LOG_MODULE_REGISTER(freemaster);

#if !defined(CONFIG_EVENTS) || !CONFIG_EVENTS
#error FreeMASTER Zephyr driver requires CONFIG_EVENTS feature enabled in Kconfig.
#endif

typedef struct
{
    char build_version[sizeof(STRINGIFY(BUILD_VERSION))];
} fmstr_zephyr_info_t;

typedef enum
{
    DATA_IDLE,
    DATA_REQUEST,
    DATA_READY,
    DATA_ERROR
} thread_info_state_t;

typedef struct
{
    thread_info_state_t state;
    struct k_thread* pthread;
    struct k_thread* pnext_thread;
    uint8_t prio;
    uint32_t stack_start;
    uint32_t stack_size;
    uint32_t stack_unused;
    uint64_t sched_usage;
    char name[CONFIG_THREAD_MAX_NAME_LEN];
} fmstr_thread_info_t;

typedef struct
{
    thread_info_state_t state;
    struct k_thread* pthread;
} fmstr_thread_req_t;

/* Structures used by FreeMASTER */
static fmstr_thread_req_t fmstr_thread_req;
static fmstr_thread_info_t fmstr_thread_info;
static fmstr_zephyr_info_t fmstr_zephyr_info;

static K_EVENT_DEFINE(fmstr_sync_event);

/* Post events */
void FMSTR_PostEvents(uint32_t events)
{
    k_event_post(&fmstr_sync_event, events);
}

/* Wait events */
uint32_t FMSTR_WaitEvents(uint32_t events, bool clear, uint32_t timeout)
{
    /* Always use false here not to clear events that are already set ... */
    uint32_t ret = k_event_wait(&fmstr_sync_event, events, false, timeout == FMSTR_WAIT_FOREVER ? K_FOREVER : K_MSEC(timeout));

    /* ... if user want it we clear afterwards */
    if (clear)
        k_event_clear(&fmstr_sync_event, events);

    return ret;
}

/* Test events */
bool FMSTR_TestEvents(uint32_t events)
{
    return k_event_test(&fmstr_sync_event, events) == events;
}

/* Notify FreeMASTER driver initialization done. Called by driver code when driver is ready. */
void FMSTR_NotifyDriverInitDone()
{
    FMSTR_PostEvents(FMSTR_EVENT_DRV_INIT_DONE);
}

/* Check if driver is already initialized. Call to make sure the driver is initialized. */
bool FMSTR_IsDriverInitialized()
{
    return FMSTR_TestEvents(FMSTR_EVENT_DRV_INIT_DONE);
}

/* Wait for thread to finish FreeMASTER initialization. */
bool FMSTR_WaitInitialized(uint32_t timeout)
{
    return FMSTR_WaitEvents(FMSTR_EVENT_THREAD_INIT_DONE, false, timeout);
}

/* Fill Zephyr info structure */
static void FMSTR_FillZephyrInfo()
{
    memcpy(&fmstr_zephyr_info.build_version, STRINGIFY(BUILD_VERSION), sizeof(STRINGIFY(BUILD_VERSION)));
}

/* Fill thread info structure for the thread */
static void FMSTR_FillThreadInfo()
{
    if (fmstr_thread_req.state == DATA_REQUEST)
    {
        struct k_thread* pthread = NULL;

        /* Clear info */
        memset(&fmstr_thread_info, 0, sizeof(fmstr_thread_info));

        /* If thread is not specified use the first */
        if (fmstr_thread_req.pthread == NULL)
        {
            fmstr_thread_info.pthread = _kernel.threads;
        }
        else
        {
            fmstr_thread_info.pthread = fmstr_thread_req.pthread;
        }

        /* Find the thread */
        #ifdef CONFIG_THREAD_MONITOR
        pthread = _kernel.threads;
        while (pthread && pthread != fmstr_thread_info.pthread)
        {
            pthread = pthread->next_thread;
        }
        #endif

        /* Thread found, fill values */
        if (pthread && pthread == fmstr_thread_info.pthread)
        {
            fmstr_thread_info.pnext_thread = pthread->next_thread;
            fmstr_thread_info.prio = pthread->base.prio;

            #ifdef CONFIG_THREAD_STACK_INFO
            fmstr_thread_info.stack_start = pthread->stack_info.start;
            fmstr_thread_info.stack_size = pthread->stack_info.size;

            #ifdef CONFIG_INIT_STACKS
            k_thread_stack_space_get(pthread, &fmstr_thread_info.stack_unused);
            #endif
            #endif

            #ifdef CONFIG_SCHED_THREAD_USAGE
            fmstr_thread_info.sched_usage = pthread->base.usage.total;
            #endif

            #ifdef CONFIG_THREAD_NAME
            _FMSTR_MemCpy(&fmstr_thread_info.name, &pthread->name, CONFIG_THREAD_MAX_NAME_LEN);
            #endif

            /* Set data ready */
            fmstr_thread_info.state = DATA_READY;
        }
        else
        {
            /* Thread not found, set error code */
            fmstr_thread_info.state = DATA_ERROR;
        }

        /* Copy result state also to request */
        fmstr_thread_req.state = fmstr_thread_info.state;
    }
}

/* FreeMASTER thread function */

void FMSTR_Thread()
{
    /* Fill Zephyr info structure */
    FMSTR_FillZephyrInfo();

    /* Note: before this thread starts execution, a user code may get/set or change configuration
       of the communication device used by FreeMASTER. The main thread shall wait for this thread
       to finish this initialization (FMSTR_WaitInitialized) before it calls any other FreeMASTER API. */

    /* FreeMASTER driver initialization */
    if(!FMSTR_Init())
    {
        LOG_ERR("FreeMASTER initialization failed. Terminating FreeMASTER thread.");
        return ;
    }

#if defined(CONFIG_FMSTR_USE_PIPE_SHELL) && CONFIG_FMSTR_SHELL_PIPE_PORT > 0
    FMSTR_PipeEnableShell(CONFIG_FMSTR_SHELL_PIPE_PORT, CONFIG_FMSTR_SHELL_INIT_LOG_LEVEL);
#endif

    /* Wakeup all who do FMSTR_WaitInitialized() */
    FMSTR_PostEvents(FMSTR_EVENT_THREAD_INIT_DONE);

    while (1)
    {
        /* Fill thread info structure if requested */
        FMSTR_FillThreadInfo();

        /* The FreeMASTER poll handles the communication interface and protocol
        processing. The poll call will put this thread to sleep until some communication takes
        place and triggers the protocol processing depending on interrupt mode configured:
            - In Short Interrupt mode the Poll processes the protocol message received by ISR.
            - In Long Interrupt mode both communication and protocol decoding is fully handled by
              ISR, so this loop will only process for regular housekeeping operations like debug printing.
        */
        FMSTR_Poll();
    }
}

#if defined(CONFIG_FMSTR_USE_THREAD) && CONFIG_FMSTR_USE_THREAD != 0

/* FreeMASTER thread */
K_THREAD_DEFINE(freemaster_work,
                CONFIG_FMSTR_THREAD_STACK_SIZE,
                FMSTR_Thread,
                NULL, NULL, NULL,
                CONFIG_FMSTR_THREAD_PRIORITY,
                0, 0);

#endif /* CONFIG_FMSTR_USE_THREAD */

#if FMSTR_USE_TSA
#include <zephyr/sys/iterable_sections.h>

/* TSA User tables are used in Zephyr to implement 'automatic' FMSTR_TSA_AUTO_TABLE tables.
   Such tables are automatically added to TSA table list without need to specify them in FMSTR_TSA_TABLE_LIST */
FMSTR_ADDR FMSTR_TsaGetUserTable(FMSTR_INDEX tableIndex, FMSTR_SIZE *tableSize)
{
    FMSTR_ADDR addr = NULL;

    if(tableIndex >= 0)
    {
        struct fmstr_tsa_table_func *p = NULL;
        uint32_t count = 0U;
        uint32_t index = (uint32_t)tableIndex;

        STRUCT_SECTION_COUNT(fmstr_tsa_table_func, &count);
        if (count > 0 && index < count)
        {
            /* Automatic TSA tables are implemented as iterable structures.  */
            STRUCT_SECTION_GET(fmstr_tsa_table_func, index, &p);
            FMSTR_ASSERT(p != NULL && p->funcptr != NULL);

            /* The structure contains the TSA table-get function. */
            addr = p->funcptr(tableSize);
        }
    }

    return addr;
}

/* Zephyr structures used by FreeMASTER UI to fetch thread information */

FMSTR_TSA_AUTO_TABLE_BEGIN(fmstr_zephyr_table)
    FMSTR_TSA_STRUCT(fmstr_zephyr_info_t)
    FMSTR_TSA_MEMBER(fmstr_zephyr_info_t, build_version, FMSTR_TSA_UINT8)

    FMSTR_TSA_STRUCT(fmstr_thread_req_t)
    FMSTR_TSA_MEMBER(fmstr_thread_req_t, state, FMSTR_TSA_UINT8)
    FMSTR_TSA_MEMBER(fmstr_thread_req_t, pthread, FMSTR_TSA_POINTER)

    FMSTR_TSA_STRUCT(fmstr_thread_info_t)
    FMSTR_TSA_MEMBER(fmstr_thread_info_t, state, FMSTR_TSA_UINT8)
    FMSTR_TSA_MEMBER(fmstr_thread_info_t, pthread, FMSTR_TSA_POINTER)
    FMSTR_TSA_MEMBER(fmstr_thread_info_t, pnext_thread, FMSTR_TSA_POINTER)
    FMSTR_TSA_MEMBER(fmstr_thread_info_t, prio, FMSTR_TSA_UINT8)
    FMSTR_TSA_MEMBER(fmstr_thread_info_t, stack_start, FMSTR_TSA_POINTER)
    FMSTR_TSA_MEMBER(fmstr_thread_info_t, stack_size, FMSTR_TSA_UINT32)
    FMSTR_TSA_MEMBER(fmstr_thread_info_t, stack_unused, FMSTR_TSA_UINT32)
    FMSTR_TSA_MEMBER(fmstr_thread_info_t, sched_usage, FMSTR_TSA_UINT64)
    FMSTR_TSA_MEMBER(fmstr_thread_info_t, name, FMSTR_TSA_UINT8)

    FMSTR_TSA_RO_VAR(fmstr_zephyr_info, FMSTR_TSA_USERTYPE(fmstr_zephyr_info_t))
    FMSTR_TSA_RW_VAR(fmstr_thread_req, FMSTR_TSA_USERTYPE(fmstr_zephyr_req_t))
    FMSTR_TSA_RO_VAR(fmstr_thread_info, FMSTR_TSA_USERTYPE(fmstr_thread_info_t))
FMSTR_TSA_TABLE_END()

/* A weak empty table list to make all AUTO tables be registered 
   even if user does not define the official table list. */
__weak FMSTR_TSA_TABLE_LIST_BEGIN()
FMSTR_TSA_TABLE_LIST_END()

#endif /* FMSTR_USE_TSA */

#else /* FMSTR_DISABLE */

/* Empty API for case when FreeMASTER is disabled. */

bool FMSTR_WaitInitialized(uint32_t timeout)
{
    return true;
}

#endif /* FMSTR_DISABLE */
