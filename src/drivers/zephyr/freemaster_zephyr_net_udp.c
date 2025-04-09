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
 * FreeMASTER Communication Driver - Network UDP driver
 */

#include <zephyr/net/socket.h>

#include "freemaster.h"
#include "freemaster_net.h"
#include "freemaster_private.h"
#include "freemaster_protocol.h"

#if FMSTR_DISABLE == 0

/******************************************************************************
 * Adapter configuration
 ******************************************************************************/
#if (defined(FMSTR_SHORT_INTR) && FMSTR_SHORT_INTR) || (defined(FMSTR_LONG_INTR) && FMSTR_LONG_INTR)
#error The FreeMASTER network UDP driver does not support interrupt mode.
#endif

/******************************************************************************
 * Local functions
 ******************************************************************************/
static FMSTR_BOOL _FMSTR_NetZephyrUdpInit(void);
static void _FMSTR_NetZephyrUdpPoll(void);
static FMSTR_S32 _FMSTR_NetZephyrUdpRecv(FMSTR_BPTR msgBuff,
                                         FMSTR_SIZE msgMaxSize,
                                         FMSTR_NET_ADDR *recvAddr,
                                         FMSTR_BOOL *isBroadcast);
static FMSTR_S32 _FMSTR_NetZephyrUdpSend(FMSTR_NET_ADDR *sendAddr, FMSTR_BPTR msgBuff, FMSTR_SIZE msgSize);
static void _FMSTR_NetZephyrUdpClose(FMSTR_NET_ADDR *addr);
static void _FMSTR_NetZephyrUdpGetCaps(FMSTR_NET_IF_CAPS *caps);

static void _FMSTR_NetAddrToFmstr(FMSTR_NET_ADDR *fmstrAddr, struct sockaddr *addr, socklen_t len);
static void _FMSTR_NetAddrFromFmstr(struct sockaddr *addr, socklen_t len, FMSTR_NET_ADDR *fmstrAddr);

/******************************************************************************
 * Local variables
 ******************************************************************************/

static int fmstrSrvSock = -1;

/******************************************************************************
 * Driver interface
 ******************************************************************************/
/* Interface of this network UDP driver */
const FMSTR_NET_DRV_INTF FMSTR_ZEPHYR_UDP_DRV = {
    .Init    = _FMSTR_NetZephyrUdpInit,
    .Poll    = _FMSTR_NetZephyrUdpPoll,
    .Recv    = _FMSTR_NetZephyrUdpRecv,
    .Send    = _FMSTR_NetZephyrUdpSend,
    .Close   = _FMSTR_NetZephyrUdpClose,
    .GetCaps = _FMSTR_NetZephyrUdpGetCaps,
};

/******************************************************************************
 * Implementation
 ******************************************************************************/

static FMSTR_BOOL _FMSTR_NetZephyrUdpInit(void)
{
    FMSTR_BOOL succes = FMSTR_TRUE;
    struct sockaddr_in addr;

   /* Do not allow second initialization of the driver */
    if (FMSTR_IsDriverInitialized())
        return FMSTR_FALSE;

    FMSTR_MemSet(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; // TODO: prepare address for IPv4 or IPv6 ?
    addr.sin_port = htons(FMSTR_NET_PORT);

    /* Create UDP server socket */
    fmstrSrvSock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // TODO: IPv6?
    if (fmstrSrvSock < 0)
    {
        succes = FMSTR_FALSE;
    }

    /* Bind server socket to local network address */
    if (succes && (zsock_bind(fmstrSrvSock, (struct sockaddr *)&addr, sizeof(addr)) < 0))
    {
        succes = FMSTR_FALSE;
    }

    /* Notify finished driver initialization,
       communication and protocol processing can be started */
    FMSTR_NotifyDriverInitDone();

    return succes;
}

static void _FMSTR_NetZephyrUdpPoll(void)
{
}

static FMSTR_S32 _FMSTR_NetZephyrUdpRecv(FMSTR_BPTR msgBuff,
                                       FMSTR_SIZE msgMaxSize,
                                       FMSTR_NET_ADDR *recvAddr,
                                       FMSTR_BOOL *isBroadcast)
{
    int recvSize = 0;
    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);

    struct zsock_pollfd fds[] = {
        {
            .fd = fmstrSrvSock,
            .events = ZSOCK_POLLIN,
        },
    };

    FMSTR_ASSERT(msgBuff != NULL);
    FMSTR_ASSERT(recvAddr != NULL);
    FMSTR_ASSERT(isBroadcast != NULL);

    *isBroadcast = FMSTR_FALSE;

    if (fmstrSrvSock < 0)
    {
        return -1;
    }

    if (zsock_poll(fds, ARRAY_SIZE(fds), FMSTR_NET_BLOCKING_TIMEOUT) > 0)
    {
        if (fds[0].revents & ZSOCK_POLLIN)
        {
            recvSize = zsock_recvfrom(fmstrSrvSock, msgBuff, msgMaxSize, 0, &addr, &addrlen);
            if (recvSize > 0)
            {
                /* Copy address of receiver */
                _FMSTR_NetAddrToFmstr(recvAddr, &addr, addrlen);
            }
        }
    }

    return recvSize;
}

static FMSTR_S32 _FMSTR_NetZephyrUdpSend(FMSTR_NET_ADDR *sendAddr, FMSTR_BPTR msgBuff, FMSTR_SIZE msgSize)
{
    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);

    FMSTR_ASSERT(msgBuff != NULL);
    FMSTR_ASSERT(sendAddr != NULL);

    if (fmstrSrvSock < 0)
    {
        return -1;
    }

    /* Copy destination address */
    _FMSTR_NetAddrFromFmstr(&addr, addrlen, sendAddr);

    return zsock_sendto(fmstrSrvSock, msgBuff, msgSize, 0, &addr, addrlen);
}

static void _FMSTR_NetZephyrUdpClose(FMSTR_NET_ADDR *addr)
{
}

static void _FMSTR_NetZephyrUdpGetCaps(FMSTR_NET_IF_CAPS *caps)
{
    FMSTR_ASSERT(caps != NULL);

    caps->flags |= FMSTR_NET_IF_CAPS_FLAG_UDP;
}

static void _FMSTR_NetAddrToFmstr(FMSTR_NET_ADDR *fmstrAddr, struct sockaddr *addr, socklen_t len)
{
    struct sockaddr_in* address = (struct sockaddr_in*)addr;

    FMSTR_ASSERT(addr != NULL);
    FMSTR_ASSERT(fmstrAddr != NULL);

    /* IP address type */
    fmstrAddr->type = FMSTR_NET_ADDR_TYPE_V4;

    /* Port */
    fmstrAddr->port = address->sin_port;

    /* IP address */
    if (fmstrAddr->type == FMSTR_NET_ADDR_TYPE_V4)
    {
        FMSTR_MemCpy(fmstrAddr->addr.v4, &address->sin_addr.s4_addr, 4);
    }

    // TODO: add support for IPv6 address
}

static void _FMSTR_NetAddrFromFmstr(struct sockaddr *addr, socklen_t len, FMSTR_NET_ADDR *fmstrAddr)
{
    struct sockaddr_in* address = (struct sockaddr_in*)addr;

    FMSTR_ASSERT(fmstrAddr != NULL);
    FMSTR_ASSERT(addr != NULL);

    /* Port */
    address->sin_port = fmstrAddr->port;

    if (fmstrAddr->type == FMSTR_NET_ADDR_TYPE_V4)
    {
        /* IP address */
        FMSTR_MemCpy(&address->sin_addr.s4_addr, fmstrAddr->addr.v4, 4);
    }

    // TODO: add support for IPv6 address
}

#endif /* (!(FMSTR_DISABLE)) */