/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      rtcp_session.h
* @brief     rtspclient rtcp session head file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/

#ifndef __RTCP_SESSION_H__
#define __RTCP_SESSION_H__

#include <pthread.h>
#include "hi_rtsp_client_comm.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#define RTSPCLIENT_RTCP_PACKLEN (1500)

typedef struct hiRTCP_SESSION_S
{
    pthread_t         ptSessThdId;       /* thread to recv cmd */
    HI_S32              s32ListenPort;
    HI_S32    s32StreamSock;
    HI_S32    s32SessSock;
    HI_RTP_PT_E          enPayloadType;           /*payload type*/
    HI_CHAR          szURL[RTSPCLIENT_MAXURL_LEN];
    HI_U16              u16LastSn;      /*last recv sn*/
    HI_U32              u32LastTs;      /*last recv sn*/
    HI_LIVE_ON_RECV      pfnRecvVideo;  /*video callbak fun*/
    HI_LIVE_ON_RECV      pfnRecvAudio;  /*audio callbak fun*/
    HI_RTP_RUNSTATE_E      enRunState;
    HI_LIVE_TRANS_TYPE_E enTransType;
    HI_RTP_PACK_TYPE_E enStreamPackType;  /*packettype*/
    HI_S32 s32RecvPort;
    HI_U8*  pu8RtcpBuff;/*buf for recv rtcp data*/
    HI_U32  u32RtcpBuffLen;/*buf len for recv rtcp data*/
} HI_RTCP_SESSION_S;

/**
 * @brief stop rtcp session.
 * @param[in] pstSession HI_RTCP_SESSION_S* : pstSession struct
 * @return   0 success
 * @return  -1 failure
 */
HI_S32  RTCP_Session_Stop(HI_RTCP_SESSION_S* pstSession);

/**
 * @brief start rtcp session.
 * @param[in] pstSession HI_RTCP_SESSION_S* : pstSession struct
 * @return   0 success
 * @return  -1 failure
 */
HI_S32  RTCP_Session_Start(HI_RTCP_SESSION_S* pstSession);

/**
 * @brief create rtcp session.
 * @param[in] pstSession HI_RTCP_SESSION_S** : pstSession struct
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 RTCP_Session_Create(HI_RTCP_SESSION_S** ppstSession);

/**
 * @brief destroy rtcp session.
 * @param[in] pstSession HI_RTCP_SESSION_S* : pstSession struct
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 RTCP_Session_Destroy(HI_RTCP_SESSION_S* pstSession);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif/* __RTCP_SESSION_H__ */
