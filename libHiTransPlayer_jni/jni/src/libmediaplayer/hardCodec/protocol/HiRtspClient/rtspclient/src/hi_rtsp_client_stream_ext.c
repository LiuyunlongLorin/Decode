/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      hi_rtsp_client_stream_ext.c
* @brief     rtspclient stream on demand connect process funcs src file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "base64.h"
#include "hi_rtsp_client.h"
#include "hi_rtsp_client_stream_ext.h"
#include "rtsp_client_msgparser.h"
#include "rtsp_client_socket.h"
#include "rtsp_client_stream.h"
#include "rtsp_client_log.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */
#ifndef MW_VERSION
#define MW_VERSION "0.0.0"
#endif
static volatile const HI_CHAR RTSPCLIENT_VERSIONINFO[] = "HIBERRY RTSPCLIENT MW Version: "MW_VERSION;
static volatile const HI_CHAR RTSPCLIENT_BUILD_DATE[] = "HIBERRY RTSPCLIENT Build Date:"__DATE__" Time:"__TIME__;

#define RTSP_OPTIONS_REQ (0)
#define RTSP_DESCRIBE_REQ (1)
#define RTSP_SETUP_REQ (2)
#define RTSP_PLAY_REQ (3)
#define RTSP_TEARDOWN_REQ (4)
#define RTSP_AUTHOR_ERROR (401)
#define RTSP_CLIENT_LISTENER_PORT (554)
#define RTSP_CLIENT_IP_LEN (22)
#define RTSP_CLIENT_MAX_STREAMHEADLEN   (512)
#define RTSP_CLIENT_ITLV_HEAD (0x24)
#define RTSP_CLIENT_STR "rtsp://"
#define RTSP_CLIENT_SETUP_OK (2)
#define RTSP_CLIENT_LOCK(mutex) \
    do \
    { \
        (HI_VOID)pthread_mutex_lock(&mutex);  \
    }while(0)

#define RTSP_CLIENT_UNLOCK(mutex)  \
    do \
    { \
        (HI_VOID)pthread_mutex_unlock(&mutex); \
    }while(0)


typedef struct hiRTSP_CLIENT_CTX_S
{
    List_Head_S pClientlist;
    pthread_mutex_t mListLock;
    HI_S32 s32UserNum;
} HI_RTSP_CLIENT_CTX_S;

static HI_RTSP_CLIENT_CTX_S* s_pstRTSPClientCTX = NULL;

static HI_BOOL RTSPCLIENT_FindClient(HI_MW_PTR hHandle)
{
    List_Head_S* pstPosNode = NULL;
    HI_RTSPCLIENT_STREAM_S* pstStreamNode = NULL;
    HI_RTSPCLIENT_STREAM_S* pstClient = (HI_RTSPCLIENT_STREAM_S*)hHandle;
    CHECK_NULL_ERROR(s_pstRTSPClientCTX);
    RTSP_CLIENT_LOCK(s_pstRTSPClientCTX->mListLock);
    HI_List_For_Each(pstPosNode, &s_pstRTSPClientCTX->pClientlist)
    {
        pstStreamNode = HI_LIST_ENTRY(pstPosNode, HI_RTSPCLIENT_STREAM_S, stStreamListPtr);
        if ( pstStreamNode == pstClient)
        {
            RTSP_CLIENT_UNLOCK(s_pstRTSPClientCTX->mListLock);
            return HI_TRUE;
        }
    }
    RTSP_CLIENT_UNLOCK(s_pstRTSPClientCTX->mListLock);

    return HI_FALSE;
}

static HI_VOID RTSPCLIENT_ClearOutBuff(HI_RTSPCLIENT_STREAM_S* pstSess)
{
    memset(pstSess->stProtoMsgInfo.szOutBuffer, 0, RTSPCLIENT_MAXBUFFER_SIZE);
    pstSess->stProtoMsgInfo.u32OutSize = 0;
    return;
}

static HI_S32 RTSPCLIENT_CheckRTSPconfig(HI_RTSPCLIENT_CONFIG_S* pstConfig)
{
    CHECK_NULL_ERROR(pstConfig->pfnOnMediaControl);
    CHECK_NULL_ERROR(pstConfig->pfnOnRecvAudio);
    CHECK_NULL_ERROR(pstConfig->pfnOnRecvVideo);
    return HI_SUCCESS;
}
static HI_VOID RTSPCLIENT_SetRTSPconfig(HI_RTSPCLIENT_STREAM_S* pstSession, HI_RTSPCLIENT_CONFIG_S* pstConfig)
{
    /*init start parameters of the session*/
    strncpy(pstSession->stProtoMsgInfo.szUrl, pstConfig->szUrl, RTSPCLIENT_MAXURL_LEN - 1);
    pstSession->stProtoMsgInfo.szUrl[RTSPCLIENT_MAXURL_LEN - 1] = '\0';
    pstSession->pfnOnRecvVideo = pstConfig->pfnOnRecvVideo;          /*video call back function*/
    pstSession->pfnOnRecvAudio = pstConfig->pfnOnRecvAudio;          /*audio call back function*/
    pstSession->pfnOnMediaControl = pstConfig->pfnOnMediaControl;
    pstSession->enStreamType = pstConfig->enStreamType;
    pstSession->ps32PrivData = (HI_S32*)pstConfig->argPriv;
    strncpy(pstSession->stAuthInfo.szUserName, pstConfig->stUserInfo.aszUserName, RTSPCLIENT_MAX_USERNAME_LEN - 1);
    pstSession->stAuthInfo.szUserName[RTSPCLIENT_MAX_USERNAME_LEN - 1] = '\0';
    strncpy(pstSession->stAuthInfo.szPassword, pstConfig->stUserInfo.aszPassword, RTSPCLIENT_MAX_PASSWORD_LEN - 1);
    pstSession->stAuthInfo.szPassword[RTSPCLIENT_MAX_PASSWORD_LEN - 1] = '\0';
    return ;
}
static HI_S32 RTSPCLIENT_CheckParam(HI_MW_PTR hClientHdl)
{
    CHECK_INVALID_HANDLE(hClientHdl);
    if (NULL == s_pstRTSPClientCTX)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "client not created\n");
        return HI_ERR_RTSPCLIENT_NOT_CREATE;
    }

    if (!RTSPCLIENT_FindClient(hClientHdl))
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " RTSPCLIENT  could not find %p!\n", hClientHdl);
        return HI_ERR_RTSPCLIENT_NOT_CREATE;
    }
    return HI_SUCCESS;
}
HI_S32 HI_RTSPCLIENT_Stream_Send_Option(HI_MW_PTR hClientHdl)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_RTSPCLIENT_STREAM_S* pstSession = NULL;

    s32Ret = RTSPCLIENT_CheckParam(hClientHdl);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " RTSPCLIENT_CheckParam FAIL %d!\n", s32Ret);
        return s32Ret;
    }
    pstSession = (HI_RTSPCLIENT_STREAM_S*)hClientHdl;

    RTSP_CLIENT_LOCK(pstSession->mSendMutex);

    RTSPCLIENT_Stream_Pack_Option(pstSession);
    s32Ret =  RTSPCLIENT_SOCKET_Send(pstSession->s32SessSock, (HI_U8*)pstSession->stProtoMsgInfo.szOutBuffer,
                                     strlen(pstSession->stProtoMsgInfo.szOutBuffer));
    if (s32Ret != HI_SUCCESS)
    {
        RTSP_CLIENT_UNLOCK(pstSession->mSendMutex);
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "send the OPTIONS request failed ret:%d\n", s32Ret);
        return HI_FAILURE;
    }
    pstSession->enProtocolPos = HI_RTSPCLIENT_METHOD_OPTIONS;

    RTSP_CLIENT_UNLOCK(pstSession->mSendMutex);

    return HI_SUCCESS;
}

HI_S32 HI_RTSPCLIENT_Stream_Send_Describe(HI_MW_PTR hClientHdl)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_RTSPCLIENT_STREAM_S* pstSession = NULL;
    s32Ret = RTSPCLIENT_CheckParam(hClientHdl);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " RTSPCLIENT_CheckParam FAIL %d!\n", s32Ret);
        return s32Ret;
    }
    pstSession = (HI_RTSPCLIENT_STREAM_S*)hClientHdl;

    RTSP_CLIENT_LOCK(pstSession->mSendMutex);

    RTSPCLIENT_ClearOutBuff(pstSession);
    RTSPCLIENT_Stream_Pack_Describe(pstSession);
    s32Ret = RTSPCLIENT_SOCKET_Send(pstSession->s32SessSock, (HI_U8*)pstSession->stProtoMsgInfo.szOutBuffer, strlen(pstSession->stProtoMsgInfo.szOutBuffer));
    if (s32Ret != HI_SUCCESS)
    {
        RTSP_CLIENT_UNLOCK(pstSession->mSendMutex);
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "send the DESCRIBE request failed\n");
        return HI_FAILURE;
    }
    pstSession->enProtocolPos = HI_RTSPCLIENT_METHOD_DESCRIBE;

    RTSP_CLIENT_UNLOCK(pstSession->mSendMutex);

    return HI_SUCCESS;
}

HI_S32 HI_RTSPCLIENT_Stream_Send_SetUp(HI_MW_PTR hClientHdl, HI_LIVE_STREAM_TYPE_E enStreamType)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_RTSPCLIENT_STREAM_S* pstSession = NULL;
    s32Ret = RTSPCLIENT_CheckParam(hClientHdl);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " RTSPCLIENT_CheckParam FAIL %d!\n", s32Ret);
        return s32Ret;
    }
    pstSession = (HI_RTSPCLIENT_STREAM_S*)hClientHdl;


    RTSP_CLIENT_LOCK(pstSession->mSendMutex);
    RTSPCLIENT_ClearOutBuff(pstSession);

    if (HI_LIVE_STREAM_TYPE_VIDEO == enStreamType)
    {
        s32Ret = RTSPCLIENT_Stream_Pack_SetUp(pstSession,  pstSession->stProtoMsgInfo.szVideoTrackID, HI_LIVE_STREAM_TYPE_VIDEO);
        if (s32Ret != HI_SUCCESS)
        {
            RTSP_CLIENT_UNLOCK(pstSession->mSendMutex);
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "Compose VIDEO Setup request failed\n");
            return HI_FAILURE;
        }
    }
    else if (HI_LIVE_STREAM_TYPE_AUDIO == enStreamType)
    {
        s32Ret = RTSPCLIENT_Stream_Pack_SetUp(pstSession,  pstSession->stProtoMsgInfo.szAudioTrackID, HI_LIVE_STREAM_TYPE_AUDIO);
        if (s32Ret != HI_SUCCESS)
        {
            RTSP_CLIENT_UNLOCK(pstSession->mSendMutex);
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "Compose AUDIO Setup request failed\n");
            return HI_FAILURE;
        }
    }
    else
    {
        RTSP_CLIENT_UNLOCK(pstSession->mSendMutex);
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " RTSPCLIENT  could not find %p!\n", hClientHdl);
        return HI_FAILURE;
    }

    s32Ret = RTSPCLIENT_SOCKET_Send(pstSession->s32SessSock, (HI_U8*)pstSession->stProtoMsgInfo.szOutBuffer, strlen(pstSession->stProtoMsgInfo.szOutBuffer));
    if (s32Ret != HI_SUCCESS)
    {
        RTSP_CLIENT_UNLOCK(pstSession->mSendMutex);
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "Send the Setup request failed\n");
        return HI_FAILURE;
    }

    pstSession->enProtocolPos = HI_RTSPCLIENT_METHOD_SETUP;
    RTSP_CLIENT_UNLOCK(pstSession->mSendMutex);

    return HI_SUCCESS;
}
HI_S32 HI_RTSPCLIENT_Stream_Send_Play(HI_MW_PTR hClientHdl)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_RTSPCLIENT_STREAM_S* pstSession = NULL;
    s32Ret = RTSPCLIENT_CheckParam(hClientHdl);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " RTSPCLIENT_CheckParam FAIL %d!\n", s32Ret);
        return s32Ret;
    }
    pstSession = (HI_RTSPCLIENT_STREAM_S*)hClientHdl;


    RTSP_CLIENT_LOCK(pstSession->mSendMutex);

    RTSPCLIENT_ClearOutBuff(pstSession);
    RTSPCLIENT_Stream_Pack_Play(pstSession);
    s32Ret = RTSPCLIENT_SOCKET_Send(pstSession->s32SessSock, (HI_U8*)pstSession->stProtoMsgInfo.szOutBuffer, strlen(pstSession->stProtoMsgInfo.szOutBuffer));

    if (s32Ret != HI_SUCCESS)
    {
        RTSP_CLIENT_UNLOCK(pstSession->mSendMutex);
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "send the PLAY request failed\n");
        return HI_FAILURE;
    }

    pstSession->enProtocolPos = HI_RTSPCLIENT_METHOD_PLAY;
    RTSP_CLIENT_UNLOCK(pstSession->mSendMutex);

    s32Ret = RTSPCLIENT_Stream_CreateRecvProcess(pstSession);
    if (s32Ret != HI_SUCCESS)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_Stream_CreateRecvProcess failed\n");
        return s32Ret;
    }

    return HI_SUCCESS;

}
HI_S32 HI_RTSPCLIENT_Stream_Send_Teardown(HI_MW_PTR hClientHdl)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_RTSPCLIENT_STREAM_S* pstSession = NULL;
    s32Ret = RTSPCLIENT_CheckParam(hClientHdl);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " RTSPCLIENT_CheckParam FAIL %d!\n", s32Ret);
        return s32Ret;
    }
    pstSession = (HI_RTSPCLIENT_STREAM_S*)hClientHdl;

    s32Ret = RTSPCLIENT_Stream_DestroyRecvProcess(pstSession);
    if ( HI_SUCCESS != s32Ret )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_Stream_CreateRecvProcess failed\n");
        return HI_FAILURE;
    }

    RTSP_CLIENT_LOCK(pstSession->mSendMutex);

    pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer, RTSPCLIENT_MAXBUFFER_SIZE,
            "TEARDOWN %s %s\r\n"
            "CSeq: %d\r\n"
            "Session: %s\r\n"
            "User-Agent: %s\r\n"
            "\r\n",
            pstSession->stProtoMsgInfo.szUrl,
            RTSP_VER,
            pstSession->stProtoMsgInfo.u32SendSeqNum++,
            pstSession->stProtoMsgInfo.szSessionId,
            pstSession->stProtoMsgInfo.szUserAgent);

    s32Ret = RTSPCLIENT_SOCKET_Send(pstSession->s32SessSock, (HI_U8*)pstSession->stProtoMsgInfo.szOutBuffer, strlen(pstSession->stProtoMsgInfo.szOutBuffer));
    if (s32Ret != HI_SUCCESS)
    {
        RTSP_CLIENT_UNLOCK(pstSession->mSendMutex);
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "send the Teardown request failed ret:%d\n", s32Ret);
        return HI_FAILURE;
    }
    pstSession->enProtocolPos = HI_RTSPCLIENT_METHOD_TEARDOWN;
    RTSP_CLIENT_UNLOCK(pstSession->mSendMutex);

    if (NULL != pstSession->pfnStateListener)
    {
        pstSession->pfnStateListener(pstSession->ps32PrivData, HI_PLAYSTAT_TYPE_NORMAL_DISCONNECT);
    }

    return HI_SUCCESS;
}

HI_S32 HI_RTSPCLIENT_Stream_SetUserAgent(HI_MW_PTR hClientHdl, HI_CHAR* pszUserAgent)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_RTSPCLIENT_STREAM_S* pstSession = NULL;
    s32Ret = RTSPCLIENT_CheckParam(hClientHdl);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " RTSPCLIENT_CheckParam FAIL %d!\n", s32Ret);
        return s32Ret;
    }
    pstSession = (HI_RTSPCLIENT_STREAM_S*)hClientHdl;


    snprintf(pstSession->stProtoMsgInfo.szUserAgent, RTSPCLIENT_VER_LEN, "%s/1.0.0(%s)", pszUserAgent, __DATE__);
    return HI_SUCCESS;
}

HI_S32  HI_RTSPCLIENT_Stream_GetMediaInfo(HI_MW_PTR hClientHdl, HI_LIVE_STREAM_INFO_S* pstStreamInfo)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_RTSPCLIENT_STREAM_S* pstSession = NULL;
    CHECK_NULL_ERROR(pstStreamInfo);
    s32Ret = RTSPCLIENT_CheckParam(hClientHdl);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " RTSPCLIENT_CheckParam FAIL %d!\n", s32Ret);
        return s32Ret;
    }
    pstSession = (HI_RTSPCLIENT_STREAM_S*)hClientHdl;

    if (HI_RTSPCLIENT_STATE_PLAY == pstSession->enClientState)
    {
        memcpy(pstStreamInfo, &(pstSession->stStreamInfo), sizeof(HI_LIVE_STREAM_INFO_S));
    }
    else
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "client state not ready\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 HI_RTSPCLIENT_Stream_SetClientCallback(HI_MW_PTR hClientHdl, HI_RTSP_CLIENT_LISTENER_S* pstListener)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_RTSPCLIENT_STREAM_S* pstSession = NULL;
    s32Ret = RTSPCLIENT_CheckParam(hClientHdl);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " RTSPCLIENT_CheckParam FAIL %d!\n", s32Ret);
        return s32Ret;
    }
    pstSession = (HI_RTSPCLIENT_STREAM_S*)hClientHdl;

    CHECK_NULL_ERROR(pstListener->onNotifyResponse);
    pstSession->stListener.onNotifyResponse = pstListener->onNotifyResponse;
    return HI_SUCCESS;
}

HI_S32  HI_RTSPCLIENT_Stream_SetStateListener(HI_MW_PTR hClientHdl, HI_LIVE_ON_EVENT pfnStateListener)
{
    HI_RTSPCLIENT_STREAM_S* pstSession = NULL;
    HI_S32 s32Ret = HI_SUCCESS;
    s32Ret = RTSPCLIENT_CheckParam(hClientHdl);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " RTSPCLIENT_CheckParam FAIL %d!\n", s32Ret);
        return s32Ret;
    }
    pstSession = (HI_RTSPCLIENT_STREAM_S*)hClientHdl;

    pstSession->pfnStateListener = pfnStateListener;
    return HI_SUCCESS;
}

HI_S32 HI_RTSPCLIENT_Stream_Create(HI_MW_PTR* phClientHdl, HI_RTSPCLIENT_CONFIG_S* pstConfig)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_RTSPCLIENT_STREAM_S* pstSession = NULL;
    CHECK_NULL_ERROR(phClientHdl);
    CHECK_NULL_ERROR(pstConfig);

    s32Ret = RTSPCLIENT_CheckRTSPconfig(pstConfig);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_CheckRTSPconfig failed:%d\n", s32Ret);
        return s32Ret;
    }

    if (NULL == s_pstRTSPClientCTX)
    {
        s_pstRTSPClientCTX = (HI_RTSP_CLIENT_CTX_S*)malloc(sizeof(HI_RTSP_CLIENT_CTX_S));
        if (!s_pstRTSPClientCTX)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "create s_pstRTSPClientCTX malloc failed\n");
            return HI_ERR_RTSPCLIENT_MALLOC_FAIL;
        }
        memset(s_pstRTSPClientCTX, 0x00, sizeof(HI_RTSP_CLIENT_CTX_S));
        s_pstRTSPClientCTX->s32UserNum = 0;
        (void)pthread_mutex_init(&s_pstRTSPClientCTX->mListLock,  NULL);
        HI_LIST_INIT_HEAD_PTR(&s_pstRTSPClientCTX->pClientlist);
    }

    pstSession = (HI_RTSPCLIENT_STREAM_S*)malloc(sizeof(HI_RTSPCLIENT_STREAM_S));
    if (!pstSession)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "create rtspclient session malloc failed\n");
        s32Ret = HI_ERR_RTSPCLIENT_MALLOC_FAIL;
        goto FREE_CTX;
    }
    memset(pstSession, 0x00, sizeof(HI_RTSPCLIENT_STREAM_S));
    pstSession->enClientState = HI_RTSPCLIENT_STATE_INIT;
    pstSession->enProtocolPos = HI_RTSPCLIENT_METHOD_BUTT;
    pstSession->s32SessSock   = RTSPCLIENT_INVALID_SOCKET;
    pstSession->ptSessThdId = RTSPCLIENT_INVALID_THD_ID;
    pstSession->ptResponseThdId = RTSPCLIENT_INVALID_THD_ID;
    pstSession->stAuthInfo.enAuthenticate = HI_RTSPCLIENT_AUTHEN_BUTT;
    RTSPCLIENT_MSGParser_GetPlayerVers(pstSession->stProtoMsgInfo.szUserAgent);
    RTSPCLIENT_SetRTSPconfig( pstSession, pstConfig);
    (void)pthread_mutex_init(&pstSession->mSendMutex, NULL);


    /*parse the url*/
    s32Ret = RTSPCLIENT_Stream_Parse_Url(pstSession);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_Parse_Url failed ret:%d\n", s32Ret);
        s32Ret =  HI_ERR_RTSPCLIENT_CREATE_FAIL;
        goto FREE_SESSION;
    }

    /*connect server */
    s32Ret = RTSPCLIENT_Stream_Connect(pstSession->stProtoMsgInfo.szServerIp, pstSession->s32SvrLisPort, &pstSession->s32SessSock);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_SOCKET_Connect failed ret %d\n", s32Ret);
        s32Ret = HI_ERR_RTSPCLIENT_CREATE_FAIL;
        goto FREE_SESSION;
    }

    s32Ret = pthread_create(&(pstSession->ptResponseThdId), NULL, RTSPCLIENT_Stream_RecvResponseProc, (HI_VOID*)pstSession);
    if (s32Ret !=  HI_SUCCESS)
    {
        pstSession->ptResponseThdId = RTSPCLIENT_INVALID_THD_ID;
        close(pstSession->s32SessSock);
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "pthread_create error %d\r\n", s32Ret);
        s32Ret = HI_ERR_RTSPCLIENT_CREATE_FAIL;
        goto FREE_SESSION;

    }

    RTSP_CLIENT_LOCK(s_pstRTSPClientCTX->mListLock);
    HI_List_Add(&(pstSession->stStreamListPtr), &(s_pstRTSPClientCTX->pClientlist));
    s_pstRTSPClientCTX->s32UserNum += 1;

    RTSP_CLIENT_UNLOCK(s_pstRTSPClientCTX->mListLock);

    *phClientHdl = (HI_MW_PTR)pstSession;

    return HI_SUCCESS;

FREE_SESSION:
    if (pstSession)
    {
        pthread_mutex_destroy(&pstSession->mSendMutex);
        free(pstSession);
        pstSession = NULL;
    }

FREE_CTX:
    if (0 == s_pstRTSPClientCTX->s32UserNum)
    {
        pthread_mutex_destroy(&s_pstRTSPClientCTX->mListLock);
        free(s_pstRTSPClientCTX);
        s_pstRTSPClientCTX = NULL;
    }
    return s32Ret;
}

HI_S32 HI_RTSPCLIENT_Stream_Destroy(HI_MW_PTR hClientHdl)
{
    HI_RTSPCLIENT_STREAM_S* pstSession = NULL;
    List_Head_S* pstPosNode = NULL;
    List_Head_S* pstTmpNode = NULL;
    HI_RTSPCLIENT_STREAM_S* pstStreamNode = NULL;
    HI_S32 s32Ret = HI_SUCCESS;
    s32Ret = RTSPCLIENT_CheckParam(hClientHdl);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " RTSPCLIENT_CheckParam FAIL %d!\n", s32Ret);
        return s32Ret;
    }
    pstSession = (HI_RTSPCLIENT_STREAM_S*)hClientHdl;


    RTSP_CLIENT_LOCK(s_pstRTSPClientCTX->mListLock);

    HI_List_For_Each_Safe(pstPosNode, pstTmpNode, &s_pstRTSPClientCTX->pClientlist)
    {
        pstStreamNode = HI_LIST_ENTRY(pstPosNode, HI_RTSPCLIENT_STREAM_S, stStreamListPtr);
        if ( pstStreamNode == pstSession)
        {
            HI_List_Del(&(pstStreamNode->stStreamListPtr));
            s_pstRTSPClientCTX->s32UserNum -= 1;
        }
    }

    RTSP_CLIENT_UNLOCK(s_pstRTSPClientCTX->mListLock);

    if (RTSPCLIENT_INVALID_THD_ID != pstSession->ptResponseThdId)
    {
        if (HI_SUCCESS != pthread_join(pstSession->ptResponseThdId, NULL))
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "join stream recv pthread fail\n");
        }

        pstSession->ptResponseThdId = RTSPCLIENT_INVALID_THD_ID;
    }

    if (NULL != pstSession->pstRtpAudio)
    {
        RTP_Session_Destroy(pstSession->pstRtpAudio);
        pstSession->pstRtpAudio = NULL;
    }

    if (NULL != pstSession->pstRtcpAudio)
    {
        RTCP_Session_Destroy(pstSession->pstRtcpAudio);
        pstSession->pstRtcpAudio = NULL;
    }

    if (NULL != pstSession->pstRtpVideo)
    {
        RTP_Session_Destroy(pstSession->pstRtpVideo);
        pstSession->pstRtpVideo = NULL;
    }

    if (NULL != pstSession->pstRtcpVideo)
    {
        RTCP_Session_Destroy(pstSession->pstRtcpVideo);
        pstSession->pstRtcpVideo = NULL;
    }

    HI_CLOSE_SOCKET(pstSession->s32SessSock);
    pthread_mutex_destroy(&pstSession->mSendMutex);

    free(pstSession);
    pstSession = NULL;

    if ( 0 == s_pstRTSPClientCTX->s32UserNum)
    {
        free(s_pstRTSPClientCTX);
        s_pstRTSPClientCTX = NULL;
    }

    return HI_SUCCESS;
}
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
