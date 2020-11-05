/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      rtspclient_common.h
* @brief     rtspclient common funcs head file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/

#ifndef __RTSPCLIENT_COMMON_H__
#define __RTSPCLIENT_COMMON_H__

#include <netinet/in.h>
#include <stdio.h>
#include "hi_type.h"
#include "hi_rtsp_client_comm.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#define RTSP_VER  "RTSP/1.0"
#define RTSPCLIENT_INVALID_SOCKET (-1)
#define RTSPCLIENT_TRANS_TIMEVAL_SEC  (5)
#define RTSPCLIENT_TRANS_TIMEVAL_USEC  (500000)
#define RTSPCLIENT_MAX_WAIT_COUNT (10)
#define HI_CLOSE_SOCKET(sockfd)          do{\
        if(0<=sockfd)\
        {\
            close(sockfd);\
            sockfd = RTSPCLIENT_INVALID_SOCKET;\
        }\
    }while(0)

/**
 * @brief rtspclient create udp recv socket.
 * @param[in] ps32Sock HI_S32* :   socket fd
 * @param[in] s32ListenPort HI_S32 : client udp listen port
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 RTSPCLIENT_SOCKET_UDPRecv(HI_S32 *ps32Sock,HI_S32  s32ListenPort);

/**
 * @brief send client data.
 * @param[in] s32WritSock HI_S32 :  write socket fd
 * @param[in] pu8InBuff HI_U8* : msg out buffer
 * @param[in] u32DataLen HI_U32 : msg  length
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 RTSPCLIENT_SOCKET_Send(HI_S32 s32WritSock, HI_U8* pu8Buff, HI_U32 u32DataLen);

/**
 * @brief read data from server.
 * @param[in] s32Sock HI_S32 : socket fd
 * @param[in] pu8InBuff HI_U8* : data in buffer
 * @param[in] u32Len HI_U32 : data  length
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 RTSPCLIENT_SOCKET_Read(HI_S32 s32Sock, HI_U8* pu8Buff, HI_U32 u32Len);

/**
 * @brief rtspclient read msg line by line.
 * @param[in] s32Sock HI_S32 : socket fd
 * @param[in] pu8InBuff HI_U8* : msg in buffer
 * @param[in] u32BuffLen HI_U32 : msg  length
 * @param[in] pu32LineLen HI_U32* : line length
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 RTSPCLIENT_SOCKET_ReadLine(HI_S32 s32Sock, HI_U8* pu8InBuff, HI_U32 u32BuffLen, HI_U32* pu32LineLen);

/**
 * @brief rtspclient read protocol message.
 * @param[in] s32Sock HI_S32 : socket fd
 * @param[in] pu8InBuff HI_U8* : msg in buffer
 * @param[in] u32BuffLen HI_U32 : buff length
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 RTSPCLIENT_SOCKET_ReadProtocolMsg(HI_S32 s32Sock, HI_U8* pu8InBuff, HI_U32 u32BuffLen);

/**
 * @brief  rtspclient get udp port by sock.
 * @param[in] s32Fd HI_S32 : udp socket fd
  * @param[in] SockAddr sockaddr_in : sock address
 * @return   udp port
 */
HI_S32 RTSPCLIENT_SOCKET_GetUdpPort(HI_S32 s32Fd, struct sockaddr_in SockAddr);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
#endif  /* __RTSPCLIENT_COMMON_H__ */
