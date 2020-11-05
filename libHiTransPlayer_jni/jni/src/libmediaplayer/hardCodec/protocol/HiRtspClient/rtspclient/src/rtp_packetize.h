/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      hi_rtp_packetize.h
* @brief     rtspclient rtp packetize head file,define of  structs for rtp
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/

#ifndef __RTP_PACKETIZE_H__
#define __RTP_PACKETIZE_H__

#include <netinet/in.h>
#include "hi_type.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

/*define the byte order,pair for server*/
#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif
#define RTP_TRANS_STREAM_VIDEO (0)
#define RTP_TRANS_STREAM_AUDIO (1)
#define RTP_TRANS_STREAM_DATA  (2)
#define RTP_TRANS_STREAM_MAX   (3)
#define RTP_TRANS_IP_MAX_LEN (32)

/*RTP Payload type define rfc3551*/
typedef enum hiRTP_PT_E
{
    HI_RTP_PT_PCMU             = 0,        /* mu-law G.711Mu*/
    HI_RTP_PT_GSM              = 3,        /* GSM */
    HI_RTP_PT_G723             = 4,        /* G.723 */
    HI_RTP_PT_PCMA             = 8,        /* a-law G.711A*/
    HI_RTP_PT_G722             = 9,        /* G.722 */
    HI_RTP_PT_L16_STEREO       = 10,       /* linear 16, 44.1khz, 2 channel */
    HI_RTP_PT_L16_MONO         = 11,       /* linear 16, 44.1khz, 1 channel */
    HI_RTP_PT_MPA              = 14,       /* mpeg audio */
    HI_RTP_PT_JPEG             = 26,       /* jpeg */
    HI_RTP_PT_H261             = 31,       /* h.261 */
    HI_RTP_PT_MPV              = 32,       /* mpeg video */
    HI_RTP_PT_MP2T             = 33,       /* mpeg2 TS stream */
    HI_RTP_PT_H263             = 34,       /* old H263 encapsulation */
    /*96-127 dynamic*/
    HI_RTP_PT_H264             = 96,       /* hisilicon define as h.264 */
    HI_RTP_PT_G726             = 97,       /* hisilicon define as G.726 */
    HI_RTP_PT_H265             = 98,       /* hisilicon define as h.265 */
    HI_RTP_PT_DATA             = 100,      /* hisilicon define as md alarm data*/
    HI_RTP_PT_AMR              = 101,      /* hisilicon define as AMR*/
    HI_RTP_PT_MJPEG            = 102,      /* hisilicon define as MJPEG */
    HI_RTP_PT_YUV              = 103,      /* hisilicon define as YUV*/
    HI_RTP_PT_ADPCM            = 104,      /* hisilicon define as ADPCM */
    HI_RTP_PT_AAC              = 105,      /* hisilicon define as AAC */
    HI_RTP_PT_SEC               = 106,  /* hisilicon define as SEC*/
    HI_RTP_PT_RED              = 107,  /* hisilicon define as RED */
    HI_RTP_PT_INVALID          = 128
} HI_RTP_PT_E;
/*pack type*/
typedef enum hiRTP_PACK_TYPE_E
{
    HI_PACK_TYPE_RAW = 0,
    HI_PACK_TYPE_RTP,     /*normal rtp ,head 12 bytes*/
    HI_PACK_TYPE_RTP_STAP,/* STAP-A add 3 byte head ,head is 15 bytes now*/
    HI_PACK_TYPE_RTP_FUA, /*FU-A ,data add 2 type head*/
    HI_PACK_TYPE_RTSP_ITLV, /*rtsp interleaved  add 4 byte head before rtp head*/
    HI_PACK_TYPE_HISI_ITLV, /*hisi interleaved add 8 byte head before rtp head*/
    HI_PACK_TYPE_RTSP_O_HTTP,
    HI_PACK_TYPE_BUTT
} HI_RTP_PACK_TYPE_E;

typedef enum hiRTP_TRANS_MODE_E
{
    HI_RTP_TRANS_UDP = 0,
    HI_RTP_TRANS_UDP_ITLV,
    HI_RTP_TRANS_TCP,
    HI_RTP_TRANS_TCP_ITLV,
    HI_RTP_TRANS_BROADCAST,
    HI_RTP_TRANS_BUTT
} HI_RTP_TRANS_MODE_E;

/* total 12Bytes */
typedef struct hiRTP_HEAD_S
{
#if (BYTE_ORDER == LITTLE_ENDIAN)
    /*byte 0  */
    HI_U16 cc      : 4;  /* CSRC count */
    HI_U16 x       : 1;  /* header extension flag */
    HI_U16 p       : 1;  /* padding flag */
    HI_U16 version : 2;  /* protocol version */
    /* byte 1 */
    HI_U16 pt      : 7;  /* payload type */
    HI_U16 marker  : 1;  /* marker bit */
#elif (BYTE_ORDER == BIG_ENDIAN)
    /* byte 0 */
    HI_U16 version : 2;  /* protocol version */
    HI_U16 p       : 1;  /* padding flag */
    HI_U16 x       : 1;  /* header extension flag */
    HI_U16 cc      : 4;  /* CSRC count */
    /*byte 1*/
    HI_U16 marker  : 1;  /* marker bit */
    HI_U16 pt      : 7;  /* payload type */
#else
#error YOU MUST DEFINE BYTE_ORDER == LITTLE_ENDIAN OR BIG_ENDIAN !
#endif
    HI_U16 seqno  : 16;  /*  bytes 2, 3 sequence number */
    HI_U32 ts;            /*bytes 4-7 timestamp in ms */
    HI_U32 ssrc;          /* bytes 8-11 synchronization source */
} HI_RTP_HEAD_S;

/* RTCP head struct total 12Bytes */
typedef struct hiRTCP_HEAD_S
{
    /* byte 0 */
    HI_U16 version :2;   /* protocol version */
    HI_U16 p       :1;   /* padding flag */
    HI_U16 rc      :5;   /* count */
    HI_U16 pt      :8;   /* byte 1 payload type */
    HI_U16 length  :16;  /* bytes 2, 3  length */
} HI_RTCP_HEAD_S;

/*rtsp interleaved head*/
typedef struct hiRTSP_ITLEAVED_HEAD_S
{
    HI_U8  u8Magic;      /*8, $:dollar sign(24 decimal)*/
    HI_U8  u8Channel;    /*8, channel id*/
    HI_U16 u16Length;     /*16, payload length*/
}HI_RTSP_ITLEAVED_HEAD_S;

/*rtp interleaved head*/
typedef struct hiRTSP_ITLEAVED_RTP_HEAD_S
{
    HI_RTSP_ITLEAVED_HEAD_S stItlvHead;
    HI_RTP_HEAD_S stRtpHead;   /*rtp head*/
}RTSP_ITLEAVED_RTP_HEAD_S;

/*rtcp interleaved head*/
typedef struct hiRTSP_ITLEAVED_RTCP_HEAD_S
{
    HI_RTSP_ITLEAVED_HEAD_S stItlvHead;
    HI_RTCP_HEAD_S stRtcpHead;   /*rtcp head*/
}HI_RTSP_ITLEAVED_RTCP_HEAD_S;

/*rtp field packet*/
#define RTP_HEAD_SET_VERSION(pHDR, val)  ((pHDR)->version = val)
#define RTP_HEAD_SET_P(pHDR, val)        ((pHDR)->p       = val)
#define RTP_HEAD_SET_X(pHDR, val)        ((pHDR)->x       = val)
#define RTP_HEAD_SET_CC(pHDR, val)       ((pHDR)->cc      = val)
#define RTP_HEAD_SET_M(pHDR, val)        ((pHDR)->marker  = val)
#define RTP_HEAD_SET_PT(pHDR, val)       ((pHDR)->pt      = val)
#define RTP_HEAD_SET_SEQNO(pHDR, _sn)    ((pHDR)->seqno  = (_sn))
#define RTP_HEAD_SET_TS(pHDR, _ts)       ((pHDR)->ts     = (_ts))
#define RTP_HEAD_SET_SSRC(pHDR, _ssrc)    ((pHDR)->ssrc  = _ssrc)
#define RTP_HDR_LEN  sizeof(HI_RTP_HEAD_S)
#define RTP_DEFAULT_SSRC 41030

#if (BYTE_ORDER == LITTLE_ENDIAN)
typedef struct hiFU_INDICATOR_S
{
    HI_U8 forbit    : 1;
    HI_U8 NRI       : 2;
    HI_U8 type      : 5;
} HI_FU_INDICATOR_S;

typedef struct hiFU_HEADER_S
{
    HI_U8 start     : 1;
    HI_U8 end       : 1;
    HI_U8 reserve   : 1;
    HI_U8 type      : 5;
} HI_FU_HEADER_S;

#else/*BIG_ENDIAN order*/
typedef struct hiFU_INDICATOR_S
{
    HI_U8 type          : 5;
    HI_U8   NRI         : 2;
    HI_U8   forbit      : 1;
} HI_FU_INDICATOR_S;

typedef struct hiFU_HEADER_S
{
    HI_U8   type            : 5;
    HI_U8   reserve      : 1;
    HI_U8   end              : 1;
    HI_U8   start            : 1;
} HI_FU_HEADER_S;

#endif

typedef enum hiFU_TYPE_E
{
    HI_FU_TYPE_A = 28,
    HI_FU_TYPE_B = 29
} HI_FU_TYPE_E;

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /*__RTP_PACKETIZE_H__*/
