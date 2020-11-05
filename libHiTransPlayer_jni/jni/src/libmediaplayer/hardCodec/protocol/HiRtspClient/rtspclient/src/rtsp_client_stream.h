/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      rtsp_client_stream.h
* @brief     rtspclient stream on demand connect process funcs head file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/
#ifndef __RTSP_CLIENT_STREAM_SESSION_H__
#define __RTSP_CLIENT_STREAM_SESSION_H__

#include <pthread.h>
#include "hi_type.h"
#include "hi_list.h"
#include "hi_rtsp_client_err.h"
#include "hi_rtsp_client_comm.h"
#include "hi_rtsp_client_stream_ext.h"
#include "rtp_session.h"
#include "rtcp_session.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#define HISITYPE (0x06)
#define SDP         (1)
#define MH          (0)
#define RTSPCLIENT_MAX_STR_LEN (512)
#define RTSPCLIENT_MAX_AUTH_LEN (128)
#define RTSPCLIENT_MAX_METHOD_LEN (32)
#define RTSPCLIENT_MAX_SESSID_LEN   (64)
#define RTSPCLIENT_MPEG4_GENERIC (0x05)
#define RTSPCLIENT_TRACKID_VIDEO (0)
#define RTSPCLIENT_TRACKID_AUDIO (1)
#define RTSPCLIENT_INVALID_TRACK_ID (-1)
#define RTSPCLIENT_INVALID_THREAD_ID (-1)
#define RTSPCLIENT_MAX_STREAM_NUM (3)
#define RTSPCLIENT_MAX_NALBASE_LEN (256)


#define CHECK_NULL_ERROR(condition) \
    do \
    { \
        if( NULL == condition ) \
        { \
            return HI_ERR_RTSPCLIENT_NULL_PTR; \
        }\
    }while(0)
#define CHECK_INVALID_HANDLE(condition) \
    do \
    { \
        if( HI_NULL == condition ) \
        { \
            return HI_ERR_RTSPCLIENT_HANDLE_INVALID; \
        }\
    }while(0)

typedef enum hiRTSPCLIENT_STATE_E
{
    HI_RTSPCLIENT_STATE_INIT = 0,
    HI_RTSPCLIENT_STATE_READY,
    HI_RTSPCLIENT_STATE_PLAY,
    HI_RTSPCLIENT_STATE_STOP,
    HI_RTSPCLIENT_STATE_BUTT
} HI_RTSPCLIENT_STATE_E;

typedef enum hiRTSPCLIENT_AUTHEN_E
{
    HI_RTSPCLIENT_AUTHEN_BASIC = 0,
    HI_RTSPCLIENT_AUTHEN_DIGEST,
    HI_RTSPCLIENT_AUTHEN_BUTT
} HI_RTSPCLIENT_AUTHEN_E;

typedef struct hiRTSPCLIENT_AUTH_INFO_S
{
    HI_RTSPCLIENT_AUTHEN_E           enAuthenticate;/*authenticate type*/
    HI_CHAR      szUserName[RTSPCLIENT_MAX_USERNAME_LEN];/*user name*/
    HI_CHAR      szPassword[RTSPCLIENT_MAX_PASSWORD_LEN];/*psw info*/
    HI_CHAR      szRealm[RTSPCLIENT_MAX_STR_LEN];/*save for realm info*/
    HI_CHAR      szNonce[RTSPCLIENT_MAX_STR_LEN];/*save for nonce info*/
}HI_RTSPCLIENT_AUTH_INFO_S;

typedef struct hiRTSPCLIENT_PROTOCOL_INFO_S
{
    HI_BOOL     bSetParam;/*setparam pos*/
    HI_BOOL     bGetParam;/*getparam pos*/
    HI_U32       u32InSize;/*recv protocol msg length*/
    HI_U32       u32OutSize;/*send protocol msg length*/
    HI_U32        u32SendSeqNum;/*send seq num*/
    HI_CHAR     szSessionId[RTSPCLIENT_MAX_SESSID_LEN];/*session id for connect*/
    HI_CHAR      szUrl[RTSPCLIENT_MAXURL_LEN];/*url for play*/
    HI_CHAR      szServerIp[RTSPCLIENT_MAXIP_LEN];/*server ip address*/
    HI_CHAR      szInBuffer[RTSPCLIENT_MAXBUFFER_SIZE];/*in protocol msg*/
    HI_CHAR      szOutBuffer[RTSPCLIENT_MAXBUFFER_SIZE];/*out protocol msg*/
    HI_CHAR     szVideoTrackID[RTSPCLIENT_MAXURL_LEN];/*video track id url*/
    HI_CHAR    szAudioTrackID[RTSPCLIENT_MAXURL_LEN];/*audio track id url*/
    HI_CHAR    szUserAgent[RTSPCLIENT_VER_LEN];
}HI_RTSPCLIENT_PROTOCOL_INFO_S;

typedef struct hiRTSPCLIENT_STREAM_S
{
    List_Head_S stStreamListPtr;
    pthread_t ptSessThdId;
    pthread_t ptResponseThdId;/*thread id for response process*/
    pthread_mutex_t    mSendMutex;/*mutext for send protocol msg*/
    HI_RTSP_CLIENT_LISTENER_S stListener;/*rtspclient listener ,state callback*/
    HI_LIVE_STREAM_TYPE_E enStreamType;/*stream type*/
    HI_BOOL   bAudio;/*need set up audio or not*/
    HI_BOOL   bVideo;/*need set up video or not*/
    HI_S32       s32Timeout;/*timeout for connect*/
    HI_S32       s32SvrLisPort;/*server listen port*/
    HI_S32       s32SessSock;/*session socket fd*/
    HI_RTSPCLIENT_STATE_E enClientState;/*client state enum*/
    HI_RTSPCLIENT_METHOD_E enProtocolPos;/*protocol state position*/
    HI_S32*   ps32PrivData;/*private data handle*/
    HI_LIVE_TRANS_TYPE_E enTransType;    /*rtp trans type tcp/udp*/
    HI_RTP_PACK_TYPE_E enPackType; /*data pack type*/
    HI_RTSPCLIENT_AUTH_INFO_S stAuthInfo;/*authenticate info struct*/
    HI_LIVE_STREAM_INFO_S stStreamInfo;/*stream info struct*/
    HI_RTSPCLIENT_PROTOCOL_INFO_S stProtoMsgInfo;/*protocol msg info struct*/
    HI_RTSP_DESCRIBE_S stDescribeInfo;/*describe info*/
    HI_LIVE_ON_RECV pfnOnRecvVideo;      /*call back for recv video data*/
    HI_LIVE_ON_RECV pfnOnRecvAudio;      /*call back for recv audio data*/
    HI_LIVE_ON_EVENT pfnStateListener;          /*call back for envent notify*/
    HI_SETUP_ON_MEDIA_CONTROL pfnOnMediaControl; /*mediao control func*/
    HI_RTP_SESSION_S* pstRtpVideo;/*rtp video*/
    HI_RTP_SESSION_S* pstRtpAudio;/*rtp audio*/
    HI_RTCP_SESSION_S* pstRtcpVideo;/*rtcp video*/
    HI_RTCP_SESSION_S* pstRtcpAudio;/*rtcp audio*/
} HI_RTSPCLIENT_STREAM_S;

HI_S32 RTSPCLIENT_Stream_Pack_Option(HI_RTSPCLIENT_STREAM_S* pstSession);
HI_S32 RTSPCLIENT_Stream_Pack_Describe(HI_RTSPCLIENT_STREAM_S* pstSession);
HI_S32 RTSPCLIENT_Stream_Pack_SetUp(HI_RTSPCLIENT_STREAM_S* pstSession, HI_CHAR* pszTrackUrl, HI_LIVE_STREAM_TYPE_E enStreamType);
HI_S32 RTSPCLIENT_Stream_Pack_Play(HI_RTSPCLIENT_STREAM_S* pstSession);
HI_S32 RTSPCLIENT_Stream_Connect(const HI_CHAR* pchServer, HI_U16 u16Port, HI_S32* p32SessSock);
HI_S32 RTSPCLIENT_Stream_Parse_Url(HI_RTSPCLIENT_STREAM_S* pstSession);
HI_VOID* RTSPCLIENT_Stream_RecvResponseProc(HI_VOID* args);
HI_S32 RTSPCLIENT_Stream_CreateRecvProcess(HI_RTSPCLIENT_STREAM_S* pstSession);
HI_S32 RTSPCLIENT_Stream_DestroyRecvProcess(HI_RTSPCLIENT_STREAM_S* pstSession);






#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /*__RTSP_CLIENT_STREAM_SESSION_H__*/
