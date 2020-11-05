/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      rtcp_session.c
* @brief     rtspclient rtcp session src file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "hi_rtsp_client.h"
#include "hi_rtsp_client_err.h"
#include "rtsp_client_stream.h"
#include "rtsp_client_socket.h"
#include "rtsp_client_msgparser.h"
#include "rtcp_session.h"
#include "rtsp_client_log.h"

/* start recv process */
HI_S32 RTCP_Session_Start(HI_RTCP_SESSION_S* pstSession)
{
    if (NULL == pstSession)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTCP_Session_Start param error\r\n");
        return HI_ERR_RTSPCLIENT_ILLEGAL_PARAM;
    }

    if (HI_LIVE_TRANS_TYPE_UDP == pstSession->enTransType)
    {
        pstSession->enRunState    = HI_RTP_RUNSTATE_INIT;
        pstSession->ptSessThdId = RTSPCLIENT_INVALID_THD_ID;
        RTSPCLIENT_SOCKET_UDPRecv(&pstSession->s32StreamSock, pstSession->s32ListenPort);
    }
    else if (HI_LIVE_TRANS_TYPE_TCP == pstSession->enTransType)
    {
        pstSession->enRunState = HI_RTP_RUNSTATE_RUNNING ;
    }
    else
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "not support transtype  error\r\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 RTCP_Session_Stop(HI_RTCP_SESSION_S* pstSession)
{

    if (NULL == pstSession)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTCP_Session_Stop param null\r\n");
        return HI_ERR_RTSPCLIENT_ILLEGAL_PARAM;
    }

    if (HI_LIVE_TRANS_TYPE_UDP == pstSession->enTransType)
    {
        HI_CLOSE_SOCKET(pstSession->s32StreamSock);
    }

    pstSession->enRunState = HI_RTP_RUNSTATE_INIT;

    return HI_SUCCESS;

}
HI_S32 RTCP_Session_Create(HI_RTCP_SESSION_S** ppstSession)
{
    HI_RTCP_SESSION_S* pstSession = NULL;

    if ( NULL == ppstSession )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTCP_Session_Create param null\r\n");
        return HI_ERR_RTSPCLIENT_ILLEGAL_PARAM;
    }

    pstSession = (HI_RTCP_SESSION_S*)malloc(sizeof(HI_RTCP_SESSION_S));
    if (NULL == pstSession)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "malloc for RTP_RECV_S error\r\n");
        return HI_FAILURE;
    }

    memset(pstSession, 0 , sizeof(HI_RTCP_SESSION_S));

    pstSession->pu8RtcpBuff = (HI_U8*)malloc(RTSPCLIENT_RTCP_PACKLEN);
    if (!pstSession->pu8RtcpBuff)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "malloc for pstSession->pu8RtcpBuff error\r\n");
        free(pstSession);
        pstSession = NULL;
        return HI_FAILURE;
    }
    memset(pstSession->pu8RtcpBuff,0x00,RTSPCLIENT_RTCP_PACKLEN);
    pstSession->u32RtcpBuffLen = RTSPCLIENT_RTCP_PACKLEN;

    *ppstSession = pstSession;
    return HI_SUCCESS;

}

HI_S32 RTCP_Session_Destroy(HI_RTCP_SESSION_S* pstSession)
{
    if (NULL == pstSession)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTCP_Session_Destroy NULL param  error\r\n");
        return HI_ERR_RTSPCLIENT_ILLEGAL_PARAM;
    }

    pstSession->enRunState = HI_RTP_RUNSTATE_INIT;

    if (NULL != pstSession->pu8RtcpBuff)
    {
        free(pstSession->pu8RtcpBuff);
        pstSession->pu8RtcpBuff = NULL;
    }

    free(pstSession);
    pstSession = NULL;

    return HI_SUCCESS;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
