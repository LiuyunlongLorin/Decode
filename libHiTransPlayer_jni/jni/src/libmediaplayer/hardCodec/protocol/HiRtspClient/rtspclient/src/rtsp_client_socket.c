/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      rtspclient_common.c
* @brief     rtspclient common funcs src file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <string.h>
#include "hi_rtsp_client.h"
#include "hi_rtsp_client_comm.h"
#include "rtsp_client_socket.h"
#include "rtsp_client_log.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* __cplusplus */

HI_S32 RTSPCLIENT_SOCKET_UDPRecv(HI_S32* ps32Sock, HI_S32  s32ListenPort)
{
    struct sockaddr_in local_addr = {0};
    HI_S32 s32SocketOpt = 1;
    HI_S32 s32StreamSock;

    if (( -1) == (s32StreamSock = socket(AF_INET, SOCK_DGRAM, 0)) )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "create socket for recv data error%s.\n", strerror(errno));
        return HI_FAILURE;
    }

    s32SocketOpt = 1;

    if ((-1) == setsockopt(s32StreamSock , SOL_SOCKET, SO_REUSEADDR, (HI_VOID*)&s32SocketOpt, sizeof(HI_S32)) )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "socket %d (for recv data) setsockopt SO_REUSEADDR error.\n",
                      s32StreamSock);
        close(s32StreamSock);
        return HI_FAILURE ;
    }

    local_addr.sin_family = AF_INET; /* host byte order*/
    local_addr.sin_port  =  htons(s32ListenPort); /*short, network byte order*/
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);/*automatically fill with my IP*/

    memset(&(local_addr.sin_zero), '\0', sizeof(local_addr.sin_zero)); /*zero the rest of the struct*/

    if ( (-1) == bind(s32StreamSock, (struct sockaddr*)&local_addr, sizeof(struct sockaddr)))
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "socket %d (for recv data) bind error.\n", s32StreamSock);
        close(s32StreamSock);
        return HI_FAILURE ;
    }

    *ps32Sock = s32StreamSock;

    return HI_SUCCESS;
}

HI_S32 RTSPCLIENT_SOCKET_Send(HI_S32 s32WritSock, HI_U8* pu8Buff, HI_U32 u32DataLen)
{
    HI_S32 s32Ret = 0;
    HI_S32 s32Size    = 0;
    HI_CHAR*  pszBufferPos = NULL;
    fd_set write_fds;
    struct timeval TimeoutVal;  /* Timeout value */
    HI_S32 s32Errno = 0;

    memset(&TimeoutVal, 0, sizeof(struct timeval));

    pszBufferPos = (HI_CHAR*)pu8Buff;

    if (s32WritSock < 0)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " LIVE CLIENT send error:Sock is %d\n", s32WritSock);
        return HI_FAILURE;
    }

    while (u32DataLen > 0)
    {
        FD_ZERO(&write_fds);
        FD_SET(s32WritSock, &write_fds);
        TimeoutVal.tv_sec = RTSPCLIENT_TRANS_TIMEVAL_SEC;
        TimeoutVal.tv_usec = RTSPCLIENT_TRANS_TIMEVAL_USEC;
        /*judge if it can send */
        s32Ret = select(s32WritSock + 1, NULL, &write_fds, NULL, &TimeoutVal);
        /*select found over time or error happend*/
        if ( 0 == s32Ret)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "LIVE CLIENT send error:select overtime %d sec\n",
                          RTSPCLIENT_TRANS_TIMEVAL_SEC);
            return HI_FAILURE;
        }
        else if ( 0 > s32Ret )
        {
            if (EINTR == errno || EAGAIN == errno)
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "  [select err: %s]\n",  strerror(errno));
                continue;
            }
            s32Errno = errno;
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "LIVE CLIENT Send error:%s\n", strerror(s32Errno));
            return HI_FAILURE;
        }

        if ( !FD_ISSET(s32WritSock, &write_fds))
        {
            s32Errno = errno;
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "LIVE CLIENT send error:%s \n", strerror(s32Errno));
            return HI_FAILURE;
        }

        s32Size = send(s32WritSock, pszBufferPos, u32DataLen, 0);
        if ( 0 > s32Size )
        {
            /*if it is not eagain error, means can not send*/
            if (errno != EAGAIN  && errno != EINTR)
            {
                s32Errno = errno;
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " RTSP CLIENT send error:%s\n", strerror(s32Errno));
                return HI_FAILURE;
            }

            /*it is eagain error, means can try again*/
            continue;
        }
        u32DataLen -= s32Size;
        pszBufferPos += s32Size;
    }

    return HI_SUCCESS;
}

HI_S32 RTSPCLIENT_SOCKET_Read(HI_S32 s32Sock, HI_U8* pu8Buff, HI_U32 u32Len)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32Offset = 0;
    HI_S32 s32RecvBytes = 0;
    HI_S32 s32Errno = 0;
    HI_S32 s32FdMax = 0;
    HI_S32 s32WaitCount = 0;
    fd_set rfds;
    struct timeval tv = {0, 0};

    if ( 0 > s32Sock )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " LIVE CLIENT recv error:Sock is %d\n", s32Sock);
        return HI_FAILURE;
    }

    while (u32Offset < u32Len)
    {
        s32FdMax = s32Sock;
        FD_ZERO(&rfds);
        FD_SET(s32Sock, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = RTSPCLIENT_TRANS_TIMEVAL_USEC;
        s32Ret = select(s32FdMax + 1, &rfds, NULL, NULL, &tv);
        /*select found over time or error happend*/
        if ( 0 == s32Ret )
        {
            /*wait 500ms per time ,if wait for 10 times (5s in sum ),then return error*/
            if (s32WaitCount < RTSPCLIENT_MAX_WAIT_COUNT)
            {
                s32WaitCount++;
                continue;
            }
            else
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "Recv error: select overtime %d sec\n",
                              RTSPCLIENT_TRANS_TIMEVAL_SEC);
                return HI_FAILURE;
            }
        }
        else if ( 0 > s32Ret )
        {
            if (EINTR == errno || EAGAIN == errno)
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "  [select err: %s]\n",  strerror(errno));
                continue;
            }
            s32Errno = errno;
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "select error:%s\n", strerror(s32Errno));
            return HI_FAILURE;
        }
        /*if select success,reset the wait time*/
        s32WaitCount = 0;

        if ( !FD_ISSET(s32Sock, &rfds))
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR,
                          "recv data from sock %d error3: fd not in fd set\r\n", s32Sock);
            return HI_FAILURE;
        }

        s32RecvBytes = recv(s32Sock, pu8Buff + u32Offset, u32Len - u32Offset, 0);
        if ( 0 == s32RecvBytes)
        {
            s32Errno = errno;
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "recv data from sock %d error1: %s\r\n", s32Sock, strerror(s32Errno));
            return HI_FAILURE;
        }
        else if ( 0 > s32RecvBytes)
        {
            s32Errno = errno;
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "recv data from sock %d error2: %s\r\n", s32Sock, strerror(s32Errno));
            return HI_FAILURE;
        }

        u32Offset += s32RecvBytes;

    }

    return HI_SUCCESS;
}

/*
 * get gets a line from stream
 * and returns a null terminated string.
 */

HI_S32 RTSPCLIENT_SOCKET_ReadLine(HI_S32 s32Sock, HI_U8* pInBuff, HI_U32 u32BuffLen, HI_U32* pu32LineLen)
{
    HI_U32 u32LineLen  = 0;
    HI_S32 s32Ret = HI_SUCCESS;
    fd_set rfds;
    struct timeval TimeVal;
    memset(&TimeVal, 0, sizeof(struct timeval));

    /*set clear when overtime*/
    FD_ZERO(&rfds);
    FD_SET(s32Sock, &rfds);

    /* wait for 5 second */
    TimeVal.tv_sec = RTSPCLIENT_TRANS_TIMEVAL_SEC;
    TimeVal.tv_usec = RTSPCLIENT_TRANS_TIMEVAL_USEC;
    s32Ret = select(s32Sock + 1, &rfds, NULL , NULL, &TimeVal);

    if ( 0 >= s32Ret )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "receive select:%s\n", strerror(errno));
        return HI_FAILURE;
    }

    while (u32LineLen < u32BuffLen)
    {
        s32Ret = recv(s32Sock, &(pInBuff[u32LineLen]), 1, 0);
        if (s32Ret <= 0)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "receive error:%s\n", strerror(errno));
            return HI_FAILURE;
        }

        u32LineLen ++;
        if (u32LineLen >= 2)
        {
            if ('\r' == pInBuff[u32LineLen - 2] &&  '\n'  == pInBuff[u32LineLen - 1])
            {
                break;
            }
        }

    }

    if (u32LineLen >= u32BuffLen)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " protocal response 's line over %d bytes "\
                      "and msg procedure failed!\n", u32BuffLen);
        return HI_FAILURE;
    }

    *pu32LineLen = u32LineLen;
    pInBuff[u32LineLen] = '\0';
    return HI_SUCCESS;
}

HI_S32 RTSPCLIENT_SOCKET_ReadProtocolMsg(HI_S32 s32Sock, HI_U8* pInBuff, HI_U32 buffLen)
{
    HI_S32 s32Ret = 0;
    HI_U8* pAddr = NULL;
    HI_U32 u32RemBuffLen = 0;
    HI_CHAR* pchHead = NULL;
    HI_CHAR* pchRtspHead = NULL;
    HI_U32 u32ContentLen = 0;
    HI_U32 u32LineLen = 0;
    HI_U8* pLinehead = NULL;

    pAddr = pInBuff;
    u32RemBuffLen = buffLen;

    /*read  200 ok  standard message header£¬end in \r\n*/
    do
    {
        s32Ret = RTSPCLIENT_SOCKET_ReadLine(s32Sock, pAddr, u32RemBuffLen, &u32LineLen);

        if (HI_SUCCESS != s32Ret)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "readline failed\n");
            return s32Ret;
        }

        pLinehead = pAddr;
        pAddr += u32LineLen;
        u32RemBuffLen   -= u32LineLen;

    }
    while ( !(( '\n' == pLinehead[1] ) && ( '\r' == pLinehead[0])));

    /*test if has content-len*/
    pchHead = strstr((HI_CHAR*)pInBuff, "200 OK");
    if (NULL == pchHead)
    {
        pchRtspHead = strstr((HI_CHAR*)pInBuff, RTSP_VER);
        if ( NULL != pchRtspHead)
        {
            pchRtspHead = pchRtspHead + strlen(RTSP_VER) + 1;
            return atoi(pchRtspHead);
        }
        else
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "unsupported protocal head\n");
            return HI_FAILURE;
        }
    }

    pchHead = strstr((HI_CHAR*)pInBuff, "Content-Length:");
    if (NULL != pchHead)
    {
        u32ContentLen = atoi(pchHead + strlen("Content-Length:"));

        if (u32ContentLen > (u32RemBuffLen - 1))
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "read protocol msg fail contentLen larger than max size\n");
            return HI_FAILURE;
        }

        /*continue to read the packet*/
        s32Ret = RTSPCLIENT_SOCKET_Read(s32Sock, pAddr, u32ContentLen);
        if (HI_SUCCESS != s32Ret)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_SOCKET_Read fail\n");
            return s32Ret;
        }

        pAddr = pAddr + u32ContentLen;
        *pAddr  = '\0';
    }

    return HI_SUCCESS;

}

HI_S32 RTSPCLIENT_SOCKET_GetUdpPort(HI_S32 s32Fd, struct sockaddr_in SockAddr)
{
    HI_S32 s32NameLen = sizeof(struct sockaddr_in);

    if ( 0 != getsockname (s32Fd, (struct sockaddr*) &SockAddr, (socklen_t*)&s32NameLen) )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "getsockname fail\n");
        return HI_FAILURE;
    }

    return ntohs( SockAddr.sin_port );
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
