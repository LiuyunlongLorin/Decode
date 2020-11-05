/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      rtp_session.c
* @brief     rtspclient rtp session src file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <string.h>
#include "hi_rtsp_client.h"
#include "hi_rtsp_client_err.h"
#include "rtsp_client_log.h"
#include "rtsp_client_stream.h"
#include "rtsp_client_socket.h"
#include "rtsp_client_msgparser.h"
#include "rtp_session.h"
#define RTP_RECV "LIVE_CLIENT_RECV"
#define RTP_SLICE_END (0X40)
#define RTP_SLICE_START (0X80)
#define RTP_H264_NAL_MASK (0x1f)
#define RTP_H265_NAL_MASK (0x7f)
#define RTP_NAL_MASK_LEFT_THREE (0xe0)
#define RTP_NAL_MASK_LEFT_TWO  (0xc0)
#define RTP_NAL_MASK_RIGHT_SIX (0x3f)
#define RTP_NAL_MASK_MID_SIX (0x7e)
#define RTP_H265_NAL_FUA (0x62)
#define RTP_H264_NAL_FUA (0x1c)
#define RTP_H264_NAL_MIN (0x17)
#define RTP_H264_NAL_MAX (0x1e)


static HI_S32 RTP_Session_SetRecvThdState(HI_RTP_SESSION_S* pstSession , HI_RTP_RUNSTATE_E enRunState)
{
    if (NULL == pstSession  || enRunState > HI_RTP_RUNSTATE_BUTT)
    {
        return HI_FAILURE;
    }

    pstSession->enRunState = enRunState;

    return HI_SUCCESS;
}

/*NAL definit:
      Type   Packet      Type name
      ---------------------------------------------------------
      0      undefined                                    -
      1-23   NAL unit    Single NAL unit packet per H.264
      24     STAP-A      Single-time aggregation packet
      25     STAP-B      Single-time aggregation packet
      26     MTAP16      Multi-time aggregation packet
      27     MTAP24      Multi-time aggregation packet
      28     FU-A        Fragmentation unit
      29     FU-B        Fragmentation unit
      30-31  undefined
*/

static HI_S32 RTP_Session_AvcNALHeaderAnalyze(const HI_U8* pu8Src, HI_U8* pu8SliceType, HI_U8* pNalHead)
{
    HI_U8 u8Head1 = *pu8Src;/*get the first byte*/
    HI_U8 u8Head2 = *(pu8Src + 1);
    HI_U8 u8Type = u8Head1 & RTP_H264_NAL_MASK;  /*get the fu indicator*/
    HI_U8 flag;

    if ( RTP_H264_NAL_FUA == u8Type) /*0x1c=28 FU-A*/
    {
        *pNalHead = (u8Head1 & RTP_NAL_MASK_LEFT_THREE) | (u8Head2 & RTP_H264_NAL_MASK);
        flag = u8Head2 & RTP_NAL_MASK_LEFT_THREE;/*check the start middle end slice*/

        if (RTP_SLICE_START == flag) /*start slice*/
        {
            SET_START_SLICE(*pu8SliceType);
        }
        else if (RTP_SLICE_END == flag) /*end slice*/
        {
            SET_END_SLICE(*pu8SliceType);
        }
        else/*middle slice*/
        {
            SET_NORMAL_SLICE(*pu8SliceType);
        }

        return HI_SUCCESS;
    }
    else if ( RTP_H264_NAL_MIN >= u8Type || RTP_H264_NAL_MAX <= u8Type)
    {
        *pNalHead = u8Head1;
        SET_START_SLICE(*pu8SliceType);
        SET_END_SLICE(*pu8SliceType);
        return HI_SUCCESS;
    }
    return HI_FAILURE;
}

static HI_S32 RTP_Session_HevcNALHeaderAnalyze(const HI_U8* pu8Src, HI_U8* pu8SliceType, HI_U8* pNalHead, HI_U8* pu8LayerId)
{
    HI_U8 u8Head1 = *pu8Src;/*get the first byte*/
    *pu8LayerId = *(pu8Src + 1);
    HI_U8 u8Head3 = *(pu8Src + 2);
    HI_U8 u8Type = u8Head1 & RTP_NAL_MASK_MID_SIX;
    HI_U8 flag;

    if ( RTP_H265_NAL_FUA == u8Type) /*FUA     0x62= 49<<1*/
    {
        *pNalHead = (u8Head3 & RTP_NAL_MASK_RIGHT_SIX);/*get last 6 bytes for NALUtype*/
        flag = u8Head3 & RTP_NAL_MASK_LEFT_TWO;

        if ( RTP_SLICE_START  == flag) /*start*/
        {
            SET_START_SLICE(*pu8SliceType);
        }
        else if (RTP_SLICE_END == flag) /*end*/
        {
            SET_END_SLICE(*pu8SliceType);
        }
        else/*middle*/
        {
            SET_NORMAL_SLICE(*pu8SliceType);
        }
    }
    else /*one slice one frame*/
    {
        *pNalHead = u8Head1 >> 1;
        SET_START_SLICE(*pu8SliceType);
        SET_END_SLICE(*pu8SliceType);
    }

    return HI_SUCCESS;
}

/*
H264 P frame  0000000161/00000161,
hsinet HISI_ITLEAVED_HDR_S£© + 0000000161/00000161
RTSP £¨RTSP_ITLEAVED_RTP_HEAD_S£©+ 0161

264 nalu type byte
                  0 1 2 3 4 5 6 7 8 9 0
                +-+-+-+-+-+-+-+-
                 |F|NRI|  Type   |
                 +-------------+--

265 nalu type byte
                 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
                +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                 |F|   Type    |  LayerId  | TID |
                 +-------------+-----------------+
*/
static HI_S32 RTP_Session_GetNalTypeAVC(HI_LIVE_RTP_HEAD_INFO_S* pstRtpHeadInfo, HI_U8 u8NalHead)
{
    switch (u8NalHead & RTP_H264_NAL_MASK)
    {
        case HI_LIVE_H264_NALU_SPS:
            pstRtpHeadInfo->unFrameType.enH264NaluType = HI_LIVE_H264_NALU_SPS;
            break;

        case HI_LIVE_H264_NALU_PPS:
            pstRtpHeadInfo->unFrameType.enH264NaluType = HI_LIVE_H264_NALU_PPS;
            break;

        case HI_LIVE_H264_NALU_SEI:
            pstRtpHeadInfo->unFrameType.enH264NaluType = HI_LIVE_H264_NALU_SEI;
            break;

        case HI_LIVE_H264_NALU_IDR:
            pstRtpHeadInfo->unFrameType.enH264NaluType = HI_LIVE_H264_NALU_IDR;
            break;

        case HI_LIVE_H264_NALU_PSLICE:
            pstRtpHeadInfo->unFrameType.enH264NaluType = HI_LIVE_H264_NALU_PSLICE;
            break;

        default:
            pstRtpHeadInfo->unFrameType.enH264NaluType = HI_LIVE_H264_NALU_BUTT;
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "Unknow  NaluType  %d  \n", u8NalHead);
            break;
    }
    return HI_SUCCESS;
}

static HI_S32 RTP_Session_GetNalTypeHEVC(HI_LIVE_RTP_HEAD_INFO_S* pstRtpHeadInfo, HI_U8 u8NalHead)
{
    switch (u8NalHead & RTP_H265_NAL_MASK) /*get last 7 bits after right move 1 bit*/
    {
        case HI_LIVE_H265_NALU_VPS:
            pstRtpHeadInfo->unFrameType.enH265NaluType = HI_LIVE_H265_NALU_VPS;
            break;

        case HI_LIVE_H265_NALU_SPS:
            pstRtpHeadInfo->unFrameType.enH265NaluType = HI_LIVE_H265_NALU_SPS;
            break;

        case HI_LIVE_H265_NALU_PPS:
            pstRtpHeadInfo->unFrameType.enH265NaluType = HI_LIVE_H265_NALU_PPS;
            break;

        case HI_LIVE_H265_NALU_SEI:
            pstRtpHeadInfo->unFrameType.enH265NaluType = HI_LIVE_H265_NALU_SEI;
            break;

        case HI_LIVE_H265_NALU_IDR:
            pstRtpHeadInfo->unFrameType.enH265NaluType = HI_LIVE_H265_NALU_IDR;
            break;

        case HI_LIVE_H265_NALU_PSLICE:
            pstRtpHeadInfo->unFrameType.enH265NaluType = HI_LIVE_H265_NALU_PSLICE;
            break;

        default:
            pstRtpHeadInfo->unFrameType.enH265NaluType = HI_LIVE_H265_NALU_BUTT;
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "~Unknow  NaluType  %d ~\n", u8NalHead);
            break;
    }
    return HI_SUCCESS;
}

HI_S32 RTP_Session_GetStreamType(HI_RTP_SESSION_S* pstSession, HI_RTP_PT_E enPayloadFormat, HI_LIVE_STREAM_TYPE_E* penStreamType)
{
    /*get stream type by payloadtype*/
    if ((HI_RTP_PT_E)pstSession->stDescribeInfo.as32PayloadType[RTSPCLIENT_STREAM_VIDEO] == enPayloadFormat)
    {
        *penStreamType = HI_LIVE_STREAM_TYPE_VIDEO;
    }
    else if ((HI_RTP_PT_E)pstSession->stDescribeInfo.as32PayloadType[RTSPCLIENT_STREAM_AUDIO] == enPayloadFormat)
    {
        *penStreamType = HI_LIVE_STREAM_TYPE_AUDIO;
    }
    else
    {
        /*get media type by payloadtype*/
        if (HI_RTP_PT_H264 == enPayloadFormat || HI_RTP_PT_H265 == enPayloadFormat)
        {
            *penStreamType = HI_LIVE_STREAM_TYPE_VIDEO;
        }
        else if (   HI_RTP_PT_G726 == enPayloadFormat || HI_RTP_PT_PCMA == enPayloadFormat
                    || HI_RTP_PT_PCMU == enPayloadFormat || HI_RTP_PT_AMR == enPayloadFormat
                    || HI_RTP_PT_ADPCM == enPayloadFormat || HI_RTP_PT_AAC == enPayloadFormat)
        {
            *penStreamType = HI_LIVE_STREAM_TYPE_AUDIO;
        }
        else if (HI_RTP_PT_DATA == enPayloadFormat)
        {
            *penStreamType = HI_LIVE_STREAM_TYPE_DATA;
        }
        else
        {
            *penStreamType = HI_LIVE_STREAM_TYPE_BUTT;
            return HI_FAILURE;
        }
    }

    return HI_SUCCESS;
}
/*outer file interface*/
HI_S32 RTP_Session_GetStreamInfo(HI_RTP_SESSION_S* pstSession, HI_U8* pu8Buff,
                                 HI_LIVE_STREAM_TYPE_E* penStreamType, HI_U32* pu32PayloadLen)
{
    HI_S32 s32HeadLen = 0;          /*header length*/
    HI_RTP_PT_E enPayloadFormat = HI_RTP_PT_INVALID;   /*payload format*/
    RTSP_ITLEAVED_RTP_HEAD_S* pstRtspItlHdr = NULL;
    HI_RTP_HEAD_S* pstRtpHdr = NULL;
    HI_S32 s32Ret = 0;

    if (NULL == penStreamType || NULL == pu32PayloadLen)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR,  "RTP_Session_GetStreamInfo param null\r\n");
        return HI_FAILURE;
    }

    /*if head is rtsp interleaved head, then extract payloadLen*/
    if (HI_PACK_TYPE_RTSP_ITLV == pstSession->enStreamPackType)
    {
        s32HeadLen = sizeof(RTSP_ITLEAVED_RTP_HEAD_S);
        pstRtspItlHdr = (RTSP_ITLEAVED_RTP_HEAD_S*)pu8Buff;
        /*read data len, not including rtp head*/
        *pu32PayloadLen = ntohs(pstRtspItlHdr->stItlvHead.u16Length) - sizeof(HI_RTP_HEAD_S);
        pstRtpHdr = (HI_RTP_HEAD_S*)(pu8Buff + s32HeadLen - sizeof(HI_RTP_HEAD_S));  /*get rtp head ptr*/
    }
    else if (HI_PACK_TYPE_RTP == pstSession->enStreamPackType)
    {
        pstRtpHdr = (HI_RTP_HEAD_S*)pu8Buff;
    }
    else
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR,  "unsupport stream pack type \r\n");
        return HI_FAILURE;
    }

    if (NULL == pstRtpHdr)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR,  "pRtpHdr  null\n");
        return HI_FAILURE;
    }
    /*extract stream type : video ,audio or other data*/
    enPayloadFormat = (HI_RTP_PT_E)pstRtpHdr->pt;
    s32Ret = RTP_Session_GetStreamType(pstSession, enPayloadFormat, penStreamType);
    if (HI_SUCCESS != s32Ret)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR,  "RTP_Session_GetStreamType  fail\n");
        return HI_FAILURE;
    }
    return s32Ret;

}

HI_S32 RTP_Session_GetHeadInfo_RTSPITLV( const HI_RTP_SESSION_S* pstSession,
        const HI_U8* pu8Buff,
        HI_LIVE_STREAM_TYPE_E enStreamType,
        HI_LIVE_RTP_HEAD_INFO_S* pstRtpHeadInfo)
{
    HI_U32 u32HeadLen = 0;                    /*header length*/
    RTSP_ITLEAVED_RTP_HEAD_S* pRtspItlHdr = NULL;
    HI_U8  u8NalHead = 0x00;
    HI_U32 extensiondata = 0;
    HI_U32 extensionLength = 0;

    if (NULL == pu8Buff || NULL == pstSession)
    {
        return HI_FAILURE;
    }

    u32HeadLen = sizeof(RTSP_ITLEAVED_RTP_HEAD_S);
    pRtspItlHdr = (RTSP_ITLEAVED_RTP_HEAD_S*)pu8Buff;
    pstRtpHeadInfo->u32TimeStamp = ntohl(pRtspItlHdr->stRtpHead.ts);
    pstRtpHeadInfo->u32Seqno = ntohs(pRtspItlHdr->stRtpHead.seqno);
    CLEAR_SLICE(pstRtpHeadInfo->u8SliceType);

    if (1 == pRtspItlHdr->stRtpHead.x)
    {
        extensiondata = ntohl(*(HI_U32*)(pu8Buff + sizeof(RTSP_ITLEAVED_RTP_HEAD_S)));
        extensionLength = 4 * (extensiondata & 0xffff);
        u32HeadLen += extensionLength + 4;
    }

    if (HI_LIVE_STREAM_TYPE_VIDEO == enStreamType)
    {
        if (HI_LIVE_VIDEO_H264 == pstSession->stDescribeInfo.enVideoFormat)
        {
            if (HI_SUCCESS != RTP_Session_AvcNALHeaderAnalyze((HI_U8*)&pu8Buff[u32HeadLen], &pstRtpHeadInfo->u8SliceType, &u8NalHead))
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "Unknow ITLV NAL Header:%x,%x\n", pu8Buff[u32HeadLen], pu8Buff[u32HeadLen + 1]);
                return HI_FAILURE;
            }
            pstRtpHeadInfo->u8NaluType = u8NalHead;

            RTP_Session_GetNalTypeAVC(pstRtpHeadInfo, u8NalHead);

            if (1 == pRtspItlHdr->stRtpHead.marker)
            {
                SET_END_SLICE(pstRtpHeadInfo->u8SliceType);
            }

        }
        else if (HI_LIVE_VIDEO_H265 == pstSession->stDescribeInfo.enVideoFormat)
        {
            HI_U8 u8LayerID = 0;

            RTP_Session_HevcNALHeaderAnalyze((HI_U8*)&pu8Buff[u32HeadLen], &pstRtpHeadInfo->u8SliceType, &u8NalHead, &u8LayerID);

            pstRtpHeadInfo->u8NaluType = u8NalHead << 1;
            pstRtpHeadInfo->u8LayerID = u8LayerID;
            RTP_Session_GetNalTypeHEVC(pstRtpHeadInfo, u8NalHead);

            if (1 == pRtspItlHdr->stRtpHead.marker)
            {
                SET_END_SLICE(pstRtpHeadInfo->u8SliceType);
            }
        }
        else
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "~~Unknow HI_PACK_TYPE_RTSP_ITLV  enPayloadFormat~~\n");
            return HI_FAILURE;
        }
    }
    else if (HI_LIVE_STREAM_TYPE_AUDIO == enStreamType)
    {
        SET_END_SLICE(pstRtpHeadInfo->u8SliceType);
        u32HeadLen = u32HeadLen - sizeof(HI_RTP_HEAD_S);
    }
    else
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "~~Unknow HI_PACK_TYPE_RTSP_ITLV StreamType\n");
        return HI_FAILURE;
    }

    pstRtpHeadInfo->u32HeadLen = u32HeadLen;

    return HI_SUCCESS;
}

HI_S32 RTP_Session_GetHeadInfo_RTP( const HI_RTP_SESSION_S* pstSession,
                                    const HI_U8* pu8Buff,
                                    HI_LIVE_STREAM_TYPE_E enStreamType,
                                    HI_LIVE_RTP_HEAD_INFO_S* pstRtpHeadInfo)

{
    HI_U32 u32HeadLen = 0;                    /*header length*/
    HI_RTP_HEAD_S* pstRtpHdr = NULL;
    HI_U8  u8NalHead = 0x00;

    if (NULL == pu8Buff || NULL == pstSession)
    {
        return HI_FAILURE;
    }

    u32HeadLen = sizeof(HI_RTP_HEAD_S);
    pstRtpHdr = (HI_RTP_HEAD_S*)pu8Buff;
    pstRtpHeadInfo->u32TimeStamp = ntohl(pstRtpHdr->ts);
    pstRtpHeadInfo->u32Seqno = ntohl(pstRtpHdr->seqno);
    CLEAR_SLICE(pstRtpHeadInfo->u8SliceType);

    if (HI_LIVE_STREAM_TYPE_VIDEO == enStreamType)
    {
        if (HI_LIVE_VIDEO_H264 == pstSession->stDescribeInfo.enVideoFormat)
        {
            if (HI_SUCCESS != RTP_Session_AvcNALHeaderAnalyze((HI_U8*)&pu8Buff[u32HeadLen], &pstRtpHeadInfo->u8SliceType, &u8NalHead))
            {
                RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "~Unknow RTP NAL Header:%x,%x~\n", pu8Buff[u32HeadLen], pu8Buff[u32HeadLen + 1]);
                return HI_FAILURE;
            }

            RTP_Session_GetNalTypeAVC(pstRtpHeadInfo, u8NalHead);

            if (!(CHECK_START_SLICE(pstRtpHeadInfo->u8SliceType)
                  && CHECK_END_SLICE(pstRtpHeadInfo->u8SliceType)))
            {
                u32HeadLen += 2;
            }

            if (1 == pstRtpHdr->marker)
            {
                SET_END_SLICE(pstRtpHeadInfo->u8SliceType);
            }
        }
        else if (HI_LIVE_VIDEO_H265 == pstSession->stDescribeInfo.enVideoFormat)
        {
            HI_U8 u8LayerID = 0;

            RTP_Session_HevcNALHeaderAnalyze((HI_U8*)&pu8Buff[u32HeadLen], &pstRtpHeadInfo->u8SliceType, &u8NalHead, &u8LayerID);

            pstRtpHeadInfo->u8NaluType = u8NalHead << 1;
            pstRtpHeadInfo->u8LayerID = u8LayerID;

            RTP_Session_GetNalTypeHEVC(pstRtpHeadInfo, u8NalHead);

            if (!(CHECK_START_SLICE(pstRtpHeadInfo->u8SliceType)
                  && CHECK_END_SLICE(pstRtpHeadInfo->u8SliceType))) /*not one slice one frame type*/
            {
                u32HeadLen += 3;
            }

            if (1 == pstRtpHdr->marker)
            {
                SET_END_SLICE(pstRtpHeadInfo->u8SliceType);
            }
        }
        else
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "~~Unknow  HI_PACK_TYPE_RTP video enPayloadFormat ~~\n");
            return HI_FAILURE;
        }
    }
    else if (HI_LIVE_STREAM_TYPE_AUDIO == enStreamType)
    {
        SET_END_SLICE(pstRtpHeadInfo->u8SliceType);
    }
    else
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "~~Unknow  HI_PACK_TYPE_RTP StreamType ~~\n");
        return HI_FAILURE;
    }

    pstRtpHeadInfo->u32HeadLen = u32HeadLen;

    return HI_SUCCESS;

}

/* start recv process */
HI_S32 RTP_Session_Start(HI_RTP_SESSION_S* pstSession)
{
    HI_S32 s32Ret = HI_SUCCESS;

    if (NULL == pstSession)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTP_Session_Start param error\r\n");
        return HI_ERR_RTSPCLIENT_ILLEGAL_PARAM;
    }

    if (HI_LIVE_TRANS_TYPE_UDP == pstSession->enTransType)
    {
        pstSession->enRunState    = HI_RTP_RUNSTATE_INIT;
        pstSession->ptSessThdId = RTSPCLIENT_INVALID_THD_ID;
        s32Ret = RTSPCLIENT_SOCKET_UDPRecv(&pstSession->s32StreamSock, pstSession->s32ListenPort);
        if (HI_SUCCESS != s32Ret)
        {
            RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_SOCKET_UDPRecv  error\r\n");
            return HI_FAILURE;
        }
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

HI_S32 RTP_Session_Stop(HI_RTP_SESSION_S* pstSession)
{
    if (NULL == pstSession)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTP_Session_Stop param null\r\n");
        return HI_ERR_RTSPCLIENT_ILLEGAL_PARAM;
    }

    RTP_Session_SetRecvThdState(pstSession, HI_RTP_RUNSTATE_INIT);

    if ( HI_LIVE_TRANS_TYPE_UDP == pstSession->enTransType )
    {
        HI_CLOSE_SOCKET(pstSession->s32StreamSock);
    }

    return HI_SUCCESS;

}

HI_S32 RTP_Session_Create(HI_RTP_SESSION_S** ppstSession)
{
    HI_RTP_SESSION_S* pstSession = NULL;

    if ( NULL == ppstSession )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTP_Session_Create param null\r\n");
        return HI_ERR_RTSPCLIENT_ILLEGAL_PARAM;
    }

    pstSession = (HI_RTP_SESSION_S*)malloc(sizeof(HI_RTP_SESSION_S));
    if (NULL == pstSession)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "malloc for RTP SESSION error\r\n");
        return HI_FAILURE;
    }
    memset(pstSession, 0 , sizeof(HI_RTP_SESSION_S));

    pstSession->pu8RtpBuff = (HI_U8*)malloc(RTSPCLIENT_RTP_PACKLEN);
    if (!pstSession->pu8RtpBuff)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "malloc for pstSession->pu8RtpBuff error\r\n");
        free(pstSession);
        pstSession = NULL;
        return HI_FAILURE;
    }
    memset(pstSession->pu8RtpBuff,0x00,RTSPCLIENT_RTP_PACKLEN);
    pstSession->u32RtpBuffLen = RTSPCLIENT_RTP_PACKLEN;
    *ppstSession = pstSession;
    return HI_SUCCESS;

}

HI_S32 RTP_Session_Destroy(HI_RTP_SESSION_S* pstSession)
{
    if (NULL == pstSession)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTP_Session_Destroy NULL param  error\r\n");
        return HI_ERR_RTSPCLIENT_ILLEGAL_PARAM;
    }

    if (NULL != pstSession->pu8RtpBuff)
    {
        free(pstSession->pu8RtpBuff);
        pstSession->pu8RtpBuff = NULL;
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
