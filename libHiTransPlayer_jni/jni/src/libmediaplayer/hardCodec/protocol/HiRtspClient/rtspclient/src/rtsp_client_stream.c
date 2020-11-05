/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      rtsp_client_stream.c
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

#define hi_usleep(usec) \
do { \
    struct timespec req; \
    req.tv_sec  = (usec) / 1000000; \
    req.tv_nsec = ((usec) % 1000000) * 1000; \
    nanosleep (&req, NULL); \
} while (0)

#define SECONDS_BEFORE_TIMEOUT (5)

static HI_VOID RTSPCLIENT_SetRTP(HI_RTP_SESSION_S* pstRtpSession, HI_RTSPCLIENT_STREAM_S* pstSession)
{
    pstSession->pstRtpVideo->enTransType = pstSession->enTransType;
    pstRtpSession->s32SessSock = pstSession->s32SessSock;
    pstRtpSession->enStreamPackType = pstSession->enPackType;
    pstRtpSession->pfnRecvVideo = pstSession->pfnOnRecvVideo;
    pstRtpSession->pfnRecvAudio = pstSession->pfnOnRecvAudio;
    pstRtpSession->ps32ExtArgs = pstSession->ps32PrivData;
    memcpy(&pstRtpSession->stDescribeInfo, &pstSession->stDescribeInfo, sizeof(HI_RTSP_DESCRIBE_S));
    memcpy(&pstRtpSession->stStreamInfo, &pstSession->stStreamInfo, sizeof(HI_LIVE_STREAM_INFO_S));
    strncpy(pstRtpSession->szURL , pstSession->stProtoMsgInfo.szUrl, RTSPCLIENT_MAXURL_LEN - 1);
    pstRtpSession->szURL[RTSPCLIENT_MAXURL_LEN - 1] = '\0';
    return;
}

static HI_VOID RTSPCLIENT_ParseRTP(HI_RTSPCLIENT_STREAM_S* pstSession)
{

    if ( HI_TRUE == pstSession->bVideo )
    {
        if (HI_LIVE_TRANS_TYPE_TCP ==  pstSession->enTransType )
        {
            pstSession->pstRtpVideo->s32StreamSock = pstSession->s32SessSock;
        }
        RTSPCLIENT_SetRTP(pstSession->pstRtpVideo, pstSession);
    }

    if ( HI_TRUE == pstSession->bAudio )
    {
        if (HI_LIVE_TRANS_TYPE_TCP ==  pstSession->enTransType )
        {
            pstSession->pstRtpAudio->s32StreamSock = pstSession->s32SessSock;
        }
        RTSPCLIENT_SetRTP(pstSession->pstRtpAudio, pstSession);
    }

    return;
}
static HI_VOID RTSPCLIENT_ClearInBuff(HI_RTSPCLIENT_STREAM_S* pstSess)
{
    memset(pstSess->stProtoMsgInfo.szInBuffer, 0, RTSPCLIENT_MAXBUFFER_SIZE);
    pstSess->stProtoMsgInfo.u32InSize = 0;
    return;
}



static HI_S32 RTSPCLIENT_GetAudioInfo(HI_CHAR* pchArgValue, HI_LIVE_STREAM_INFO_S* pstStreamInfo)
{
    HI_S32 s32ConfigNum = atoi(pchArgValue);
    switch (s32ConfigNum)
    {
        case HI_RTSPCLIENT_AUDIO_CONFIGNUM_8_SINGLECHN:
            pstStreamInfo->stAencChAttr.u32SampleRate = HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_8;
            pstStreamInfo->stAencChAttr.u32Channel = RTSPCLIENT_AUDIO_CHANNEL_SINGLE;
            break;

        case HI_RTSPCLIENT_AUDIO_CONFIGNUM_16_SINGLECHN:
            pstStreamInfo->stAencChAttr.u32SampleRate = HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_16;
            pstStreamInfo->stAencChAttr.u32Channel = RTSPCLIENT_AUDIO_CHANNEL_SINGLE;
            break;

        case HI_RTSPCLIENT_AUDIO_CONFIGNUM_24_SINGLECHN:
            pstStreamInfo->stAencChAttr.u32SampleRate = HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_24;
            pstStreamInfo->stAencChAttr.u32Channel = RTSPCLIENT_AUDIO_CHANNEL_SINGLE;
            break;

        case HI_RTSPCLIENT_AUDIO_CONFIGNUM_32_SINGLECHN:
            pstStreamInfo->stAencChAttr.u32SampleRate = HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_32;
            pstStreamInfo->stAencChAttr.u32Channel = RTSPCLIENT_AUDIO_CHANNEL_SINGLE;
            break;

        case HI_RTSPCLIENT_AUDIO_CONFIGNUM_48_SINGLECHN:
            pstStreamInfo->stAencChAttr.u32SampleRate = HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_48;
            pstStreamInfo->stAencChAttr.u32Channel = RTSPCLIENT_AUDIO_CHANNEL_SINGLE;
            break;

        case HI_RTSPCLIENT_AUDIO_CONFIGNUM_441_SINGLECHN:
            pstStreamInfo->stAencChAttr.u32SampleRate = HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_441;
            pstStreamInfo->stAencChAttr.u32Channel = RTSPCLIENT_AUDIO_CHANNEL_SINGLE;
            break;

        case HI_RTSPCLIENT_AUDIO_CONFIGNUM_22050_SINGLECHN:
            pstStreamInfo->stAencChAttr.u32SampleRate = HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_22050;
            pstStreamInfo->stAencChAttr.u32Channel = RTSPCLIENT_AUDIO_CHANNEL_SINGLE;
            break;

        case HI_RTSPCLIENT_AUDIO_CONFIGNUM_8_DOUBLECHN:
            pstStreamInfo->stAencChAttr.u32SampleRate = HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_8;
            pstStreamInfo->stAencChAttr.u32Channel = RTSPCLIENT_AUDIO_CHANNEL_DOUBLE;
            break;

        case HI_RTSPCLIENT_AUDIO_CONFIGNUM_16_DOUBLECHN:
            pstStreamInfo->stAencChAttr.u32SampleRate = HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_16;
            pstStreamInfo->stAencChAttr.u32Channel = RTSPCLIENT_AUDIO_CHANNEL_DOUBLE;
            break;

        case HI_RTSPCLIENT_AUDIO_CONFIGNUM_24_DOUBLECHN:
            pstStreamInfo->stAencChAttr.u32SampleRate = HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_24;
            pstStreamInfo->stAencChAttr.u32Channel = RTSPCLIENT_AUDIO_CHANNEL_DOUBLE;
            break;

        case HI_RTSPCLIENT_AUDIO_CONFIGNUM_32_DOUBLECHN:
            pstStreamInfo->stAencChAttr.u32SampleRate = HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_32;
            pstStreamInfo->stAencChAttr.u32Channel = RTSPCLIENT_AUDIO_CHANNEL_DOUBLE;
            break;

        case HI_RTSPCLIENT_AUDIO_CONFIGNUM_48_DOUBLECHN:
            pstStreamInfo->stAencChAttr.u32SampleRate = HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_48;
            pstStreamInfo->stAencChAttr.u32Channel = RTSPCLIENT_AUDIO_CHANNEL_DOUBLE;
            break;

        case HI_RTSPCLIENT_AUDIO_CONFIGNUM_22050_DOUBLECHN:
            pstStreamInfo->stAencChAttr.u32SampleRate = HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_22050;
            pstStreamInfo->stAencChAttr.u32Channel = RTSPCLIENT_AUDIO_CHANNEL_DOUBLE;
            break;

        case HI_RTSPCLIENT_AUDIO_CONFIGNUM_441_DOUBLECHN:
            pstStreamInfo->stAencChAttr.u32SampleRate = HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_441;
            pstStreamInfo->stAencChAttr.u32Channel = RTSPCLIENT_AUDIO_CHANNEL_DOUBLE;
            break;

        default:
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "unsupported audio CONFIGNUM!\n");
            return HI_FAILURE;
            break;
    }

    return HI_SUCCESS;
}

static HI_S32 RTSPCLIENT_GetAudioType(HI_RTSPCLIENT_STREAM_S* pstSession, HI_CHAR* pchArgValue, HI_LIVE_STREAM_INFO_S* pstStreamInfo)
{
    pstSession->stDescribeInfo.as32PayloadType[RTSPCLIENT_STREAM_AUDIO] = atoi(pchArgValue);
    if (0 == pstSession->stDescribeInfo.as32PayloadType[RTSPCLIENT_STREAM_AUDIO] )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "get audio payload type failed!\n");
        return HI_FAILURE;
    }
    pstStreamInfo->bAudio = HI_TRUE;

    if (NULL != strstr(pchArgValue, "G726"))
    {
        pstSession->stDescribeInfo.enAudioFormat = HI_LIVE_AUDIO_G726;
        pstStreamInfo->stAencChAttr.enAudioFormat = HI_LIVE_AUDIO_G726;

        if (NULL != strstr(pchArgValue, "G726-16"))
        {
            pstStreamInfo->stAencChAttr.u32BitRate = 16;
        }
        else if (NULL != strstr(pchArgValue, "G726-24"))
        {
            pstStreamInfo->stAencChAttr.u32BitRate = 24;
        }
        else if (NULL != strstr(pchArgValue, "G726-40"))
        {
            pstStreamInfo->stAencChAttr.u32BitRate = 40;
        }
        else
        {
            pstStreamInfo->stAencChAttr.u32BitRate = 32;
        }
    }
    else if (NULL != strstr(pchArgValue, "PCMA"))
    {
        pstSession->stDescribeInfo.enAudioFormat = HI_LIVE_AUDIO_G711A;
        pstStreamInfo->stAencChAttr.enAudioFormat = HI_LIVE_AUDIO_G711A;
    }
    else if (NULL != strstr(pchArgValue, "PCMU"))
    {
        pstSession->stDescribeInfo.enAudioFormat = HI_LIVE_AUDIO_G711Mu;
        pstStreamInfo->stAencChAttr.enAudioFormat = HI_LIVE_AUDIO_G711Mu;
    }
    else if (NULL != strstr(pchArgValue, "AAC"))
    {
        pstSession->stDescribeInfo.enAudioFormat = HI_LIVE_AUDIO_AAC;
        pstStreamInfo->stAencChAttr.enAudioFormat = HI_LIVE_AUDIO_AAC;
    }
    else
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "unsupported audio format!\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

static HI_S32 RTSPCLIENT_GetVideoType(HI_RTSPCLIENT_STREAM_S* pstSession, HI_CHAR* pchArgValue, HI_LIVE_STREAM_INFO_S* pstStreamInfo)
{
    pstSession->stDescribeInfo.as32PayloadType[RTSPCLIENT_STREAM_VIDEO] = atoi(pchArgValue);
    if (0 == pstSession->stDescribeInfo.as32PayloadType[RTSPCLIENT_STREAM_VIDEO])
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "get video payload type failed!\n");
        return HI_FAILURE;
    }
    pstStreamInfo->bVideo = HI_TRUE;

    if (NULL != strstr(pchArgValue, "H264"))
    {
        pstSession->stDescribeInfo.enVideoFormat = HI_LIVE_VIDEO_H264;
        pstStreamInfo->stVencChAttr.enVideoFormat = HI_LIVE_VIDEO_H264;
    }
    else if (NULL != strstr(pchArgValue, "H265"))
    {
        pstSession->stDescribeInfo.enVideoFormat = HI_LIVE_VIDEO_H265;
        pstStreamInfo->stVencChAttr.enVideoFormat = HI_LIVE_VIDEO_H265;
    }
    else if (NULL != strstr(pchArgValue, "JPEG"))
    {
        pstSession->stDescribeInfo.enVideoFormat = HI_LIVE_VIDEO_JPEG;
        pstStreamInfo->stVencChAttr.enVideoFormat = HI_LIVE_VIDEO_JPEG;
    }
    else
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "unsupported video format!\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;

}

static HI_VOID RTSPCLIENT_SetClientState(HI_RTSPCLIENT_STREAM_S* pstSess, HI_RTSPCLIENT_STATE_E enClientState)
{
    pstSess->enClientState = enClientState;
}

static HI_S32 RTSPCLIENT_AACHeaderAnalyze(HI_RTP_SESSION_S* pstSession, HI_U8** ppu8Buff, HI_U32* pu32Len)
{
    HI_U8 u8ADTS[RTSP_CLIENT_ADTS_HEAD_LENGTH] = {0xFF, 0xF1, 0x00, 0x00, 0x00, 0x00, 0xFC};
    HI_U32 u32AudioSamprate = pstSession->stStreamInfo.stAencChAttr.u32SampleRate;
    HI_U32 u32AudioChannel = pstSession->stStreamInfo.stAencChAttr.u32Channel;
    HI_U32 u32AULen = 0;
    if (NULL == ppu8Buff || NULL == *ppu8Buff  || NULL == pu32Len)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_AACHeaderAnalyze param null!\n");
        return HI_FAILURE;
    }
    if (*pu32Len < RTSP_CLIENT_ADTS_HEAD_LENGTH)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_AACHeaderAnalyze datalen too short!\n");
        return HI_FAILURE;
    }
    switch (u32AudioSamprate)
    {
        case  HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_16:
            u8ADTS[2] = 0x60;
            break;
        case  HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_32:
            u8ADTS[2] = 0x54;
            break;
        case  HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_441:
            u8ADTS[2] = 0x50;
            break;
        case  HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_48:
            u8ADTS[2] = 0x4C;
            break;
        case  HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_96:
            u8ADTS[2] = 0x40;
            break;
        default:
            break;
    }
    u8ADTS[3] = ( RTSPCLIENT_AUDIO_CHANNEL_DOUBLE == u32AudioChannel ) ? 0x80 : 0x40;

    u32AULen = *pu32Len - RTSP_CLIENT_AAC_HEAD_LENGTH + RTSP_CLIENT_ADTS_HEAD_LENGTH;
    u32AULen <<= 5;/*8bit * 2 - 11 = 5(headerSize 11bit)*/
    u32AULen |= 0x1F;/*5 bit    1*/
    u8ADTS[4] = u32AULen >> 8;
    u8ADTS[5] = u32AULen & 0xFF;
    *ppu8Buff = *ppu8Buff + RTSP_CLIENT_AAC_HEAD_LENGTH - RTSP_CLIENT_ADTS_HEAD_LENGTH;
    memcpy(*ppu8Buff, u8ADTS, RTSP_CLIENT_ADTS_HEAD_LENGTH);
    *pu32Len = *pu32Len - RTSP_CLIENT_AAC_HEAD_LENGTH + RTSP_CLIENT_ADTS_HEAD_LENGTH;

    return HI_SUCCESS;

}

static HI_S32 RTSPCLIENT_GetVideoTrackID(HI_RTSPCLIENT_STREAM_S* pLiveSess, HI_CHAR* pchTrackUrl)
{
    HI_CHAR* pszMedia = NULL;
    HI_CHAR* pszControl = NULL;
    HI_CHAR* pszTrackID = NULL;
    HI_CHAR* pszTrackUrl = NULL;
    HI_CHAR  aszTrackID[RTSPCLIENT_TRACKID_LEN] = {0};

    if (NULL == pLiveSess || NULL == pchTrackUrl)
    {
        return HI_FAILURE;
    }

    pszMedia = strstr(pLiveSess->stProtoMsgInfo.szInBuffer, "m=video");
    if (NULL == pszMedia)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "bad pszControl trackUrl do not have video\n");
        return HI_FAILURE;
    }

    pszControl = strstr(pszMedia, "a=control");
    if (NULL == pszControl)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "bad pszControl trackUrl do not have control\n");
        return HI_FAILURE;
    }

    pszTrackUrl = strstr(pszControl, "rtsp://");
    if (NULL != pszTrackUrl)
    {
        if (RTSPCLIENT_SCANF_RET_ONE != sscanf(pszControl, "a=control:%255s\r\n", pchTrackUrl))
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "bad pszControl trackUrl\n");
            return HI_FAILURE;
        }
        pchTrackUrl[RTSPCLIENT_MAXURL_LEN-1] = '\0';

        return HI_SUCCESS;
    }

    pszTrackID = strstr(pszControl, "trackID=");
    if (NULL == pszTrackID)
    {
        if (RTSPCLIENT_SCANF_RET_ONE != sscanf(pszControl, "a=control:%127s ", aszTrackID))
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "bad pszControl trackID\n");
            return HI_FAILURE;
        }
        aszTrackID[RTSPCLIENT_TRACKID_LEN-1] = '\0';
        snprintf(pchTrackUrl, RTSPCLIENT_MAXURL_LEN, "%s/%s", pLiveSess->stProtoMsgInfo.szUrl, aszTrackID);
        return HI_SUCCESS;
    }
    else
    {
        if (RTSPCLIENT_SCANF_RET_ONE != sscanf(pszTrackID, "trackID=%127s ", aszTrackID))
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "bad pszControl trackID\n");
            return HI_FAILURE;
        }
        aszTrackID[RTSPCLIENT_TRACKID_LEN-1] = '\0';

        snprintf(pchTrackUrl, RTSPCLIENT_MAXURL_LEN, "%s/trackID=%s", pLiveSess->stProtoMsgInfo.szUrl, aszTrackID);
        return HI_SUCCESS;
    }

    return HI_FAILURE;
}

static HI_S32 RTSPCLIENT_GetAudioTrackID(HI_RTSPCLIENT_STREAM_S* pLiveSess, HI_CHAR* pchTrackUrl)
{
    HI_CHAR* pszMedia = NULL;
    HI_CHAR* pszControl = NULL;
    HI_CHAR* pszTrackID = NULL;
    HI_CHAR* pszTrackUrl = NULL;
    HI_CHAR  aszTrackID[RTSPCLIENT_TRACKID_LEN] = {0};

    if (NULL == pLiveSess || NULL == pchTrackUrl)
    {
        return HI_FAILURE;
    }

    pszMedia = strstr(pLiveSess->stProtoMsgInfo.szInBuffer, "m=audio");
    if (NULL == pszMedia)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "bad pszControl trackUrl can't find audio\n");
        return HI_FAILURE;
    }

    pszControl = strstr(pszMedia, "a=control");
    if (NULL == pszControl)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "bad pszControl trackUrl can't find control\n");
        return HI_FAILURE;
    }

    pszTrackUrl = strstr(pszControl, "rtsp://");
    if (NULL != pszTrackUrl)
    {
        if (RTSPCLIENT_SCANF_RET_ONE != sscanf(pszControl, "a=control:%255s\r\n", pchTrackUrl))
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "bad pszControl trackUrl\n");
            return HI_FAILURE;
        }
        pchTrackUrl[RTSPCLIENT_MAXURL_LEN-1]='\0';

        return HI_SUCCESS;
    }


    pszTrackID = strstr(pszControl, "trackID=");
    if (NULL == pszTrackID)
    {
        if (RTSPCLIENT_SCANF_RET_ONE != sscanf(pszControl, "a=control:%127s ", aszTrackID))
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "bad pszControl trackID\n");
            return HI_FAILURE;
        }
        aszTrackID[RTSPCLIENT_TRACKID_LEN-1] = '\0';

        snprintf(pchTrackUrl, RTSPCLIENT_MAXURL_LEN, "%s/%s", pLiveSess->stProtoMsgInfo.szUrl, aszTrackID);
        return HI_SUCCESS;
    }
    else
    {
        if (RTSPCLIENT_SCANF_RET_ONE != sscanf(pszTrackID, "trackID=%127s ", aszTrackID))
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "bad pszControl trackID\n");
            return HI_FAILURE;
        }
        aszTrackID[RTSPCLIENT_TRACKID_LEN-1] = '\0';

        snprintf(pchTrackUrl, RTSPCLIENT_MAXURL_LEN, "%s/trackID=%s", pLiveSess->stProtoMsgInfo.szUrl, aszTrackID);
        return HI_SUCCESS;
    }

    return HI_FAILURE;
}
static HI_S32 RTSPCLIENT_RespDesVideo(HI_RTSPCLIENT_STREAM_S* pstSession)
{
    HI_S32 s32Ret = 0;
    pstSession->bVideo = HI_TRUE;
    s32Ret = RTP_Session_Create(&pstSession->pstRtpVideo);
    if (HI_SUCCESS != s32Ret)
    {
        pstSession->bVideo  = HI_FALSE;
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTP_Session_Create VIDEO fail\n");
        return s32Ret;
    }

    s32Ret = RTCP_Session_Create(&pstSession->pstRtcpVideo);
    if (HI_SUCCESS != s32Ret)
    {
        pstSession->bVideo  = HI_FALSE;
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTCP_Session_Create VIDEO fail\n");
        RTP_Session_Destroy(pstSession->pstRtpVideo);
        return s32Ret;
    }

    s32Ret = RTSPCLIENT_GetVideoTrackID(pstSession, pstSession->stProtoMsgInfo.szVideoTrackID);
    if (HI_SUCCESS != s32Ret)
    {
        pstSession->bVideo  = HI_FALSE;
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "SDP_GetTrackID VIDEO fail\n");
        RTCP_Session_Destroy(pstSession->pstRtcpVideo);
        RTP_Session_Destroy(pstSession->pstRtpVideo);
        return s32Ret;
    }
    return HI_SUCCESS;
}

static HI_S32 RTSPCLIENT_RespDesAudio(HI_RTSPCLIENT_STREAM_S* pstSession)
{
    HI_S32 s32Ret = 0;
    pstSession->bAudio = HI_TRUE;
    s32Ret = RTP_Session_Create(&pstSession->pstRtpAudio);
    if (HI_SUCCESS != s32Ret)
    {
        pstSession->bAudio  = HI_FALSE;
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTP_Session_Create AUDIO fail\n");
        return s32Ret;
    }
    s32Ret = RTCP_Session_Create(&pstSession->pstRtcpAudio);
    if (HI_SUCCESS != s32Ret)
    {
        pstSession->bAudio  = HI_FALSE;
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTCP_Session_Create AUDIO fail\n");
        RTP_Session_Destroy(pstSession->pstRtpAudio);
        return s32Ret;
    }

    s32Ret = RTSPCLIENT_GetAudioTrackID(pstSession, pstSession->stProtoMsgInfo.szAudioTrackID);
    if (HI_SUCCESS != s32Ret)
    {
        pstSession->bAudio  = HI_FALSE;
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "SDP_GetTrackID Audio fail\n");
        RTCP_Session_Destroy(pstSession->pstRtcpAudio);
        RTP_Session_Destroy(pstSession->pstRtpAudio);
        return s32Ret;
    }
    return HI_SUCCESS;

}

static HI_S32 RTSPCLIENT_Stream_OpenTcp(struct sockaddr*  pstSockAddr, HI_S32 s32NameLen)
{
    HI_S32 s32SockFd = RTSPCLIENT_INVALID_SOCKET;
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32Flag = 1;
    HI_S32 s32Error = 0;
    HI_S32 s32Len = sizeof(HI_S32);
    HI_S32 s32WaitCount = 0;
    struct timeval tm;
    fd_set set;

    if ((s32SockFd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_SOCKET_OpenTcp socket error  \n");
        return HI_FAILURE;
    }

    if ( 0 != ioctl(s32SockFd, FIONBIO, &u32Flag)) /*set socket to non-blocked*/
    {
        HI_CLOSE_SOCKET(s32SockFd);
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_SOCKET_OpenTcp set socket to non-blocked error  \n");
        return HI_FAILURE;
    }

    s32Ret = connect(s32SockFd, pstSockAddr, s32NameLen);

    if (s32Ret  < 0)
    {

        if (errno != EINPROGRESS)
        {
            HI_CLOSE_SOCKET(s32SockFd);
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_SOCKET_OpenTcp connect error  \n");
            return HI_FAILURE;
        }

        do
        {

            tm.tv_sec = 0;
            tm.tv_usec = RTSPCLIENT_TRANS_TIMEVAL_USEC;
            FD_ZERO(&set);
            FD_SET(s32SockFd, &set);
            s32Ret = select(s32SockFd + 1, NULL, &set, NULL, &tm);
            /*if time out */
            if ( 0 == s32Ret )
            {
                s32WaitCount++;
                if (s32WaitCount < RTSPCLIENT_MAX_WAIT_COUNT)
                {
                    continue;
                }
                else
                {
                    /*if time out for 10 times (5 s in sum) return error*/
                    HI_CLOSE_SOCKET(s32SockFd);
                    errno = ETIMEDOUT;
                    RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_SOCKET_OpenTcp select timeout  \n");
                    return HI_FAILURE;
                }
            }
            else if (s32Ret < 0)
            {
                if (EINTR == errno || EAGAIN == errno)
                {
                    RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "  [select err: %s]\n",  strerror(errno));
                    continue;
                }
                HI_CLOSE_SOCKET(s32SockFd);
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_SOCKET_OpenTcp select error  \n");
                return HI_FAILURE;
            }
            else
            {
                break;
            }

        }
        while (1);

        if (FD_ISSET(s32SockFd, &set))
        {
            getsockopt(s32SockFd, SOL_SOCKET, SO_ERROR, &s32Error, (socklen_t*)&s32Len);

            if (s32Error != 0)
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "getsockopt error %d \n", s32Error);
                HI_CLOSE_SOCKET(s32SockFd);
                return HI_FAILURE;
            }
        }
        else
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "select err:sockfd not set \n");
            HI_CLOSE_SOCKET(s32SockFd);
            return HI_FAILURE;
        }

    }

    u32Flag = 0;
    if (0 != ioctl(s32SockFd, FIONBIO, &u32Flag)) /*set socket to blocked*/
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "ioctl set block failed \n");
        HI_CLOSE_SOCKET(s32SockFd);
        return HI_FAILURE;
    }
    return s32SockFd;
}

static HI_S32 RTSPCLIENT_Stream_RecvMsg(HI_RTSPCLIENT_STREAM_S* pstSession, HI_U8* aszRecvBuff )
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U8*  pLinehead = NULL;
    HI_U32  remBuffLen = 0;
    HI_U8*   pAddr = NULL;
    HI_U32   LineLen = 0;

    pAddr = aszRecvBuff + 4;
    remBuffLen = RTSPCLIENT_MAXBUFFER_SIZE - 4;

    do
    {
        s32Ret = RTSPCLIENT_SOCKET_ReadLine(pstSession->s32SessSock, pAddr, remBuffLen, &LineLen);
        if (HI_SUCCESS != s32Ret)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_Stream_RecvMsg withdraw recv thread %llu ret:%d .\n", (HI_U64)pstSession->ptSessThdId, s32Ret);
            return HI_FAILURE;
        }

        pLinehead = pAddr;
        pAddr += LineLen;
        remBuffLen -= LineLen;
    }
    while ( !(('\n' == pLinehead[1] ) && ( '\r' == pLinehead[0] )));
    memset(aszRecvBuff, 0, RTSPCLIENT_MAXBUFFER_SIZE);
    return HI_SUCCESS;

}

/*process the recvied stream and send to player callbak func*/
static HI_S32 RTSPCLIENT_Stream_ProcStream(HI_RTP_SESSION_S* pstSession, HI_U8* pBuff,
        HI_U32 u32PackLen, HI_LIVE_STREAM_TYPE_E enStreamType)
{
    HI_S32 s32Ret = 0;
    HI_U32 u32HeadLen = 0;          /*header length*/
    HI_LIVE_RTP_HEAD_INFO_S stRtpHeadInfo;

    if (NULL == pBuff || NULL == pstSession)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "~~Unknow  HI_PACK_TYPE_RTP StreamType ~~\n");
        return HI_FAILURE;
    }

    memset(&stRtpHeadInfo, 0, sizeof(HI_LIVE_RTP_HEAD_INFO_S));

    if (HI_PACK_TYPE_RTSP_ITLV == pstSession->enStreamPackType )
    {
        s32Ret = RTP_Session_GetHeadInfo_RTSPITLV(pstSession, pBuff, enStreamType, &stRtpHeadInfo);
        if (HI_SUCCESS != s32Ret)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "~  RTP_Session_GetHeadInfo_RTSPITLV  failed%d ~~\n", s32Ret);
            return HI_FAILURE;
        }
    }
    else if ( HI_PACK_TYPE_RTP ==  pstSession->enStreamPackType )
    {
        s32Ret = RTP_Session_GetHeadInfo_RTP(pstSession, pBuff, enStreamType, &stRtpHeadInfo);
        if (HI_SUCCESS != s32Ret)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "~  RTP_Session_GetHeadInfo_RTSPITLV  failed %d~~\n", s32Ret);
            return HI_FAILURE;
        }
    }
    else
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "~~Unknow  pack type%d ~~\n", pstSession->enStreamPackType);
        return HI_FAILURE;
    }

    u32HeadLen = stRtpHeadInfo.u32HeadLen;

    if (HI_LIVE_STREAM_TYPE_VIDEO == enStreamType)
    {
        if (NULL != pstSession->pfnRecvVideo)
        {
            pstSession->pfnRecvVideo((HI_U8*)pBuff + u32HeadLen, u32PackLen - u32HeadLen,
                                     &stRtpHeadInfo, pstSession->ps32ExtArgs);
        }
    }
    else if (HI_LIVE_STREAM_TYPE_AUDIO == enStreamType)
    {
        pBuff = (HI_U8*)pBuff + u32HeadLen;
        u32PackLen = u32PackLen - u32HeadLen;
        if (HI_LIVE_AUDIO_AAC == pstSession->stStreamInfo.stAencChAttr.enAudioFormat)
        {
            RTSPCLIENT_AACHeaderAnalyze(pstSession, &pBuff, &u32PackLen);
        }
        if (NULL != pstSession->pfnRecvAudio)
        {
            pstSession->pfnRecvAudio((HI_U8*)pBuff , u32PackLen,
                                     &stRtpHeadInfo, pstSession->ps32ExtArgs);
        }
    }
    else
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "~~Unknow   StreamType ~~\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;

}

static HI_S32 RTSPCLIENT_Stream_Parse_Transport(HI_RTSPCLIENT_STREAM_S* pstSession, HI_LIVE_STREAM_INFO_S* pstStreamInfo)
{
    HI_CHAR* pszArgValue = NULL;
    HI_CHAR* pszMedia = NULL;
    HI_CHAR aszBase64Sps[RTSPCLIENT_MAX_SPS_LEN] = {0};
    HI_CHAR aszBase64Pps[RTSPCLIENT_MAX_PPS_LEN] = {0};
    HI_U32 u32Base64SpsLen = 0;
    HI_U32 u32Base64PpsLen = 0;

    if (NULL == pstSession || NULL == pstStreamInfo)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "Parse_Describe  null param\n");
        return HI_FAILURE;
    }

    pstSession->stDescribeInfo.as32PayloadType[RTSPCLIENT_STREAM_VIDEO] = -1;
    pstSession->stDescribeInfo.as32PayloadType[RTSPCLIENT_STREAM_AUDIO] = -1;
    pstSession->stDescribeInfo.as32PayloadType[RTSPCLIENT_STREAM_DATA]  = -1;
    pszMedia = strstr(pstSession->stProtoMsgInfo.szInBuffer, "m=video");

    if (NULL != pszMedia)
    {
        pstSession->stDescribeInfo.as32RtpChn[RTSPCLIENT_STREAM_VIDEO] = 0;
        pszArgValue = RTSPCLIENT_MSGParser_SDP_GetPos(pszMedia, (HI_CHAR*)"a=rtpmap:");
        if (NULL != pszArgValue)
        {
            if (HI_SUCCESS != RTSPCLIENT_GetVideoType(pstSession, pszArgValue, pstStreamInfo))
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_GetVideoType fail !\n");
                return HI_FAILURE;
            }
        }
        else
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "unknow video format!\n");
            if (RTSPCLIENT_SCANF_RET_ONE != sscanf(pszMedia, "%*[^ ] %*[^ ] %*[^ ] %d[^\r\n]", &pstSession->stDescribeInfo.as32PayloadType[RTSPCLIENT_STREAM_VIDEO]))
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "can;t parse video format!\n");
                return HI_FAILURE;
            }
        }

        pszArgValue = RTSPCLIENT_MSGParser_SDP_GetPos(pszMedia, (HI_CHAR*)"a=framerate:");

        if (NULL != pszArgValue)
        {
            pstStreamInfo->stVencChAttr.u32FrameRate = atoi(pszArgValue);
        }

        pszArgValue = RTSPCLIENT_MSGParser_SDP_GetPos(pszMedia, (HI_CHAR*)"a=x-dimensions:");

        if (NULL != pszArgValue)
        {
            if (RTSPCLIENT_SCANF_RET_TWO != sscanf(pszArgValue, "%d,%d", &(pstStreamInfo->stVencChAttr.u32PicWidth), &(pstStreamInfo->stVencChAttr.u32PicHeight)))
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "can't parse video pic height and width!\n");
                return HI_FAILURE;
            }
        }

        pszArgValue = strstr(pszMedia, (HI_CHAR*)"sprop-parameter-sets");

        if (NULL != pszArgValue)
        {
            memset(pstStreamInfo->stVencChAttr.au8Sps, 0, sizeof(pstStreamInfo->stVencChAttr.au8Sps));
            memset(pstStreamInfo->stVencChAttr.au8Pps, 0, sizeof(pstStreamInfo->stVencChAttr.au8Pps));
            if (HI_SUCCESS != RTSPCLIENT_MSGParser_GetSPSPPS(pszArgValue, aszBase64Sps, &u32Base64SpsLen, aszBase64Pps, &u32Base64PpsLen))
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "can't parse SPS PPS info!\n");
                return HI_FAILURE;
            }
            pstStreamInfo->stVencChAttr.u32SpsLen = HI_RTSPCLIENT_Base64Decode((const unsigned char*)aszBase64Sps,
                                                    u32Base64SpsLen,
                                                    pstStreamInfo->stVencChAttr.au8Sps,
                                                    sizeof(pstStreamInfo->stVencChAttr.au8Sps));

            pstStreamInfo->stVencChAttr.u32PpsLen = HI_RTSPCLIENT_Base64Decode((const unsigned char*)aszBase64Pps,
                                                    u32Base64PpsLen,
                                                    pstStreamInfo->stVencChAttr.au8Pps,
                                                    sizeof(pstStreamInfo->stVencChAttr.au8Pps));
        }
    }

    pszMedia = strstr(pstSession->stProtoMsgInfo.szInBuffer, "m=audio");

    if (NULL != pszMedia)
    {
        pstSession->stDescribeInfo.as32RtpChn[RTSPCLIENT_STREAM_AUDIO] = 2;
        pszArgValue = RTSPCLIENT_MSGParser_SDP_GetPos(pszMedia, (HI_CHAR*)"a=rtpmap:");
        if (NULL != pszArgValue)
        {
            if (HI_SUCCESS != RTSPCLIENT_GetAudioType(pstSession, pszArgValue, pstStreamInfo))
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_GetAudioType fail !\n");
                return HI_FAILURE;
            }
        }
        else
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "unknow audio format!\n");
            if (RTSPCLIENT_SCANF_RET_ONE != sscanf(pszMedia, "%*[^ ] %*[^ ] %*[^ ] %d[^\r\n]", &pstSession->stDescribeInfo.as32PayloadType[RTSPCLIENT_STREAM_AUDIO]))
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "can't parse audio format!\n");
                return HI_FAILURE;
            }
        }
        if ( HI_LIVE_AUDIO_AAC == pstStreamInfo->stAencChAttr.enAudioFormat)
        {
            pszArgValue = RTSPCLIENT_MSGParser_SDP_GetPos(pszMedia, (HI_CHAR*)"config=");
            if (NULL != pszArgValue)
            {
                if (HI_SUCCESS != RTSPCLIENT_GetAudioInfo(pszArgValue, pstStreamInfo))
                {
                    RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_GetAudioInfo fail !\n");
                    return HI_FAILURE;
                }
            }
            else
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "can't parse audio confignum for aac !\n");
                return HI_FAILURE;
            }
        }

    }

    return HI_SUCCESS;
}




static HI_S32 RTSPCLIENT_Stream_ComposePingReq(HI_RTSPCLIENT_STREAM_S* pstSession, HI_CHAR* pchOutBuff)
{
    HI_S32  s32Strlen = 0;

    HI_CHAR aszRequestMothod[RTSPCLIENT_MAX_METHOD_LEN] = {0};

    HI_CHAR aszInputAuth[RTSPCLIENT_MAX_USERNAME_LEN + RTSPCLIENT_MAX_PASSWORD_LEN] = {0};
    HI_CHAR aszOutAuth[(RTSPCLIENT_MAX_USERNAME_LEN + RTSPCLIENT_MAX_PASSWORD_LEN) / 3 * 4 + 1] = {0};
    HI_S32 s32InputLen = 0;
    HI_S32 s32OutputLen = 0;

    if (NULL == pstSession || NULL == pchOutBuff)
    {
        return HI_FAILURE;
    }

    snprintf(aszInputAuth, RTSPCLIENT_MAX_AUTH_LEN, "%s:%s", pstSession->stAuthInfo.szUserName, pstSession->stAuthInfo.szPassword);
    s32InputLen = strlen(aszInputAuth);
    HI_RTSPCLIENT_Base64Encode((const unsigned char*)aszInputAuth, s32InputLen, (unsigned char*)aszOutAuth, s32OutputLen);

    if (pstSession->stProtoMsgInfo.bSetParam)
    {
        strncpy(aszRequestMothod, RTSP_METHOD_SET_PARAMETER, RTSPCLIENT_MAX_METHOD_LEN - 1);
    }
    else if (pstSession->stProtoMsgInfo.bGetParam)
    {
        strncpy(aszRequestMothod, RTSP_METHOD_GET_PARAMETER, RTSPCLIENT_MAX_METHOD_LEN - 1);
    }
    else
    {
        strncpy(aszRequestMothod, RTSP_METHOD_OPTIONS, RTSPCLIENT_MAX_METHOD_LEN - 1);
    }
    s32Strlen += snprintf(pchOutBuff + s32Strlen, RTSPCLIENT_MAXBUFFER_SIZE - s32Strlen, "%s %s %s\r\n", aszRequestMothod, pstSession->stProtoMsgInfo.szUrl, RTSP_VER);
    s32Strlen += snprintf(pchOutBuff + s32Strlen, RTSPCLIENT_MAXBUFFER_SIZE - s32Strlen, "CSeq: %d\r\n", pstSession->stProtoMsgInfo.u32SendSeqNum++);
    s32Strlen += snprintf(pchOutBuff + s32Strlen, RTSPCLIENT_MAXBUFFER_SIZE - s32Strlen, "Session: %s\r\n", pstSession->stProtoMsgInfo.szSessionId);

    if ( HI_RTSPCLIENT_AUTHEN_DIGEST == pstSession->stAuthInfo.enAuthenticate )
    {
        /*not compteled*/
    }
    else if ( HI_RTSPCLIENT_AUTHEN_BASIC == pstSession->stAuthInfo.enAuthenticate )
    {
        s32Strlen += snprintf(pchOutBuff + s32Strlen, RTSPCLIENT_MAXBUFFER_SIZE - s32Strlen, "Authorization: Basic %s\r\n", aszOutAuth);
    }

    s32Strlen += snprintf(pchOutBuff + s32Strlen, RTSPCLIENT_MAXBUFFER_SIZE - s32Strlen, "User-Agent: %s\r\n", pstSession->stProtoMsgInfo.szUserAgent);
    s32Strlen += snprintf(pchOutBuff + s32Strlen, RTSPCLIENT_MAXBUFFER_SIZE - s32Strlen, "\r\n");

    return HI_SUCCESS;
}

static HI_S32 RTSPCLIENT_Stream_TimerProcPing( HI_RTSPCLIENT_STREAM_S* pstSession, time_t* pLastTimeTick, HI_S32 s32Interval)
{
    HI_BOOL bSendPing = HI_FALSE;
    HI_S32 s32Ret = HI_SUCCESS;
    HI_CHAR szSendBuff[RTSPCLIENT_MAXBUFFER_SIZE] = {0};
    time_t t_Interval = 0;
    time_t t_now =  time(NULL);

    if ((NULL == pLastTimeTick) || (NULL == pstSession) || (s32Interval <= 0))
    {
        return HI_FAILURE;
    }

    if (t_now < *pLastTimeTick)
    {
        *pLastTimeTick = t_now;
    }
    else
    {
        t_Interval = t_now - (*pLastTimeTick);

        if (t_Interval >= s32Interval)
        {
            bSendPing = HI_TRUE;
            *pLastTimeTick = t_now;
        }
    }

    if (HI_TRUE == bSendPing)
    {
        s32Ret = RTSPCLIENT_Stream_ComposePingReq(pstSession, szSendBuff);
        if (HI_SUCCESS != s32Ret)
        {
            return s32Ret;
        }
        /*send heart beat packet*/
        s32Ret = RTSPCLIENT_SOCKET_Send(pstSession->s32SessSock, (HI_U8*)szSendBuff, strlen(szSendBuff));
        if (HI_SUCCESS != s32Ret)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR,
                          "RTSP_CLIENT_TRANS_ping fail: sock =%d, sendlen = %u.\n", pstSession->s32SessSock, (HI_U32)strlen(szSendBuff));
            return s32Ret;
        }

    }

    return HI_SUCCESS;
}

static HI_S32 RTSPCLIENT_Stream_OnRespOptions(HI_RTSPCLIENT_STREAM_S* pstSession)
{
    if (strstr(pstSession->stProtoMsgInfo.szInBuffer, "GET_PARAMETER"))
    {
        pstSession->stProtoMsgInfo.bGetParam = HI_TRUE;
    }

    if (strstr(pstSession->stProtoMsgInfo.szInBuffer, "SET_PARAMETER"))
    {
        pstSession->stProtoMsgInfo.bSetParam = HI_TRUE;
    }

    return HI_SUCCESS;
}

static HI_S32 RTSPCLIENT_Stream_OnRespDescribe(HI_RTSPCLIENT_STREAM_S* pstSession)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_LIVE_STREAM_CONTROL_S stControlInfo;
    stControlInfo.enTransType = HI_LIVE_TRANS_TYPE_BUTT;
    stControlInfo.bAudio = HI_FALSE;
    stControlInfo.bVideo = HI_FALSE;
    stControlInfo.s32AudioRecvPort = 0;
    stControlInfo.s32VideoRecvPort = 0;

    /*notify user client the media info*/
    s32Ret = RTSPCLIENT_Stream_Parse_Transport(pstSession, &pstSession->stStreamInfo);
    if (s32Ret != HI_SUCCESS)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_Stream_Parse_Transport failed\n");
        return s32Ret;
    }

    if ((HI_LIVE_STREAM_TYPE_VIDEO == pstSession->enStreamType || HI_LIVE_STREAM_TYPE_AV == pstSession->enStreamType) && HI_TRUE == pstSession->stStreamInfo.bVideo)
    {
        s32Ret = RTSPCLIENT_RespDesVideo(pstSession);
        if (HI_SUCCESS != s32Ret)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_RespVideo failed\n");
            return s32Ret;
        }
    }

    if ((HI_LIVE_STREAM_TYPE_AUDIO == pstSession->enStreamType || HI_LIVE_STREAM_TYPE_AV == pstSession->enStreamType) && HI_TRUE == pstSession->stStreamInfo.bAudio)
    {
        s32Ret = RTSPCLIENT_RespDesAudio(pstSession);
        if (HI_SUCCESS != s32Ret)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_RespDesAudio failed\n");
            goto RELEASE_VIDEO;
        }

    }

    if (pstSession->pfnOnMediaControl != NULL)
    {
        s32Ret = pstSession->pfnOnMediaControl(pstSession->ps32PrivData, &stControlInfo);
        if (s32Ret != HI_SUCCESS)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "pfnOnMediaControl failed\n");
            goto RELEASE_AUDIO;
        }
        pstSession->enTransType = stControlInfo.enTransType;

        if (HI_LIVE_TRANS_TYPE_TCP ==  pstSession->enTransType )
        {
            pstSession->enPackType = HI_PACK_TYPE_RTSP_ITLV;

        }
        else if (HI_LIVE_TRANS_TYPE_UDP == pstSession->enTransType  )
        {
            pstSession->enPackType = HI_PACK_TYPE_RTP;
            if ( HI_TRUE == pstSession->bVideo )
            {
                pstSession->pstRtpVideo->s32RecvPort = stControlInfo.s32VideoRecvPort;
            }
            if ( HI_TRUE == pstSession->bAudio )
            {
                pstSession->pstRtpAudio->s32RecvPort = stControlInfo.s32AudioRecvPort;
            }
        }
        else
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "not support transtype\n");
            goto RELEASE_AUDIO;
        }
    }
    else
    {
        pstSession->enTransType = HI_LIVE_TRANS_TYPE_TCP;
        pstSession->enPackType = HI_PACK_TYPE_RTSP_ITLV;
    }

    RTSPCLIENT_ParseRTP(pstSession);

    return HI_SUCCESS;
RELEASE_AUDIO:
    if (HI_TRUE == pstSession->bAudio)
    {
        pstSession->bAudio = HI_FALSE;
        RTCP_Session_Destroy(pstSession->pstRtcpAudio);
        RTP_Session_Destroy(pstSession->pstRtpAudio);
    }
RELEASE_VIDEO:
    if (HI_TRUE == pstSession->bVideo)
    {
        pstSession->bVideo  = HI_FALSE;
        RTCP_Session_Destroy(pstSession->pstRtcpVideo);
        RTP_Session_Destroy(pstSession->pstRtpVideo);
    }
    return s32Ret;
}

static HI_S32 RTSPCLIENT_Stream_OnRespSetup(HI_RTSPCLIENT_STREAM_S* pstSession)
{
    HI_S32 s32Ret = HI_SUCCESS;
    s32Ret = RTSPCLIENT_MSGParser_GetSessID(pstSession->stProtoMsgInfo.szInBuffer, pstSession->stProtoMsgInfo.szSessionId);
    if (s32Ret != HI_SUCCESS)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "Get Session failed\n");
        return HI_FAILURE;
    }

    if (HI_RTSPCLIENT_STATE_PLAY != pstSession->enClientState )
    {
        RTSPCLIENT_SetClientState(pstSession, HI_RTSPCLIENT_STATE_READY);
    }

    s32Ret = RTSPCLIENT_MSGParser_GetTimeout(pstSession->stProtoMsgInfo.szInBuffer, &pstSession->s32Timeout);
    if (s32Ret != HI_SUCCESS)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "Get Timeout failed\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

static HI_S32 RTSPCLIENT_Stream_OnRespPlay(HI_RTSPCLIENT_STREAM_S* pstSession)
{

    RTSPCLIENT_SetClientState(pstSession, HI_RTSPCLIENT_STATE_PLAY);

    /*set player's status as "connected"*/
    if (NULL != pstSession->pfnStateListener)
    {
        pstSession->pfnStateListener(pstSession->ps32PrivData, HI_PLAYSTAT_TYPE_CONNECTED);
    }

    return HI_SUCCESS;

}

static HI_S32 RTSPCLIENT_Stream_HandleResponse(HI_RTSPCLIENT_STREAM_S* pstSession)
{
    HI_S32 s32Ret = HI_SUCCESS;
    CHECK_NULL_ERROR(pstSession);
    if (NULL == strstr(pstSession->stProtoMsgInfo.szInBuffer, "RTSP/1.0 200 OK"))
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "the  response is not OK\n");
        return HI_FAILURE;
    }
    switch (pstSession->enProtocolPos)
    {
        case HI_RTSPCLIENT_METHOD_OPTIONS:
            s32Ret = RTSPCLIENT_Stream_OnRespOptions(pstSession);
            break;

        case HI_RTSPCLIENT_METHOD_DESCRIBE:
            s32Ret = RTSPCLIENT_Stream_OnRespDescribe(pstSession);
            break;

        case HI_RTSPCLIENT_METHOD_SETUP:
            s32Ret = RTSPCLIENT_Stream_OnRespSetup(pstSession);
            break;

        case HI_RTSPCLIENT_METHOD_PLAY:
            s32Ret = RTSPCLIENT_Stream_OnRespPlay(pstSession);
            break;

        default:
            break;
    }

    return s32Ret;
}

static HI_S32 RTSPCLIENT_Stream_Pack_Version(HI_RTSPCLIENT_STREAM_S* pstSession)
{
    /*get player version */
    pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer + pstSession->stProtoMsgInfo.u32OutSize, RTSPCLIENT_MAXBUFFER_SIZE - pstSession->stProtoMsgInfo.u32OutSize, "User-Agent: %s\r\n", pstSession->stProtoMsgInfo.szUserAgent);
    pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer + pstSession->stProtoMsgInfo.u32OutSize, RTSPCLIENT_MAXBUFFER_SIZE - pstSession->stProtoMsgInfo.u32OutSize, "\r\n");
    return HI_SUCCESS;
}

static HI_S32 RTSPCLIENT_Stream_RecvRTCP(HI_RTSPCLIENT_STREAM_S* pstSession, HI_U8* aszRecvBuff, HI_RTSP_ITLEAVED_HEAD_S* pRtspItlvHdr )
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32          u32RtcpBuffLen = 0;
    HI_U16          u16RtspItlvLength = 0;
    u16RtspItlvLength = ntohs(pRtspItlvHdr->u16Length);
    if (pstSession->pstRtcpVideo->u32RtcpBuffLen < (sizeof(HI_RTSP_ITLEAVED_HEAD_S) + u16RtspItlvLength))
    {
        u32RtcpBuffLen = (sizeof(HI_RTSP_ITLEAVED_HEAD_S) + u16RtspItlvLength);
        if (NULL != pstSession->pstRtcpVideo->pu8RtcpBuff)
        {
            free(pstSession->pstRtcpVideo->pu8RtcpBuff);
            pstSession->pstRtcpVideo->pu8RtcpBuff = NULL;
        }
        pstSession->pstRtcpVideo->pu8RtcpBuff = (HI_U8*)malloc(sizeof(HI_U8) * u32RtcpBuffLen);
        if (NULL == pstSession->pstRtcpVideo->pu8RtcpBuff)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "malloc trans receive buff failed errno %d.RtspItlvLength = %u\n", errno, u16RtspItlvLength);
            return HI_FAILURE;
        }

        pstSession->pstRtcpVideo->u32RtcpBuffLen = u32RtcpBuffLen;
    }
    /*copy itlv head to RtcpBuf*/
    memcpy(pstSession->pstRtcpVideo->pu8RtcpBuff, aszRecvBuff, sizeof(HI_RTSP_ITLEAVED_HEAD_S));

    s32Ret = RTSPCLIENT_SOCKET_Read(pstSession->s32SessSock, pstSession->pstRtcpVideo->pu8RtcpBuff + sizeof(HI_RTSP_ITLEAVED_HEAD_S), u16RtspItlvLength);
    if (HI_SUCCESS != s32Ret )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTP_Session_Recv_RTCP withdraw recv thread ret:%d.\n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

static HI_S32 RTSPCLIENT_Stream_RecvRTP(HI_RTSPCLIENT_STREAM_S* pstSession, HI_U8* aszRecvBuff, HI_RTSP_ITLEAVED_HEAD_S* pRtspItlvHdr)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32  u32RtpBuffLen = 0;
    HI_U16  u16RtspItlvLength = 0;
    HI_U32  u32PayloadLen   = 0;
    HI_S32   u32PackLen = 0; /*media pack len*/
    HI_U32   u32StreamHeadLen = 0;
    HI_LIVE_STREAM_TYPE_E   enStreamType = HI_LIVE_STREAM_TYPE_BUTT;
    u16RtspItlvLength = ntohs(pRtspItlvHdr->u16Length);

    u32StreamHeadLen =  sizeof(RTSP_ITLEAVED_RTP_HEAD_S);

    if (pstSession->pstRtpVideo->u32RtpBuffLen < (sizeof(HI_RTSP_ITLEAVED_HEAD_S) + u16RtspItlvLength))
    {
        u32RtpBuffLen = (sizeof(HI_RTSP_ITLEAVED_HEAD_S) + u16RtspItlvLength);
        if (NULL != pstSession->pstRtpVideo->pu8RtpBuff)
        {
            free(pstSession->pstRtpVideo->pu8RtpBuff);
            pstSession->pstRtpVideo->pu8RtpBuff = NULL;
        }
        pstSession->pstRtpVideo->pu8RtpBuff = (HI_U8*)malloc(sizeof(HI_U8) * u32RtpBuffLen);
        if (NULL == pstSession->pstRtpVideo->pu8RtpBuff )
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "malloc trans receive buff failed errno %d.RtspItlvLength = %u\n", errno, u16RtspItlvLength);
            return HI_FAILURE;
        }
        pstSession->pstRtpVideo->u32RtpBuffLen = u32RtpBuffLen;
    }

    /*copy itlv head to RtpBuf*/
    memcpy(pstSession->pstRtpVideo->pu8RtpBuff, aszRecvBuff, sizeof(HI_RTSP_ITLEAVED_HEAD_S));

    s32Ret = RTSPCLIENT_SOCKET_Read(pstSession->s32SessSock, pstSession->pstRtpVideo->pu8RtpBuff + sizeof(HI_RTSP_ITLEAVED_HEAD_S), sizeof(HI_RTP_HEAD_S));
    if (HI_SUCCESS != s32Ret )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTP_Session_Recv_RTPHead withdraw recv thread ret:%d .\n", s32Ret);
        return HI_FAILURE;
    }

    if ( u16RtspItlvLength < sizeof(HI_RTP_HEAD_S) )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "rtp buffer overflow error RTP_Session_Recv_RTPHead withdraw recv thread ret:%d .\n", s32Ret);
        return HI_FAILURE;
    }
    /*recv the rtp data */
    s32Ret = RTSPCLIENT_SOCKET_Read(pstSession->s32SessSock, pstSession->pstRtpVideo->pu8RtpBuff + sizeof(RTSP_ITLEAVED_RTP_HEAD_S), u16RtspItlvLength - sizeof(HI_RTP_HEAD_S));
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "recv rtp error withdraw recv  ret:%d.\n",  s32Ret);
        return HI_FAILURE;
    }
    /*handle the recv media data*/
    RTP_Session_GetStreamInfo(pstSession->pstRtpVideo, pstSession->pstRtpVideo->pu8RtpBuff , &enStreamType, &u32PayloadLen);
    u32PackLen = u32PayloadLen + u32StreamHeadLen;
    RTSPCLIENT_Stream_ProcStream(pstSession->pstRtpVideo, pstSession->pstRtpVideo->pu8RtpBuff , u32PackLen, enStreamType);

    return HI_SUCCESS;
}

/* not completed now*/
static HI_VOID* RTSPCLIENT_Stream_RecvUDPProc(HI_VOID* args)
{
    HI_RTSPCLIENT_STREAM_S* pstSession = NULL;

    prctl(PR_SET_NAME, (unsigned long)"RecvProc_UDP", 0, 0, 0);

    if (NULL == args)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR,  "withdraw recv thread:iuput arg is null\r\n");
        return HI_NULL;
    }

    pstSession  = (HI_RTSPCLIENT_STREAM_S*)args;

    while (HI_RTSPCLIENT_STATE_PLAY == pstSession->enClientState  || HI_RTSPCLIENT_STATE_READY == pstSession->enClientState )
    {
        hi_usleep(200000);
    }

    if (!(HI_RTSPCLIENT_STATE_STOP == pstSession->enClientState ))
    {
        /*not completed now*/
    }

    return HI_NULL;

}

static HI_VOID*  RTSPCLIENT_Stream_RecvTCPProc(HI_VOID* args)
{
    HI_S32   s32Ret = HI_SUCCESS;
    HI_S32   s32SockError = 0;
    HI_S32   FirstRecvlen = 4;/*start recv bytes */
    HI_RTSPCLIENT_STREAM_S* pstSession = NULL;
    HI_U8           aszRecvBuff[RTSPCLIENT_MAXBUFFER_SIZE] = {0};
    HI_RTSP_ITLEAVED_HEAD_S* pRtspItlvHdr = NULL;
    struct          timeval tv;
    fd_set          readFds;
    time_t          TimeTick = time(NULL);
    prctl(PR_SET_NAME, (unsigned long)"RecvProc_TCP", 0, 0, 0);

    if (NULL == args)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR,  "null param withdraw recv thread:iuput arg is null\r\n");
        return HI_NULL;
    }

    pstSession  = (HI_RTSPCLIENT_STREAM_S*)args;
    while (HI_RTSPCLIENT_STATE_PLAY == pstSession->enClientState  || HI_RTSPCLIENT_STATE_READY == pstSession->enClientState )
    {
        tv.tv_sec  = RTSPCLIENT_TRANS_TIMEVAL_SEC;
        tv.tv_usec = RTSPCLIENT_TRANS_TIMEVAL_USEC;
        FD_ZERO(&readFds);
        FD_SET(pstSession->s32SessSock, &readFds);
        if (HI_RTSPCLIENT_STATE_READY == pstSession->enClientState)
        {
            hi_usleep(10000);
            continue;
        }
        /*judge if recv socket has data to read*/
        s32Ret = select(pstSession->s32SessSock + 1, &readFds, NULL, NULL, &tv);
        if ( 0 > s32Ret )
        {
            if (EINTR == errno || EAGAIN == errno)
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "  [select err: %s]\n",  strerror(errno));
                continue;
            }
            s32SockError = errno;
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "withdraw recv thread %llu :select from sock %d error = %s\r\n",
                          (HI_U64)pstSession->ptSessThdId, pstSession->s32SessSock, strerror(s32SockError));
            goto ErrorProc;
        }
        else if ( 0 == s32Ret)
        {
            s32SockError = errno;
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "withdraw recv thread %llu :select from sock %d error=%s (over time %d.%d secs)\r\n",
                          (HI_U64)pstSession->ptSessThdId, pstSession->s32SessSock, strerror(s32SockError), RTSPCLIENT_TRANS_TIMEVAL_SEC, RTSPCLIENT_TRANS_TIMEVAL_USEC);
            goto ErrorProc;
        }

        /*select ok and recv socket not in fdset, some wrong happend*/
        if (!FD_ISSET(pstSession->s32SessSock, &readFds))
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "sock %d for recving data is not in fdset.\r\n", pstSession->s32SessSock);
            goto ErrorProc;
        }

        if (pstSession->s32Timeout > RTSPCLIENT_TRANS_TIMEVAL_SEC)
        {
            RTSPCLIENT_Stream_TimerProcPing(pstSession, &TimeTick, (pstSession->s32Timeout - RTSPCLIENT_TRANS_TIMEVAL_SEC));
        }

        s32Ret = RTSPCLIENT_SOCKET_Read(pstSession->s32SessSock, aszRecvBuff, FirstRecvlen);
        if (HI_SUCCESS != s32Ret)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "read first  error withdraw recv thread %llu  ret:%d.\n", (HI_U64)pstSession->ptSessThdId, s32Ret);
            goto ErrorProc;
        }

        if (0 == strncmp((const HI_CHAR*)aszRecvBuff, "RTSP", 4))
        {
            s32Ret = RTSPCLIENT_Stream_RecvMsg(pstSession, aszRecvBuff);
            if (HI_SUCCESS != s32Ret)
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_Stream_RecvMsg failed recv thread %llu  ret:%d.\n", (HI_U64)pstSession->ptSessThdId, s32Ret);
                goto ErrorProc;
            }
            continue;
        }
        else if ( RTSP_CLIENT_ITLV_HEAD == aszRecvBuff[0] ) /*interleaved*/
        {
            pRtspItlvHdr = (HI_RTSP_ITLEAVED_HEAD_S*)aszRecvBuff;

            if ((pstSession->stDescribeInfo.as32RtpChn[RTSPCLIENT_STREAM_VIDEO] + 1 == pRtspItlvHdr->u8Channel)
                || (pstSession->stDescribeInfo.as32RtpChn[RTSPCLIENT_STREAM_AUDIO] + 1 == pRtspItlvHdr->u8Channel))
            {
                s32Ret = RTSPCLIENT_Stream_RecvRTCP(pstSession, aszRecvBuff, pRtspItlvHdr);
                if (HI_SUCCESS != s32Ret)
                {
                    RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_Stream_RecvRTCP failed recv thread %llu  ret:%d.\n", (HI_U64)pstSession->ptSessThdId, s32Ret);
                    goto ErrorProc;
                }
                continue;
            }
            else if ((pstSession->stDescribeInfo.as32RtpChn[RTSPCLIENT_STREAM_VIDEO] == pRtspItlvHdr->u8Channel)
                     || (pstSession->stDescribeInfo.as32RtpChn[RTSPCLIENT_STREAM_AUDIO] == pRtspItlvHdr->u8Channel))
            {
                s32Ret = RTSPCLIENT_Stream_RecvRTP(pstSession, aszRecvBuff, pRtspItlvHdr);
                if (HI_SUCCESS != s32Ret)
                {
                    RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_Stream_RecvRTP failed recv thread %llu  ret:%d.\n", (HI_U64)pstSession->ptSessThdId, s32Ret);
                    goto ErrorProc;
                }
            }
            else
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "unkown itlv channel");
                goto ErrorProc;
            }
        }
        else
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "unkown itlv data\n");
            goto ErrorProc;
        }

    }

ErrorProc:

    if (!(HI_RTSPCLIENT_STATE_STOP == pstSession->enClientState ))
    {
        if (NULL != pstSession->pfnStateListener)
        {
            pstSession->pfnStateListener(pstSession->ps32PrivData, HI_PLAYSTAT_TYPE_ABORTIBE_DISCONNECT);
        }
    }

    return HI_NULL;
}

HI_S32 RTSPCLIENT_Stream_Pack_Option(HI_RTSPCLIENT_STREAM_S* pstSession)
{
    if ( NULL == pstSession)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_Stream_Pack_Option  null param\n");
        return HI_FAILURE;
    }

    pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer, RTSPCLIENT_MAXBUFFER_SIZE, "OPTIONS %s %s\r\n", pstSession->stProtoMsgInfo.szUrl, RTSP_VER);
    pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer + pstSession->stProtoMsgInfo.u32OutSize, RTSPCLIENT_MAXBUFFER_SIZE - pstSession->stProtoMsgInfo.u32OutSize, "CSeq: %d\r\n", pstSession->stProtoMsgInfo.u32SendSeqNum++);

    /*Digest*/
    if (HI_RTSPCLIENT_AUTHEN_DIGEST == pstSession->stAuthInfo.enAuthenticate)
    {
        /*to do*/
    }
    /*Basic*/
    else if (HI_RTSPCLIENT_AUTHEN_BASIC == pstSession->stAuthInfo.enAuthenticate)
    {
        /*to do*/
    }

    /*not authenticate now*/
    RTSPCLIENT_Stream_Pack_Version(pstSession);

    return HI_SUCCESS;

}

HI_S32 RTSPCLIENT_Stream_Pack_Describe(HI_RTSPCLIENT_STREAM_S* pstSession)
{
    if ( NULL == pstSession)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_Stream_Pack_Describe  null param\n");
        return HI_FAILURE;
    }

    pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer,
            RTSPCLIENT_MAXBUFFER_SIZE, "DESCRIBE %s %s\r\n", pstSession->stProtoMsgInfo.szUrl, RTSP_VER);
    pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer + pstSession->stProtoMsgInfo.u32OutSize,
            RTSPCLIENT_MAXBUFFER_SIZE - pstSession->stProtoMsgInfo.u32OutSize, "CSeq: %d\r\n", pstSession->stProtoMsgInfo.u32SendSeqNum++);
    pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer + pstSession->stProtoMsgInfo.u32OutSize,
            RTSPCLIENT_MAXBUFFER_SIZE - pstSession->stProtoMsgInfo.u32OutSize, "Accept: application/sdp\r\n");

    if (HI_RTSPCLIENT_AUTHEN_DIGEST == pstSession->stAuthInfo.enAuthenticate)
    {
        /*to do*/
    }
    else if (HI_RTSPCLIENT_AUTHEN_BASIC == pstSession->stAuthInfo.enAuthenticate)
    {
        /*to do*/
    }

    RTSPCLIENT_Stream_Pack_Version(pstSession);

    return HI_SUCCESS;

}

HI_S32 RTSPCLIENT_Stream_Pack_SetUp(HI_RTSPCLIENT_STREAM_S* pstSession,  HI_CHAR* pszTrackUrl, HI_LIVE_STREAM_TYPE_E enStreamType)
{
    HI_RTP_SESSION_S* pstRtpSession = NULL;

    if (NULL == pszTrackUrl || NULL == pstSession)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "Pack_SetUp null param\n");
        return HI_FAILURE;
    }


    pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer,
            RTSPCLIENT_MAXBUFFER_SIZE, "SETUP %s %s\r\n", pszTrackUrl, RTSP_VER);
    pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer + pstSession->stProtoMsgInfo.u32OutSize,
            RTSPCLIENT_MAXBUFFER_SIZE - pstSession->stProtoMsgInfo.u32OutSize, "CSeq: %d\r\n", pstSession->stProtoMsgInfo.u32SendSeqNum++);

    if (HI_LIVE_TRANS_TYPE_TCP == pstSession->enTransType)
    {
        pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer + pstSession->stProtoMsgInfo.u32OutSize,
                RTSPCLIENT_MAXBUFFER_SIZE - pstSession->stProtoMsgInfo.u32OutSize, "Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n",
                pstSession->stDescribeInfo.as32RtpChn[enStreamType], pstSession->stDescribeInfo.as32RtpChn[enStreamType] + 1);
    }
    else if (HI_LIVE_TRANS_TYPE_UDP == pstSession->enTransType)
    {
        if (HI_LIVE_STREAM_TYPE_VIDEO == enStreamType )
        {
            pstRtpSession = pstSession->pstRtpVideo;
        }
        else if (HI_LIVE_STREAM_TYPE_AUDIO == enStreamType)
        {
            pstRtpSession = pstSession->pstRtpAudio;
        }
        else
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "Pack_SetUp unsupport media type\n");
            return HI_FAILURE;
        }
        pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer + pstSession->stProtoMsgInfo.u32OutSize,
                RTSPCLIENT_MAXBUFFER_SIZE - pstSession->stProtoMsgInfo.u32OutSize, "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n",
                pstRtpSession->s32RecvPort, pstRtpSession->s32RecvPort + 1);
    }
    else
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "Pack_SetUp unsupport transtype\n");
        return HI_FAILURE;
    }

    if (strlen(pstSession->stProtoMsgInfo.szSessionId) > 0)
    {
        pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer + pstSession->stProtoMsgInfo.u32OutSize,
                RTSPCLIENT_MAXBUFFER_SIZE - pstSession->stProtoMsgInfo.u32OutSize, "Session: %s\r\n", pstSession->stProtoMsgInfo.szSessionId);
    }

    if (HI_RTSPCLIENT_AUTHEN_DIGEST == pstSession->stAuthInfo.enAuthenticate)
    {
        /*do to */
    }
    else if (HI_RTSPCLIENT_AUTHEN_BASIC == pstSession->stAuthInfo.enAuthenticate)
    {
        /*to do*/
    }

    RTSPCLIENT_Stream_Pack_Version(pstSession);
    return HI_SUCCESS;
}

HI_S32 RTSPCLIENT_Stream_Pack_Play(HI_RTSPCLIENT_STREAM_S* pstSession)
{
    if ( NULL == pstSession)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_Stream_Pack_Play  null param\n");
        return HI_FAILURE;
    }

    pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer + pstSession->stProtoMsgInfo.u32OutSize,
            RTSPCLIENT_MAXBUFFER_SIZE - pstSession->stProtoMsgInfo.u32OutSize, "PLAY %s %s\r\n", pstSession->stProtoMsgInfo.szUrl, RTSP_VER);
    pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer + pstSession->stProtoMsgInfo.u32OutSize,
            RTSPCLIENT_MAXBUFFER_SIZE - pstSession->stProtoMsgInfo.u32OutSize, "CSeq: %d\r\n", pstSession->stProtoMsgInfo.u32SendSeqNum++);
    pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer + pstSession->stProtoMsgInfo.u32OutSize,
            RTSPCLIENT_MAXBUFFER_SIZE - pstSession->stProtoMsgInfo.u32OutSize, "Session: %s\r\n", pstSession->stProtoMsgInfo.szSessionId);
    pstSession->stProtoMsgInfo.u32OutSize += snprintf(pstSession->stProtoMsgInfo.szOutBuffer + pstSession->stProtoMsgInfo.u32OutSize,
            RTSPCLIENT_MAXBUFFER_SIZE - pstSession->stProtoMsgInfo.u32OutSize, "Range: npt=0.000-\r\n");

    if (HI_RTSPCLIENT_AUTHEN_DIGEST == pstSession->stAuthInfo.enAuthenticate)
    {
        /*to do*/
    }
    else if (HI_RTSPCLIENT_AUTHEN_BASIC == pstSession->stAuthInfo.enAuthenticate)
    {
        /*to do*/
    }

    RTSPCLIENT_Stream_Pack_Version(pstSession);
    return HI_SUCCESS;
}

HI_S32 RTSPCLIENT_Stream_Connect(const HI_CHAR* pchServer, HI_U16 u16Port, HI_S32* ps32SessSock)
{
    struct sockaddr_in SockAddr;
    memset(&SockAddr, 0, sizeof(struct sockaddr_in));
    if (0 == inet_aton(pchServer, (struct in_addr*) & (SockAddr.sin_addr.s_addr )))
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_DEBUG, "wrong ip address cause connect  error\n");
        return HI_FAILURE;
    }

    /* after get the hostent*/
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_port = htons(u16Port);
    *ps32SessSock =  RTSPCLIENT_Stream_OpenTcp((struct sockaddr*) & SockAddr, sizeof(SockAddr));

    if (*ps32SessSock < 0)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_DEBUG, "HI_RTSPCLIENT_SOCKET_OpenTcp  fail\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;

}

HI_S32 RTSPCLIENT_Stream_Parse_Url(HI_RTSPCLIENT_STREAM_S* pstSession)
{
    HI_CHAR aszTemp[RTSPCLIENT_MAXURL_LEN] = {0};
    HI_CHAR aszHost[RTSPCLIENT_MAXIP_LEN] = {0};
    HI_CHAR* pszUserName = NULL;
    HI_CHAR* pszPassWord = NULL;
    HI_CHAR* pszIPTemp = NULL;
    HI_CHAR* pszIPEnd = NULL;

    if (NULL == pstSession )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_Parse_Url param null\n");
        return HI_FAILURE;
    }

    if (strncmp(pstSession->stProtoMsgInfo.szUrl, RTSP_CLIENT_STR, strlen(RTSP_CLIENT_STR)))
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_Parse_Url invalid url\n");
        return HI_FAILURE;
    }

    /*check for username and password*/
    pszIPTemp = pstSession->stProtoMsgInfo.szUrl + strlen(RTSP_CLIENT_STR);
    pszIPEnd = strchr(pszIPTemp, '/');

    if (NULL == pszIPEnd)
    {
        pszIPEnd = strchr(pszIPTemp, '\0');
    }

    strncpy(aszTemp, pszIPTemp, RTSPCLIENT_MAXURL_LEN - 1);
    aszTemp[RTSPCLIENT_MAXURL_LEN - 1] = '\0';
    pszUserName = strchr(aszTemp, ':');
    pszPassWord = strrchr(aszTemp, '@');

    if (NULL != pszUserName && NULL != pszPassWord)
    {
        if (pszPassWord - pszUserName > 0)
        {
            strncpy(pstSession->stAuthInfo.szUserName, aszTemp, RTSPCLIENT_MAX_USERNAME_LEN - 1);
            strncpy(pstSession->stAuthInfo.szPassword, pszUserName + 1, RTSPCLIENT_MAX_PASSWORD_LEN - 1);
            pszIPTemp = pszIPTemp + (pszPassWord - aszTemp) + 1;
            memset(aszTemp, 0, RTSPCLIENT_MAXURL_LEN);
            snprintf(aszTemp, RTSPCLIENT_MAXURL_LEN, "rtsp://%s", pszIPTemp);
        }
        else
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "parse url  failed cannot get the password and username\n");
            return HI_FAILURE;
        }
    }
    else
    {
        memset(aszTemp, 0, RTSPCLIENT_MAXURL_LEN);
        strncpy(aszTemp, pstSession->stProtoMsgInfo.szUrl, RTSPCLIENT_MAXURL_LEN - 1);
        aszTemp[RTSPCLIENT_MAXURL_LEN - 1] = '\0';
    }

    /*parse the ip address*/
    if (pszIPEnd - pszIPTemp > RTSP_CLIENT_IP_LEN)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "HI_RTSPCLIENT_Stream_Parse_Url invalid url parse IP addr failed\n");
        return HI_FAILURE;
    }

    strncpy(aszHost, pszIPTemp, RTSPCLIENT_MAXIP_LEN - 1);
    aszHost[RTSPCLIENT_MAXIP_LEN - 1] = '\0';
    /*check for the listen port*/
    pszIPTemp = strchr(aszHost, ':');

    if (pszIPTemp)
    {
        pszIPEnd = pszIPTemp;
        pszIPTemp++;
        pstSession->s32SvrLisPort = atoi(pszIPTemp);
        aszHost[pszIPEnd - aszHost] = '\0';
    }
    else
    {
        pstSession->s32SvrLisPort = RTSP_CLIENT_LISTENER_PORT;
    }

    strncpy(pstSession->stProtoMsgInfo.szServerIp, aszHost, strlen(aszHost));
    memset(pstSession->stProtoMsgInfo.szUrl, 0, RTSPCLIENT_MAXURL_LEN);
    strncpy(pstSession->stProtoMsgInfo.szUrl, aszTemp, RTSPCLIENT_MAXURL_LEN - 1);
    pstSession->stProtoMsgInfo.szUrl[RTSPCLIENT_MAXURL_LEN - 1] = '\0';
    return HI_SUCCESS;

}

HI_VOID* RTSPCLIENT_Stream_RecvResponseProc(HI_VOID* args)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_S32 s32SockError = 0;
    HI_RTSPCLIENT_STREAM_S* pstSession = NULL;
    struct          timeval tv;
    fd_set          readFds;
    prctl(PR_SET_NAME, (unsigned long)"RecvResponseProc", 0, 0, 0);

    if (NULL == args)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR,  "withdraw recv thread:iuput arg is null\r\n");
        return HI_NULL;
    }
    pstSession  = (HI_RTSPCLIENT_STREAM_S*)args;

    while ( pstSession->enClientState != HI_RTSPCLIENT_STATE_PLAY && pstSession->enClientState != HI_RTSPCLIENT_STATE_STOP)
    {
        tv.tv_sec  = RTSPCLIENT_TRANS_TIMEVAL_SEC;
        tv.tv_usec = RTSPCLIENT_TRANS_TIMEVAL_USEC;
        FD_ZERO(&readFds);
        FD_SET(pstSession->s32SessSock, &readFds);

        /*judge if recv socket has data to read*/
        s32Ret = select(pstSession->s32SessSock + 1, &readFds, NULL, NULL, &tv);
        if (s32Ret < 0)
        {
            if (EINTR == errno || EAGAIN == errno)
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "  [select err: %s]\n",  strerror(errno));
                continue;
            }
            s32SockError = errno;
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "withdraw recv thread %llu :select from sock %d error = %s\r\n",
                          (HI_U64)pstSession->ptSessThdId, pstSession->s32SessSock, strerror(s32SockError));
            goto ErrorProc;
        }
        else if ( 0 == s32Ret )
        {
            s32SockError = errno;
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "recv thread %llu :select from sock %d error=%s (over time %d.%d secs)\r\n",
                          (HI_U64)pstSession->ptSessThdId, pstSession->s32SessSock, strerror(s32SockError), RTSPCLIENT_TRANS_TIMEVAL_SEC, RTSPCLIENT_TRANS_TIMEVAL_USEC);
            goto ErrorProc;
        }

        /*select ok and recv socket not in fdset, some wrong happend*/
        if (!FD_ISSET(pstSession->s32SessSock, &readFds))
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "sock %d for recving data is not in fdset. \r\n", pstSession->s32SessSock);
            goto ErrorProc;
        }

        RTSPCLIENT_ClearInBuff(pstSession);
        s32Ret = RTSPCLIENT_SOCKET_ReadProtocolMsg(pstSession->s32SessSock, (HI_U8*)pstSession->stProtoMsgInfo.szInBuffer, sizeof(pstSession->stProtoMsgInfo.szInBuffer));
        if (s32Ret != HI_SUCCESS)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_SOCKET_ReadProtocolMsg  failed\n");
            goto ErrorProc;
        }

        s32Ret = RTSPCLIENT_Stream_HandleResponse(pstSession);
        if (s32Ret != HI_SUCCESS)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_Stream_HandleResponse  failed\n");
            goto ErrorProc;
        }
        /*notify user response received and send next request*/
        if (pstSession->stListener.onNotifyResponse)
        {
            pstSession->stListener.onNotifyResponse((HI_MW_PTR)pstSession, pstSession->stProtoMsgInfo.szInBuffer, pstSession->enProtocolPos);
        }

    }
ErrorProc:
    if (pstSession->enClientState != HI_RTSPCLIENT_STATE_STOP && pstSession->enClientState != HI_RTSPCLIENT_STATE_PLAY)
    {
        if (NULL != pstSession->pfnStateListener)
        {
            pstSession->pfnStateListener(pstSession->ps32PrivData, HI_PLAYSTAT_TYPE_ABORTIBE_DISCONNECT);
        }
    }

    return HI_NULL;
}

/* start recv process */
HI_S32 RTSPCLIENT_Stream_CreateRecvProcess(HI_RTSPCLIENT_STREAM_S* pstSession)
{
    HI_S32 s32Ret = HI_SUCCESS;

    CHECK_NULL_ERROR(pstSession);
    if ((HI_TRUE == pstSession->bAudio) )
    {
        s32Ret = RTP_Session_Start(pstSession->pstRtpAudio);
        if (HI_SUCCESS != s32Ret )
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "[Fail]HI_CLIENT_TRANS_Recv_Start failed %X \r\n", s32Ret);
            return s32Ret;
        }
        s32Ret = RTCP_Session_Start(pstSession->pstRtcpAudio);
        if (HI_SUCCESS != s32Ret )
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "[Fail]HI_CLIENT_TRANS_Recv_Start failed %X \r\n", s32Ret);
            goto AUDIO_RTP_STOP;
        }
    }

    if ((HI_TRUE == pstSession->bVideo))
    {
        s32Ret = RTP_Session_Start(pstSession->pstRtpVideo);
        if (HI_SUCCESS != s32Ret )
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "[Fail]HI_CLIENT_TRANS_Recv_Start failed %X \r\n", s32Ret);
            goto AUDIO_RTCP_STOP;
        }
        s32Ret = RTCP_Session_Start(pstSession->pstRtcpVideo);
        if (HI_SUCCESS != s32Ret )
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "[Fail]HI_CLIENT_TRANS_Recv_Start failed %X \r\n", s32Ret);
            goto VIDEO_RTP_STOP;
        }
    }

    if (HI_LIVE_TRANS_TYPE_UDP == pstSession->enTransType)
    {
        s32Ret = pthread_create(&(pstSession->ptSessThdId), NULL, RTSPCLIENT_Stream_RecvUDPProc, (HI_VOID*)pstSession);
        if (s32Ret !=  HI_SUCCESS)
        {
            pstSession->enClientState = HI_RTSPCLIENT_STATE_INIT;
            pstSession->ptSessThdId = RTSPCLIENT_INVALID_THD_ID;
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "pthread_create error %d\r\n", s32Ret);
            goto  VIDEO_RTCP_STOP;
        }

    }
    else if (HI_LIVE_TRANS_TYPE_TCP == pstSession->enTransType)
    {
        s32Ret = pthread_create(&(pstSession->ptSessThdId), NULL, RTSPCLIENT_Stream_RecvTCPProc, (HI_VOID*)pstSession);
        if (s32Ret !=  HI_SUCCESS)
        {
            pstSession->enClientState = HI_RTSPCLIENT_STATE_INIT;
            pstSession->ptSessThdId = RTSPCLIENT_INVALID_THD_ID;
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "pthread_create error %d\r\n", s32Ret);
            goto  VIDEO_RTCP_STOP;
        }
    }
    else
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "not support transtype  error\r\n");
        goto  VIDEO_RTCP_STOP;
    }

    return HI_SUCCESS;
VIDEO_RTCP_STOP:
    if ((HI_TRUE == pstSession->bVideo) )
    {
        RTCP_Session_Stop(pstSession->pstRtcpVideo);
    }
VIDEO_RTP_STOP:
    if ((HI_TRUE == pstSession->bVideo) )
    {
        RTP_Session_Stop(pstSession->pstRtpVideo);
    }
AUDIO_RTCP_STOP:
    if ((HI_TRUE == pstSession->bAudio) )
    {
        RTCP_Session_Stop(pstSession->pstRtcpAudio);
    }
AUDIO_RTP_STOP:
    if ((HI_TRUE == pstSession->bAudio) )
    {
        RTP_Session_Stop(pstSession->pstRtpAudio);
    }
    return s32Ret;
}

HI_S32 RTSPCLIENT_Stream_DestroyRecvProcess(HI_RTSPCLIENT_STREAM_S* pstSession)
{
    HI_S32 s32Ret = HI_SUCCESS;
    pstSession->enClientState = HI_RTSPCLIENT_STATE_STOP;

    if (RTSPCLIENT_INVALID_THD_ID != pstSession->ptSessThdId)
    {
        if (HI_SUCCESS != pthread_join(pstSession->ptSessThdId, NULL))
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "join stream recv pthread fail = %d:%s\n", s32Ret, strerror(errno));
        }
        pstSession->ptSessThdId = RTSPCLIENT_INVALID_THD_ID;
    }

    if (pstSession->bAudio)
    {
        s32Ret = RTCP_Session_Stop(pstSession->pstRtcpAudio);
        if (HI_SUCCESS != s32Ret )
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "[Fail]audio RTCP_Session_Stop failed %X \r\n", s32Ret);
            return s32Ret;
        }

        s32Ret =  RTP_Session_Stop(pstSession->pstRtpAudio);
        if (HI_SUCCESS != s32Ret )
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "[Fail]audio RTP_Session_Stop failed %X \r\n", s32Ret);
            return s32Ret;
        }

    }

    if (pstSession->bVideo)
    {
        s32Ret = RTP_Session_Stop(pstSession->pstRtpVideo);
        if (HI_SUCCESS != s32Ret )
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "[Fail]video RTP_Session_Stop failed %X \r\n", s32Ret);
            return s32Ret;
        }

        s32Ret = RTCP_Session_Stop(pstSession->pstRtcpVideo);
        if (HI_SUCCESS != s32Ret )
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "[Fail]video RTCP_Session_Stop failed %X \r\n", s32Ret);
            return s32Ret;
        }

    }

    return HI_SUCCESS;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
