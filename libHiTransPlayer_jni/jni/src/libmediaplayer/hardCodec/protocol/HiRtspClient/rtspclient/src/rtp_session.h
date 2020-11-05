/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      hi_rtp_session.h
* @brief     rtspclient rtp session head file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/

#ifndef __RTP_SESSION_H__
#define __RTP_SESSION_H__

#include <pthread.h>
#include "hi_rtsp_client_comm.h"
#include "rtp_packetize.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#define HI_ERR_RTP_MALLOC (1)
#define RTSPCLIENT_MAX_PACKET_BUFF_SIZE (640*1024)
#define RTSPCLIENT_STREAM_VIDEO (0)
#define RTSPCLIENT_STREAM_AUDIO (1)
#define RTSPCLIENT_STREAM_DATA  (2)
#define RTSPCLIENT_STREAM_MAX   (3)
#define RTSPCLIENT_CHAR_MAX_LEN (32)   /*for char[] version picattr length */
#define RTSPCLIENT_UNCARE_LEN (256)    /*for char[] uncare length*/
#define RTSPCLIENT_TRACKID_LEN (128)   /*for max track id length*/
#define RTSPCLIENT_VER_LEN (128)   /*for max ver length*/
#define RTSPCLIENT_MAXURL_LEN (256)   /*for max url length*/
#define RTSPCLIENT_MAXIP_LEN (64)   /*for max url length*/
#define RTSPCLIENT_MAXBUFFER_SIZE   (4096)
#define RTSPCLIENT_RTP_PACKLEN (1500)

typedef struct hiRTP_STATUS_S
{
    HI_U64  u64SendPacket;    /* number of packets send */
    HI_U64  u64SendByte;      /* bytes sent */
    HI_U64  u64SendError;     /* error times when send */
    HI_U64  u64RecvPacket;    /* number of packets received */
    HI_U64  u64RecvByte;      /* bytes of payload received */
    HI_U64  u64Unable;   /* packets not availlable when they were queried */
    HI_U64  u64Bad;           /* packets that did not appear to be RTP */
    HI_U64  u64Discarded;      /* incoming packets discarded because the queue exceeds its max size */
    HI_U64  u64TimeoutCnt;
} HI_RTP_STATUS_S;

typedef enum hiRTP_RUNSTATE_E
{
    HI_RTP_RUNSTATE_INIT  = 0,
    HI_RTP_RUNSTATE_READY,
    HI_RTP_RUNSTATE_RUNNING,
    HI_RTP_RUNSTATE_STOP,
    HI_RTP_RUNSTATE_DELINIT,
    HI_RTP_RUNSTATE_BUTT
} HI_RTP_RUNSTATE_E;

typedef struct hiRTSP_DESCRIBE_S
{
    HI_LIVE_VIDEO_TYPE_E   enVideoFormat;
    HI_LIVE_AUDIO_TYPE_E   enAudioFormat;
    HI_S32              as32PayloadType[RTSPCLIENT_STREAM_MAX];
    HI_S32              as32RtpChn[RTSPCLIENT_STREAM_MAX];
} HI_RTSP_DESCRIBE_S;

typedef struct hiRTP_SESSION_S
{
    pthread_t         ptSessThdId;       /* thread to recv cmd */
    HI_S32              s32ListenPort;
    HI_S32    s32StreamSock;
    HI_S32    s32SessSock;
    HI_RTP_PT_E          enPayloadType;           /*payload type*/
    HI_CHAR          szURL[RTSPCLIENT_MAXURL_LEN];
    HI_U16              u16LastSn;      /*last recv sn*/
    HI_U32              u32LastTs;      /*last recv sn*/
    HI_RTP_STATUS_S         stRtpStatus;          /*stats*/
    HI_LIVE_ON_RECV      pfnRecvVideo;  /*video callbak fun*/
    HI_LIVE_ON_RECV      pfnRecvAudio;  /*audio callbak fun*/
    HI_RTP_RUNSTATE_E      enRunState;
    HI_LIVE_TRANS_TYPE_E enTransType;
    HI_RTP_PACK_TYPE_E enStreamPackType;  /*packettype*/
    HI_S32 s32RecvPort;
    HI_RTSP_DESCRIBE_S     stDescribeInfo;
    HI_LIVE_STREAM_INFO_S stStreamInfo;
    HI_U8*  pu8RtpBuff;/*buf for recv rtp data*/
    HI_U32  u32RtpBuffLen;/*buf len for recv rtp data*/
    HI_S32*  ps32ExtArgs;
} HI_RTP_SESSION_S;

/**
 * @brief get stream info.
 * @param[in] pstSession  const HI_RTP_SESSION_S* : rtp session struct
 * @param[in] pu8Buff const HI_U8* : in dada buffer
 * @param[in,out] penStreamType HI_LIVE_STREAM_TYPE_E* : stream type
 * @param[in,out] pu32PayloadLen HI_U32* : rtp head info struct
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 RTP_Session_GetStreamInfo(HI_RTP_SESSION_S* pstSession, HI_U8* pu8Buff,
                                HI_LIVE_STREAM_TYPE_E* penStreamType, HI_U32* pu32PayloadLen);

/**
 * @brief get head info in rtspITLV.
 * @param[in] pstSession  const HI_RTP_SESSION_S* : rtp session struct
 * @param[in] pu8Buff const HI_U8* : in dada buffer
 * @param[in] enStreamType HI_LIVE_STREAM_TYPE_E : stream type
 * @param[in,out] pstRtpHeadInfo HI_LIVE_RTP_HEAD_INFO_S* : rtp head info struct
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 RTP_Session_GetHeadInfo_RTSPITLV( const HI_RTP_SESSION_S* pstSession,const HI_U8* pu8Buff,
                                                                                                            HI_LIVE_STREAM_TYPE_E enStreamType,HI_LIVE_RTP_HEAD_INFO_S* pstRtpHeadInfo);

/**
 * @brief get head info in rtp packet type.
 * @param[in] pstSession  const HI_RTP_SESSION_S* : rtp session struct
 * @param[in] pu8Buff const HI_U8* : in dada buffer
 * @param[in] enStreamType HI_LIVE_STREAM_TYPE_E : stream type
 * @param[in,out] pstRtpHeadInfo HI_LIVE_RTP_HEAD_INFO_S* : rtp head info struct
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 RTP_Session_GetHeadInfo_RTP( const HI_RTP_SESSION_S* pstSession, const HI_U8* pu8Buff,
                                                                                                            HI_LIVE_STREAM_TYPE_E enStreamType,HI_LIVE_RTP_HEAD_INFO_S* pstRtpHeadInfo);

/**
 * @brief stop rtp session normal or abnormal by enPlayState.
 * @param[in,out] pstSession HI_RTP_SESSION_S* : pstSession struct
 * @return   0 success
 * @return  -1 failure
 */
HI_S32  RTP_Session_Stop(HI_RTP_SESSION_S* pstSession);

/**
 * @brief start rtp session.
 * @param[in] pstSession HI_RTP_SESSION_S* : pstSession struct
 * @return   0 success
 * @return  -1 failure
 */
HI_S32  RTP_Session_Start(HI_RTP_SESSION_S* pstSession);

/**
 * @brief create rtp session.
 * @param[in,out] ppstSession HI_RTP_SESSION_S** : pstSession struct
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 RTP_Session_Create(HI_RTP_SESSION_S** ppstSession);

/**
 * @brief destroy rtp session.
 * @param[in] pstSession HI_RTP_SESSION_S* : pstSession struct
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 RTP_Session_Destroy(HI_RTP_SESSION_S* pstSession);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif/*__RTP_SESSION_H__*/
