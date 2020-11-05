/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      hi_rtsp_client.c
* @brief     rtsp client manage src file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "hi_list.h"
#include "hi_rtsp_client.h"
#include "hi_rtsp_client_comm.h"
#include "hi_rtsp_client_err.h"
#include "hi_rtsp_client_stream_ext.h"
#include "rtsp_client_stream.h"
#include "rtsp_client_socket.h"
#include "rtsp_client_msgparser.h"
#include "rtsp_client_log.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */
typedef struct hiRTSPCLIENT_LIST_S
{
    List_Head_S stClientList;
    pthread_mutex_t mListLock;
} HI_RTSPCLIENT_LIST_S;

typedef struct hiRTSPCLIENT_NODE_S
{
    List_Head_S stNodeListPtr;
    HI_MW_PTR hClientHdl;
    HI_LIVE_STREAM_TYPE_E enStreamType;
    HI_BOOL bNeedSetupVideo;
    HI_BOOL bNeedSetupAudio;
    HI_RTSPCLIENT_METHOD_E enMethod;
    pthread_cond_t      mSendCond;
    pthread_mutex_t    mSendMutex;
    pthread_t ptSendThdId;
} HI_RTSPCLIENT_NODE_S;

static HI_RTSPCLIENT_NODE_S* s_pstClientNode = NULL;

static HI_S32 RTSPCLIENT_OnNotifyResponse(__attribute__((unused))HI_MW_PTR hClientHdl, const HI_CHAR* pszResponse, HI_RTSPCLIENT_METHOD_E enMethod)
{
    switch (enMethod)
    {
        case HI_RTSPCLIENT_METHOD_OPTIONS:

            (HI_VOID)pthread_mutex_lock(&(s_pstClientNode->mSendMutex));
            s_pstClientNode->enMethod = HI_RTSPCLIENT_METHOD_OPTIONS;
            (HI_VOID)pthread_cond_signal(&(s_pstClientNode->mSendCond));
            (HI_VOID)pthread_mutex_unlock(&(s_pstClientNode->mSendMutex));

            break;

        case HI_RTSPCLIENT_METHOD_DESCRIBE:
            if ( strstr(pszResponse, "m=video") && (HI_LIVE_STREAM_TYPE_AV == s_pstClientNode->enStreamType || HI_LIVE_STREAM_TYPE_VIDEO == s_pstClientNode->enStreamType))
            {
                s_pstClientNode->bNeedSetupVideo = HI_TRUE;
            }
            if ( strstr(pszResponse, "m=audio") && (HI_LIVE_STREAM_TYPE_AV == s_pstClientNode->enStreamType || HI_LIVE_STREAM_TYPE_AUDIO == s_pstClientNode->enStreamType))
            {
                s_pstClientNode->bNeedSetupAudio = HI_TRUE;
            }

            (HI_VOID)pthread_mutex_lock(&(s_pstClientNode->mSendMutex));
            s_pstClientNode->enMethod = HI_RTSPCLIENT_METHOD_DESCRIBE;
            (HI_VOID)pthread_cond_signal(&(s_pstClientNode->mSendCond));
            (HI_VOID)pthread_mutex_unlock(&(s_pstClientNode->mSendMutex));

            break;

        case HI_RTSPCLIENT_METHOD_SETUP:
            (HI_VOID)pthread_mutex_lock(&(s_pstClientNode->mSendMutex));
            s_pstClientNode->enMethod = HI_RTSPCLIENT_METHOD_SETUP;
            (HI_VOID)pthread_cond_signal(&(s_pstClientNode->mSendCond));
            (HI_VOID)pthread_mutex_unlock(&(s_pstClientNode->mSendMutex));

            break;

        case HI_RTSPCLIENT_METHOD_PLAY:
            (HI_VOID)pthread_mutex_lock(&(s_pstClientNode->mSendMutex));
            s_pstClientNode->enMethod = HI_RTSPCLIENT_METHOD_PLAY;
            (HI_VOID)pthread_cond_signal(&(s_pstClientNode->mSendCond));
            (HI_VOID)pthread_mutex_unlock(&(s_pstClientNode->mSendMutex));
            break;

        default:
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT response not supported do not handle\n");
            break;
    }

    return HI_SUCCESS;
}
static HI_S32 RTSPCLIENT_HandleDescribe(HI_VOID)
{
    HI_S32 s32Ret = 0;
    if (HI_LIVE_STREAM_TYPE_AV == s_pstClientNode->enStreamType || HI_LIVE_STREAM_TYPE_VIDEO == s_pstClientNode->enStreamType)
    {
        s32Ret = HI_RTSPCLIENT_Stream_Send_SetUp(s_pstClientNode->hClientHdl, HI_LIVE_STREAM_TYPE_VIDEO);
        if (HI_SUCCESS != s32Ret)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_Send_SetUp video failed\n");
            return s32Ret;
        }
        s_pstClientNode->bNeedSetupVideo = HI_FALSE;
    }
    else if (HI_LIVE_STREAM_TYPE_AV == s_pstClientNode->enStreamType || HI_LIVE_STREAM_TYPE_AUDIO == s_pstClientNode->enStreamType)
    {
        s32Ret = HI_RTSPCLIENT_Stream_Send_SetUp(s_pstClientNode->hClientHdl, HI_LIVE_STREAM_TYPE_AUDIO);
        if (HI_SUCCESS != s32Ret)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_Send_SetUp audio failed\n");
            return s32Ret;
        }
        s_pstClientNode->bNeedSetupAudio = HI_FALSE;
    }
    else
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_Send_SetUp failed\n");
        return s32Ret;
    }

    return HI_SUCCESS;
}
static HI_S32 RTSPCLIENT_HandleSetup(HI_VOID)
{
    HI_S32 s32Ret = 0;

    if (s_pstClientNode->bNeedSetupAudio)
    {
        s32Ret = HI_RTSPCLIENT_Stream_Send_SetUp(s_pstClientNode->hClientHdl, HI_LIVE_STREAM_TYPE_AUDIO);
        if (HI_SUCCESS != s32Ret)
        {

            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_Send_SetUp failed\n");
            return s32Ret;
        }
        s_pstClientNode->bNeedSetupAudio = HI_FALSE;
    }
    else if (s_pstClientNode->bNeedSetupVideo)
    {
        s32Ret = HI_RTSPCLIENT_Stream_Send_SetUp(s_pstClientNode->hClientHdl, HI_LIVE_STREAM_TYPE_VIDEO);
        if (HI_SUCCESS != s32Ret)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_Send_SetUp failed\n");
            return s32Ret;
        }
        s_pstClientNode->bNeedSetupVideo = HI_FALSE;
    }
    else
    {
        s32Ret = HI_RTSPCLIENT_Stream_Send_Play(s_pstClientNode->hClientHdl);
        if (HI_SUCCESS != s32Ret)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_Send_Play failed\n");
            return s32Ret;
        }
    }

    return HI_SUCCESS;
}

static HI_VOID* RTSPCLIENT_SendProc(__attribute__((unused))HI_VOID* arg)
{
    HI_S32 s32Ret = HI_SUCCESS;

    while (HI_RTSPCLIENT_METHOD_PLAY != s_pstClientNode->enMethod && HI_RTSPCLIENT_METHOD_TEARDOWN != s_pstClientNode->enMethod)
    {
        (HI_VOID)pthread_mutex_lock(&(s_pstClientNode->mSendMutex));

        (HI_VOID)pthread_cond_wait(&(s_pstClientNode->mSendCond), &(s_pstClientNode->mSendMutex));

        switch (s_pstClientNode->enMethod)
        {
            case HI_RTSPCLIENT_METHOD_OPTIONS:
                s32Ret = HI_RTSPCLIENT_Stream_Send_Describe(s_pstClientNode->hClientHdl);
                if (HI_SUCCESS != s32Ret)
                {
                    (HI_VOID)pthread_mutex_unlock(&(s_pstClientNode->mSendMutex));
                    RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_Send_Describe failed\n");
                    goto ErrorProc;
                }
                break;

            case HI_RTSPCLIENT_METHOD_DESCRIBE:
                s32Ret = RTSPCLIENT_HandleDescribe();
                if (HI_SUCCESS != s32Ret)
                {
                    (HI_VOID)pthread_mutex_unlock(&(s_pstClientNode->mSendMutex));
                    RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_HandleDescribe failed\n");
                    goto ErrorProc;
                }
                break;

            case HI_RTSPCLIENT_METHOD_SETUP:
                s32Ret = RTSPCLIENT_HandleSetup();
                if (HI_SUCCESS != s32Ret)
                {
                    (HI_VOID)pthread_mutex_unlock(&(s_pstClientNode->mSendMutex));
                    RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_HandleSetup failed\n");
                    goto ErrorProc;

                }
                break;

            case HI_RTSPCLIENT_METHOD_PLAY:
                break;

            case HI_RTSPCLIENT_METHOD_TEARDOWN:
                break;

            default:
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT response do not handle\n");
                break;
        }

        (HI_VOID)pthread_mutex_unlock(&(s_pstClientNode->mSendMutex));
    }

ErrorProc:
    if (s_pstClientNode->enMethod != HI_RTSPCLIENT_METHOD_PLAY && s_pstClientNode->enMethod != HI_RTSPCLIENT_METHOD_TEARDOWN)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_SendProc error \n");
    }

    return HI_NULL;
}

HI_S32  HI_RTSPCLIENT_GetMediaInfo(HI_MW_PTR hClientHdl, HI_LIVE_STREAM_INFO_S* pstStreamInfo)
{
    HI_S32 s32Ret = HI_SUCCESS;
    CHECK_INVALID_HANDLE(hClientHdl);
    CHECK_NULL_ERROR(pstStreamInfo);

    s32Ret = HI_RTSPCLIENT_Stream_GetMediaInfo(hClientHdl, pstStreamInfo);
    if (HI_SUCCESS != s32Ret )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_GetMediaInfo fail\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;

}

HI_S32  HI_RTSPCLIENT_SetListener(HI_MW_PTR hClientHdl, HI_LIVE_ON_EVENT pfnStateListener)
{
    HI_S32 s32Ret = HI_SUCCESS;
    CHECK_INVALID_HANDLE(hClientHdl);
    CHECK_NULL_ERROR(pfnStateListener);
    HI_RTSP_CLIENT_LISTENER_S stListener;
    memset(&stListener, 0x00, sizeof(HI_RTSP_CLIENT_LISTENER_S));
    s32Ret = HI_RTSPCLIENT_Stream_SetStateListener(hClientHdl, pfnStateListener);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " HI_RTSPCLIENT_Stream_SetListener fail %p!\n", hClientHdl);
        return HI_ERR_RTSPCLIENT_NOT_CREATE;
    }

    stListener.onNotifyResponse = RTSPCLIENT_OnNotifyResponse;
    s32Ret = HI_RTSPCLIENT_Stream_SetClientCallback(hClientHdl, &stListener);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_Stream_SetOnDestroy failed\n");
        return HI_ERR_RTSPCLIENT_CREATE_FAIL;
    }

    return HI_SUCCESS;
}

HI_S32  HI_RTSPCLIENT_SetUserAgent(HI_MW_PTR hClientHdl, HI_CHAR* pszUserAgent)
{
    CHECK_INVALID_HANDLE(hClientHdl);
    CHECK_NULL_ERROR(pszUserAgent);
    HI_S32 s32Ret = HI_SUCCESS;

    s32Ret = HI_RTSPCLIENT_Stream_SetUserAgent(hClientHdl, pszUserAgent);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, " HI_RTSPCLIENT_Stream_SetUserAgent fail %p!\n", hClientHdl);
        return s32Ret;
    }
    return HI_SUCCESS;

}

HI_S32 HI_RTSPCLIENT_Start(HI_MW_PTR hClientHdl)
{
    HI_S32 s32Ret = HI_SUCCESS;
    CHECK_INVALID_HANDLE(hClientHdl);
    if (NULL == s_pstClientNode)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT not created\n");
        return HI_FAILURE;
    }

    /*send first request and wait for response callback*/
    s32Ret = HI_RTSPCLIENT_Stream_Send_Option(hClientHdl);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_Send_Option failed\n");
        return s32Ret;
    }

    return HI_SUCCESS;

}


HI_S32 HI_RTSPCLIENT_Stop(HI_MW_PTR hClientHdl)
{
    HI_S32 s32Ret = HI_SUCCESS;
    CHECK_INVALID_HANDLE(hClientHdl);

    if (NULL != s_pstClientNode)
    {
        (HI_VOID)pthread_mutex_lock(&(s_pstClientNode->mSendMutex));
        s_pstClientNode->enMethod = HI_RTSPCLIENT_METHOD_TEARDOWN;
        (HI_VOID)pthread_cond_signal(&(s_pstClientNode->mSendCond));
        (HI_VOID)pthread_mutex_unlock(&(s_pstClientNode->mSendMutex));
    }

    s32Ret = HI_RTSPCLIENT_Stream_Send_Teardown(hClientHdl);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_Send_Teardown failed\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;

}

HI_S32 HI_RTSPCLIENT_Create(HI_MW_PTR* phClientHdl, HI_RTSPCLIENT_CONFIG_S* pstConfig)
{
    HI_S32 s32Ret = HI_SUCCESS;
    CHECK_NULL_ERROR(phClientHdl);
    CHECK_NULL_ERROR(pstConfig);
    HI_MW_PTR hClientHdl = HI_NULL;
    if (NULL == s_pstClientNode)
    {
        s_pstClientNode = (HI_RTSPCLIENT_NODE_S*)malloc(sizeof(HI_RTSPCLIENT_NODE_S));
        if (!s_pstClientNode)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "create g_pstClientNode malloc failed\n");
            return HI_FAILURE;
        }
        memset(s_pstClientNode, 0x00, sizeof(HI_RTSPCLIENT_NODE_S));
    }

    /*init mutex and cond var */
    if (HI_SUCCESS != pthread_mutex_init(&s_pstClientNode->mSendMutex, NULL))
    {
        free(s_pstClientNode);
        s_pstClientNode = NULL;
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "Fail to init space monitor mutex  \n");
        return HI_FAILURE;
    }

    if (HI_SUCCESS != pthread_cond_init(&s_pstClientNode->mSendCond, NULL))
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "Fail to init condition var  \n");
        pthread_mutex_destroy(&s_pstClientNode->mSendMutex);
        free(s_pstClientNode);
        s_pstClientNode = NULL;
        return HI_FAILURE;
    }

    s_pstClientNode->enMethod = HI_RTSPCLIENT_METHOD_BUTT;
    s_pstClientNode->enStreamType = pstConfig->enStreamType;
    s_pstClientNode->bNeedSetupAudio = HI_FALSE;
    s_pstClientNode->bNeedSetupVideo = HI_FALSE;

    s32Ret = HI_RTSPCLIENT_Stream_Create(&hClientHdl, pstConfig);
    if (HI_SUCCESS != s32Ret)
    {
        pthread_mutex_destroy(&s_pstClientNode->mSendMutex);
        pthread_cond_destroy(&(s_pstClientNode->mSendCond));
        free(s_pstClientNode);
        s_pstClientNode = NULL;
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_Create failed\n");
        return s32Ret;
    }
    s_pstClientNode->hClientHdl = hClientHdl;

    s32Ret = pthread_create(&(s_pstClientNode->ptSendThdId), NULL, RTSPCLIENT_SendProc, (HI_VOID*)s_pstClientNode);
    if (s32Ret !=  HI_SUCCESS)
    {
        s_pstClientNode->ptSendThdId = RTSPCLIENT_INVALID_THD_ID;
        pthread_mutex_destroy(&s_pstClientNode->mSendMutex);
        pthread_cond_destroy(&(s_pstClientNode->mSendCond));
        free(s_pstClientNode);
        s_pstClientNode = NULL;
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "pthread_create error %d\r\n", s32Ret);
        return HI_FAILURE;
    }

    *phClientHdl = hClientHdl;

    return HI_SUCCESS;
}

HI_S32 HI_RTSPCLIENT_Destroy(HI_MW_PTR hClientHdl)
{
    HI_S32 s32Ret = HI_SUCCESS;
    CHECK_INVALID_HANDLE(hClientHdl);

    s32Ret = HI_RTSPCLIENT_Stream_Destroy(hClientHdl);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_Destroy failed\n");
    }

    if (NULL != s_pstClientNode)
    {
        if (RTSPCLIENT_INVALID_THD_ID != s_pstClientNode->ptSendThdId)
        {
            if (HI_SUCCESS != pthread_join(s_pstClientNode->ptSendThdId, NULL))
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "join stream recv pthread fail \n");
            }

            s_pstClientNode->ptSendThdId = RTSPCLIENT_INVALID_THD_ID;
        }
        pthread_cond_destroy(&(s_pstClientNode->mSendCond));
        pthread_mutex_destroy(&(s_pstClientNode->mSendMutex));
        free(s_pstClientNode);
        s_pstClientNode = NULL;
    }

    return s32Ret;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
