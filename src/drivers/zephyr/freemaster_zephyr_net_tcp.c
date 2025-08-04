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
 * FreeMASTER Communication Driver - Network TCP driver
 */


#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/fcntl.h>

#include "freemaster.h"
#include "freemaster_net.h"
#include "freemaster_private.h"
#include "freemaster_protocol.h"

#if FMSTR_DISABLE == 0

/******************************************************************************
 * Adapter configuration
 ******************************************************************************/
#if (defined(FMSTR_SHORT_INTR) && FMSTR_SHORT_INTR) || (defined(FMSTR_LONG_INTR) && FMSTR_LONG_INTR)
#error The FreeMASTER network TCP driver does not support interrupt mode.
#endif

#if FMSTR_NET_AUTODISCOVERY != 0
/* Count of all sessions (TCP sessions + one UDP broadcast session) */
#define FMSTR_TCP_SESSION_COUNT (FMSTR_SESSION_COUNT + 1)
#else
#define FMSTR_TCP_SESSION_COUNT FMSTR_SESSION_COUNT
#endif

/* We need one for each session plus one listening */
#if (FMSTR_TCP_SESSION_COUNT+1) > CONFIG_ZVFS_OPEN_MAX
#error FreeMASTER requires more sockets than configured in system. Update CONFIG_ZVFS_OPEN_MAX to match.
#endif

/* We need one for each session plus one listening */
#if (FMSTR_TCP_SESSION_COUNT+1) > CONFIG_ZVFS_POLL_MAX
#error FreeMASTER requires more sessions than zsock_poll capacity. Update CONFIG_ZVFS_POLL_MAX to match.
#endif

/* Needed network connections */
#if (FMSTR_TCP_SESSION_COUNT+2) > CONFIG_NET_MAX_CONN
#warning FreeMASTER requires more network connections than configured in system. Update CONFIG_NET_MAX_CONN to match.
#endif

/* Needed network contexts to allocate */
#if (FMSTR_TCP_SESSION_COUNT+2) > CONFIG_NET_MAX_CONTEXTS
#warning FreeMASTER requires more network contexts to allocate. Update CONFIG_NET_MAX_CONTEXTS to match.
#endif

/******************************************************************************
 * Local types
 ******************************************************************************/

typedef struct FMSTR_TCP_SESSION_S
{
    int sock;
    FMSTR_BOOL receivePending;
    FMSTR_NET_ADDR address;
} FMSTR_TCP_SESSION;

/******************************************************************************
 * Local functions
 ******************************************************************************/
static FMSTR_BOOL _FMSTR_NetZephyrTcpInit(void);
static void _FMSTR_NetZephyrTcpPoll(void);
static FMSTR_S32 _FMSTR_NetZephyrTcpRecv(FMSTR_BPTR msgBuff,
                                         FMSTR_SIZE msgMaxSize,
                                         FMSTR_NET_ADDR *recvAddr,
                                         FMSTR_BOOL *isBroadcast);
static FMSTR_S32 _FMSTR_NetZephyrTcpSend(FMSTR_NET_ADDR *sendAddr, FMSTR_BPTR msgBuff, FMSTR_SIZE msgSize);
static void _FMSTR_NetZephyrTcpClose(FMSTR_NET_ADDR *addr);
static void _FMSTR_NetZephyrTcpGetCaps(FMSTR_NET_IF_CAPS *caps);
static void _FMSTR_NetAddrToFmstr(struct sockaddr *remoteAddr, FMSTR_NET_ADDR *fmstrAddr);

/******************************************************************************
 * Local variables
 ******************************************************************************/

/* TCP sessions */
static FMSTR_TCP_SESSION fmstrTcpSessions[FMSTR_TCP_SESSION_COUNT];
/* TCP listen socket */
static int fmstrTcpListenSock = 0;

#if FMSTR_NET_AUTODISCOVERY != 0
/* UDP Broadcast socket */
static int fmstrUdpBroadcastSock = 0;
#endif /* FMSTR_NET_AUTODISCOVERY */

/******************************************************************************
 * Driver interface
 ******************************************************************************/
/* Interface of this network UDP driver */
const FMSTR_NET_DRV_INTF FMSTR_ZEPHYR_TCP_DRV = {
    .Init    = _FMSTR_NetZephyrTcpInit,
    .Poll    = _FMSTR_NetZephyrTcpPoll,
    .Recv    = _FMSTR_NetZephyrTcpRecv,
    .Send    = _FMSTR_NetZephyrTcpSend,
    .Close   = _FMSTR_NetZephyrTcpClose,
    .GetCaps = _FMSTR_NetZephyrTcpGetCaps,
};

/******************************************************************************
 * Implementation
 ******************************************************************************/

static FMSTR_BOOL _FMSTR_NetZephyrTcpInit(void)
{
    FMSTR_INDEX i;
    FMSTR_BOOL succes = FMSTR_TRUE;
    struct sockaddr_in addr;

    /* Do not allow second initialization of the driver */
    if (FMSTR_IsDriverInitialized())
        return FMSTR_FALSE;

    FMSTR_MemSet(&fmstrTcpSessions, 0, sizeof(fmstrTcpSessions));
    FMSTR_MemSet(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; // TODO: prepare address for IPv4 or IPv6 ?
    addr.sin_port = htons(FMSTR_NET_PORT);

    /* Prepare sockets */
    for (i = 0; i < FMSTR_TCP_SESSION_COUNT; i++)
    {
        fmstrTcpSessions[i].sock = -1;
    }

    /* Create new listen socket */
    fmstrTcpListenSock = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // TODO: IPv6?
    if (fmstrTcpListenSock >= 0)
    {
        /* Bind & listen */
        if ((zsock_bind(fmstrTcpListenSock, (struct sockaddr *)&addr, sizeof(addr)) >= 0) &&
            (zsock_listen(fmstrTcpListenSock, 0) >= 0))
        {
            /* successfully listening.. */
        }
        else
        {
            succes = FMSTR_FALSE;

            zsock_close(fmstrTcpListenSock);
            fmstrTcpListenSock = -1;
        }
    }
    else
    {
        succes = FMSTR_FALSE;
    }

#if FMSTR_NET_AUTODISCOVERY != 0

    /* Create new UDP listen socket */
    fmstrUdpBroadcastSock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fmstrUdpBroadcastSock < 0)
    {
        succes = FMSTR_FALSE;
    }

    /* Socket bind */
    if (succes && (zsock_bind(fmstrUdpBroadcastSock, (struct sockaddr *)&addr, sizeof(addr)) < 0))
    {
        succes = FMSTR_FALSE;
    }

    /* Set broadcast UDP session as first session */
    if (succes)
    {
        fmstrTcpSessions[0].sock = fmstrUdpBroadcastSock;
    }

#endif /* FMSTR_NET_AUTODISCOVERY */

    /* Notify finished driver initialization,
       communication and protocol processing can be started */
    FMSTR_NotifyDriverInitDone();

    return succes;
}

static FMSTR_TCP_SESSION *_FMSTR_NetZephyrTcpSessionPending(void)
{
    FMSTR_INDEX i;

    for (i = 0; i < FMSTR_TCP_SESSION_COUNT; i++)
    {
        /* Find pending session */
        if (fmstrTcpSessions[i].sock >= 0 && fmstrTcpSessions[i].receivePending != FMSTR_FALSE)
        {
            return &fmstrTcpSessions[i];
        }
    }

    return NULL;
}

static FMSTR_TCP_SESSION *_FMSTR_NetZephyrTcpSessionFindByAddr(FMSTR_NET_ADDR *sendAddr)
{
    FMSTR_INDEX i;

    for (i = 0; i < FMSTR_TCP_SESSION_COUNT; i++)
    {
        /* Find free session */
        if (sendAddr == NULL)
        {
            if (fmstrTcpSessions[i].sock < 0)
            {
                return &fmstrTcpSessions[i];
            }
        }
        /* Find session by address */
        else
        {
            if (FMSTR_MemCmp(&fmstrTcpSessions[i].address, sendAddr, sizeof(FMSTR_NET_ADDR)) == 0)
            {
                return &fmstrTcpSessions[i];
            }
        }
    }

    return NULL;
}

static FMSTR_TCP_SESSION *_FMSTR_NetZephyrTcpSessionFindBySocket(int sock)
{
    FMSTR_INDEX i;

    for (i = 0; i < FMSTR_TCP_SESSION_COUNT; i++)
    {
        if (fmstrTcpSessions[i].sock == sock)
        {
            return &fmstrTcpSessions[i];
        }
    }

    return NULL;
}

static void _FMSTR_NetZephyrTcpAccept(void)
{
    struct sockaddr_storage addrStorage;
    struct sockaddr* addr = (struct sockaddr*)&addrStorage;
    socklen_t length = sizeof(addrStorage);
    int newSock = 0;

    FMSTR_MemSet(&addrStorage, 0, sizeof(addrStorage));

    /* Accept socket */
    newSock = zsock_accept(fmstrTcpListenSock, addr, &length);
    if (newSock >= 0)
    {
        FMSTR_TCP_SESSION* newSes = _FMSTR_NetZephyrTcpSessionFindByAddr(NULL);
        if (newSes != NULL)
        {
            newSes->sock = newSock;
            _FMSTR_NetAddrToFmstr(addr, &newSes->address);

#if FMSTR_DEBUG_LEVEL >= 2
            FMSTR_DEBUG_PRINTF("TCP socket %d accepted connection from port %d\n", newSock, newSes->address.port);
#endif
        }
        else
        {
            /* Actively deny new connection if no free session.
            This shall never happen as the listen sock is not polled
            when all sessions are in use. */
            zsock_close(newSock);

#if FMSTR_DEBUG_LEVEL >= 2
            FMSTR_DEBUG_PRINTF("TCP socket %d denied connection\n", newSock);
#endif
        }
    }
    else
    {
#if FMSTR_DEBUG_LEVEL >= 2
        FMSTR_DEBUG_PRINTF("TCP socket accept failed\n");
#endif
    }
}

static void _FMSTR_NetZephyrTcpPoll(void)
{
    /* Array for polling all sessions + one listening socket */
    static struct zsock_pollfd pollFds[FMSTR_TCP_SESSION_COUNT+1];

    FMSTR_INDEX i, pollCount;
    FMSTR_TCP_SESSION* session;

    if (fmstrTcpListenSock < 0)
    {
        return;
    }

#if FMSTR_NET_AUTODISCOVERY != 0
    if (fmstrUdpBroadcastSock < 0)
    {
        return;
    }
#endif

    /* Any session is still pending to read */
    if (_FMSTR_NetZephyrTcpSessionPending() != NULL)
    {
        return;
    }

    /* Prepare poll array to detect sessions ready to read */
    pollCount = 0;
    for (i = 0; i < FMSTR_TCP_SESSION_COUNT; i++ )
    {
        session = &fmstrTcpSessions[i];
        if (session->sock >= 0 && session->receivePending == FMSTR_FALSE)
        {
            /* Poll events that affect recv operation */
            pollFds[pollCount].fd = session->sock;
            pollFds[pollCount].events = ZSOCK_POLLIN | ZSOCK_POLLPRI | ZSOCK_POLLHUP | ZSOCK_POLLNVAL;
            pollFds[pollCount].revents = 0;
            pollCount++;
        }
    }

    /* If there is a free session available */
    if (_FMSTR_NetZephyrTcpSessionFindByAddr(NULL) != NULL)
    {
        /* ..also add listening socket to accept new connection */
        pollFds[pollCount].fd = fmstrTcpListenSock;
        pollFds[pollCount].events = ZSOCK_POLLIN;
        pollFds[pollCount].revents = 0;
        pollCount++;
    }

    /* Poll active sessions for incoming data with specified timeout */
    if (zsock_poll(pollFds, (int)pollCount, FMSTR_NET_BLOCKING_TIMEOUT) > 0)
    {
        for (i = 0; i < pollCount; i++)
        {
            /* Any of requested events occurred? */
            if (pollFds[i].revents != 0)
            {
                /* Read event on listening socket */
                if (pollFds[i].fd == fmstrTcpListenSock)
                {
                    /* Accept new connection */
                    _FMSTR_NetZephyrTcpAccept();
                }
                else
                {
                    /* Read or Exception event on established connection (or on a UDP broadcast session) */
                    session = _FMSTR_NetZephyrTcpSessionFindBySocket(pollFds[i].fd);
                    if (session != NULL)
                    {
                        session->receivePending = FMSTR_TRUE;
                    }
                }
            }
        }
    }
}

static FMSTR_S32 _FMSTR_NetZephyrTcpRecv(FMSTR_BPTR msgBuff,
                                       FMSTR_SIZE msgMaxSize,
                                       FMSTR_NET_ADDR *recvAddr,
                                       FMSTR_BOOL *isBroadcast)
{
    int res                = 0;
    FMSTR_TCP_SESSION *ses = NULL;

    FMSTR_ASSERT(msgBuff != NULL);
    FMSTR_ASSERT(recvAddr != NULL);
    FMSTR_ASSERT(isBroadcast != NULL);

    *isBroadcast = FMSTR_FALSE;

    if (fmstrTcpListenSock < 0)
    {
        return 0;
    }

    /* Any receive pending? */
    ses = _FMSTR_NetZephyrTcpSessionPending();
    if (ses == NULL)
    {
        return 0;
    }

#if FMSTR_NET_AUTODISCOVERY != 0
    /* Receive UDP broadcast */
    if (ses->sock == fmstrUdpBroadcastSock)
    {
        struct sockaddr remote_addr;
        socklen_t length = sizeof(remote_addr);

        *isBroadcast = FMSTR_TRUE;

        res = zsock_recvfrom(ses->sock, msgBuff, msgMaxSize, 0, &remote_addr, &length);

        /* Copy address */
        _FMSTR_NetAddrToFmstr(&remote_addr, &ses->address);
        /* Copy address */
        FMSTR_MemCpy(recvAddr, &ses->address, sizeof(FMSTR_NET_ADDR));
    }
    else
#endif /* FMSTR_NET_AUTODISCOVERY */
    {
        res = zsock_recv(ses->sock, msgBuff, msgMaxSize, 0);

        /* Copy address */
        FMSTR_MemCpy(recvAddr, &ses->address, sizeof(FMSTR_NET_ADDR));

        /* Zero in blocking socket mode means socket was closed by peer */
        if (res == 0)
        {
            /* Upper layer will free the session */
            res = -1;
        }
    }

    return res;
}

static FMSTR_S32 _FMSTR_NetZephyrTcpSend(FMSTR_NET_ADDR *sendAddr, FMSTR_BPTR msgBuff, FMSTR_SIZE msgSize)
{
    FMSTR_TCP_SESSION *ses = NULL;
    int res                = 0;

    FMSTR_ASSERT(msgBuff != NULL);
    FMSTR_ASSERT(sendAddr != NULL);

    /* Find session by address */
    ses = _FMSTR_NetZephyrTcpSessionFindByAddr(sendAddr);
    if (ses == NULL)
    {
        /* Same as socket error */
        return -1;
    }

    /* This session is not pending now */
    ses->receivePending = FMSTR_FALSE;

#if FMSTR_NET_AUTODISCOVERY != 0
    /* Receive UDP broadcast */
    if (ses->sock == fmstrUdpBroadcastSock)
    {
        struct sockaddr_in destAddr4;
        FMSTR_MemSet(&destAddr4, 0, sizeof(destAddr4));
        destAddr4.sin_family = AF_INET;
        destAddr4.sin_port   = htons(sendAddr->port);
        FMSTR_MemCpy(&destAddr4.sin_addr.s_addr, sendAddr->addr.v4, 4);

        /* Send data */
        res = zsock_sendto(ses->sock, msgBuff, msgSize, 0, (struct sockaddr *)&destAddr4, sizeof(destAddr4));
    }
    else
#endif
    {
        /* Send data in blocking mode */
        res = zsock_send(ses->sock, msgBuff, msgSize, 0);
    }

    return res;
}

static void _FMSTR_NetZephyrTcpClose(FMSTR_NET_ADDR *addr)
{
    FMSTR_TCP_SESSION *ses = NULL;

    /* Find session by address */
    ses = _FMSTR_NetZephyrTcpSessionFindByAddr(addr);
    if (ses == NULL)
    {
        /* Session not found */
        return;
    }

#if FMSTR_NET_AUTODISCOVERY != 0
    if (ses->sock == fmstrUdpBroadcastSock)
    {
        /* Broadcast session cannot be closed */
        return;
    }
#endif

    /* Close socket */
    if(ses->sock >= 0)
    {
#if FMSTR_DEBUG_LEVEL >= 2
        FMSTR_DEBUG_PRINTF("TCP socket %d closing\n", ses->sock);
#endif
        (void)zsock_close(ses->sock);

        FMSTR_MemSet(ses, 0, sizeof(FMSTR_TCP_SESSION));
        ses->sock = -1;
    }
}

static void _FMSTR_NetZephyrTcpGetCaps(FMSTR_NET_IF_CAPS *caps)
{
    FMSTR_ASSERT(caps != NULL);

    caps->flags |= FMSTR_NET_IF_CAPS_FLAG_TCP;
}

static void _FMSTR_NetAddrToFmstr(struct sockaddr *remoteAddr, FMSTR_NET_ADDR *fmstrAddr)
{
    FMSTR_ASSERT(remoteAddr != NULL);
    FMSTR_ASSERT(fmstrAddr != NULL);

    if ((((struct sockaddr *)remoteAddr)->sa_family & AF_INET) != 0U)
    {
        struct sockaddr_in *in = (struct sockaddr_in *)remoteAddr;
        fmstrAddr->type        = FMSTR_NET_ADDR_TYPE_V4;
        FMSTR_MemCpy(fmstrAddr->addr.v4, &in->sin_addr.s_addr, sizeof(fmstrAddr->addr.v4));
        fmstrAddr->port = htons(in->sin_port);
    }
#if CONFIG_NET_IPV6
    else if ((((struct sockaddr *)remoteAddr)->sa_family & AF_INET6) != 0U)
    {
        struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)remoteAddr;

        fmstrAddr->type = FMSTR_NET_ADDR_TYPE_V6;
        FMSTR_MemCpy(fmstrAddr->addr.v6, &(in6->sin6_addr.s6_addr), sizeof(fmstrAddr->addr.v6));
        fmstrAddr->port = htons(in6->sin6_port);
    }
#endif
}

#endif /* (!(FMSTR_DISABLE)) */