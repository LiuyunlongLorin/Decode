/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      hi_rtsp_client_comm.h
* @brief     rtsp client  module header file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/

#ifndef __HI_RTSP_CLIENT_COMM_H__
#define __HI_RTSP_CLIENT_COMM_H__


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */
#include "hi_mw_type.h"

#define RTSPCLIENT_INVALID_THD_ID  ((pthread_t)-1)
#define RTSPCLIENT_MAX_USERNAME_LEN (64)
#define RTSPCLIENT_MAX_PASSWORD_LEN (64)
#define RTSPCLIENT_MAX_SPS_LEN  (512)
#define RTSPCLIENT_MAX_PPS_LEN  (512)
#define RTSPCLIENT_MAX_URI_LEN  (256)

/*one frame one slice case , it is the start slice and end slice*/
#define SET_START_SLICE(x)    ((x) |= 0x01)   /*set start slice of a frame*/
#define SET_NORMAL_SLICE(x)   ((x) |= 0x02)   /*set middle slice of a frame*/
#define SET_END_SLICE(x)      ((x) |= 0x04)   /*set end slice of a frame*/
#define CLEAR_SLICE(x)        ((x) = 0)       /*clear slice of a frame*/
#define CHECK_START_SLICE(x)  ((x) & 0x01)    /*check start slice*/
#define CHECK_NORMAL_SLICE(x) ((x) & 0x02)    /*check middle slice*/
#define CHECK_END_SLICE(x)    ((x) & 0x04)    /*check end slice*/

typedef enum hiLIVE_AUDIO_TYPE_E
{
    HI_LIVE_AUDIO_G711A   = 1,   /**< G.711 A            */
    HI_LIVE_AUDIO_G711Mu,   /**< G.711 Mu           */
    HI_LIVE_AUDIO_ADPCM,   /**< ADPCM              */
    HI_LIVE_AUDIO_G726,   /**< G.726              */
    HI_LIVE_AUDIO_AMR,   /**< AMR encoder format */
    HI_LIVE_AUDIO_AMRDTX,   /**< AMR encoder formant and VAD1 enable */
    HI_LIVE_AUDIO_AAC,   /**< AAC encoder        */
    HI_LIVE_AUDIO_WAV,   /**< WAV encoder        */
    HI_LIVE_AUDIO_MP3,   /**< MP3 encoder */
    HI_LIVE_AUDIO_BUTT    /**< invalid*/
} HI_LIVE_AUDIO_TYPE_E;

typedef struct hiLIVE_AUIDO_ATTR_S
{
    HI_LIVE_AUDIO_TYPE_E    enAudioFormat;       /*audio format*/
    HI_U32   u32SampleRate;       /*audio sample rate HZ*/
    HI_U32   u32BitWidth;          /*audip sample bit width */
    HI_U32   u32Channel;         /*sound mode */
    HI_U32   u32BitRate;          /*audio bit rate kbps*/
} HI_LIVE_AUIDO_ATTR_S;

typedef enum hiLIVE_VIDEO_TYPE_E
{
    HI_LIVE_VIDEO_H261  = 0,  /**< H261  */
    HI_LIVE_VIDEO_H263,  /**< H263  */
    HI_LIVE_VIDEO_MPEG2,  /**< MPEG2 */
    HI_LIVE_VIDEO_MPEG4,  /**< MPEG4 */
    HI_LIVE_VIDEO_H264,  /**< H264  */
    HI_LIVE_VIDEO_MJPEG,  /**< MOTION_JPEG*/
    HI_LIVE_VIDEO_YUV,  /**< YUV*/
    HI_LIVE_VIDEO_JPEG,  /**< JPEG*/
    HI_LIVE_VIDEO_H265,  /**< H265  */
    HI_LIVE_VIDEO_BUTT         /**< invalid*/
} HI_LIVE_VIDEO_TYPE_E;

/**video attribute struct*/
typedef struct hiLIVE_VIDEO_ATTR_S
{
    HI_LIVE_VIDEO_TYPE_E   enVideoFormat;                /** video format */
    HI_U32              u32PicWidth;                   /**video width*/
    HI_U32              u32PicHeight;                  /**video height*/
    HI_U32              u32FrameRate;                  /**frame rate*/
    HI_U32              u32BitRate;                    /**video bit rate kbps*/
    HI_U8                au8Sps[RTSPCLIENT_MAX_SPS_LEN];/**sps data*/
    HI_U8                au8Pps[RTSPCLIENT_MAX_PPS_LEN];/**pps data*/
    HI_U32              u32SpsLen;/**sps len*/
    HI_U32              u32PpsLen;/**pps len*/
} HI_LIVE_VIDEO_ATTR_S;

/**user info  struct*/
typedef struct hiLIVE_USER_INFO_S
{
    HI_CHAR      aszUserName[RTSPCLIENT_MAX_USERNAME_LEN];/**username data*/
    HI_CHAR      aszPassword[RTSPCLIENT_MAX_PASSWORD_LEN];/**password data*/
} HI_LIVE_USER_INFO_S;

/**stream info  struct*/
typedef struct hiLIVE_STREAM_INFO_S
{
    HI_BOOL bAudio;/**audio pos*/
    HI_BOOL bVideo;/**video pos*/
    HI_LIVE_VIDEO_ATTR_S stVencChAttr;/**venc info  struct*/
    HI_LIVE_AUIDO_ATTR_S stAencChAttr;/**aenc info  struct*/
} HI_LIVE_STREAM_INFO_S;

/**playstate enum*/
typedef enum hiLIVE_PLAYSTAT_TYPE_E
{
    HI_PLAYSTAT_TYPE_CONNECTING = 0, /**connecting*/
    HI_PLAYSTAT_TYPE_CONNECT_FAILED, /**cant connect or wrong psw*/
    HI_PLAYSTAT_TYPE_CONNECTED,      /**connected*/
    HI_PLAYSTAT_TYPE_ABORTIBE_DISCONNECT, /**abnormal disconnect*/
    HI_PLAYSTAT_TYPE_NORMAL_DISCONNECT,   /**user disconnect*/
    HI_PLAYSTAT_TYPE_BUTT/**palystate butt*/
} HI_LIVE_PLAYSTAT_TYPE_E;

/**stream type enum : video ,audio or other data*/
typedef enum hiLIVE_STREAM_TYPE_E
{
    HI_LIVE_STREAM_TYPE_VIDEO = 0,/**video stream type*/
    HI_LIVE_STREAM_TYPE_AUDIO,/**audio stream type*/
    HI_LIVE_STREAM_TYPE_AV,/**audio and video stream type*/
    HI_LIVE_STREAM_TYPE_DATA,/**data type*/
    HI_LIVE_STREAM_TYPE_BUTT/**butt stream  type*/
} HI_LIVE_STREAM_TYPE_E;

/**rtp data trans type enum*/
typedef enum hiLIVE_TRANS_TYPE_E
{
    HI_LIVE_TRANS_TYPE_UDP = 0,/**udp trans type*/
    HI_LIVE_TRANS_TYPE_TCP,/**rtp data trans type */
    HI_LIVE_TRANS_TYPE_BUTT/**rtp data trans type */
} HI_LIVE_TRANS_TYPE_E ;

/**h264 nalu types*/
typedef enum hiLIVE_H264_NALU_TYPE_E
{
    HI_LIVE_H264_NALU_BSLICE  = 0,    /**< h264 B frame */
    HI_LIVE_H264_NALU_PSLICE = 1, /**PSLICE types*/
    HI_LIVE_H264_NALU_ISLICE = 2, /**ISLICE types*/
    HI_LIVE_H264_NALU_IDR = 5,  /**< IDR of h264*/
    HI_LIVE_H264_NALU_SEI    = 6, /**SEI types*/
    HI_LIVE_H264_NALU_SPS    = 7, /**SPS types*/
    HI_LIVE_H264_NALU_PPS    = 8, /**PPS types*/
    HI_LIVE_H264_NALU_BUTT/**butt types*/
} HI_LIVE_H264_NALU_TYPE_E;

/**h265 nalu types*/
typedef enum hiLIVE_H265_NALU_TYPE_E
{
    HI_LIVE_H265_NALU_BSLICE = 0,    /**< h265 B frame */
    HI_LIVE_H265_NALU_PSLICE   = 1, /*pslice£º00 00 00 01 02*/
    HI_LIVE_H265_NALU_ISLICE = 2,    /**< h265 I frame */
    HI_LIVE_H265_NALU_IDR   = 19,/**Islice£º00 00 00 01 26*/
    HI_LIVE_H265_NALU_VPS      = 32,/**vps£º00 00 00 01 40*/
    HI_LIVE_H265_NALU_SPS      = 33,/**sps£º00 00 00 01 42*/
    HI_LIVE_H265_NALU_PPS      = 34,/**pps£º00 00 00 01 44*/
    HI_LIVE_H265_NALU_SEI      = 39,/**sei£º00 00 00 01 4e*/
    HI_LIVE_H265_NALU_BUTT/**butt types*/
} HI_LIVE_H265_NALU_TYPE_E;

/**g726 nalu types*/
typedef enum hiLIVE_G726_NALU_TYPE_E
{
    HI_LIVE_G726_NALU_NORMAL = 1,/**g726 normal types*/
    HI_LIVE_G726_NALU_BUTT/**g726 butt types*/
} HI_LIVE_G726_NALU_TYPE_E;

/**frame type union*/
typedef union hiLIVE_FRAME_TYPE_U
{
    HI_LIVE_H264_NALU_TYPE_E    enH264NaluType;/**h264 nalu types*/
    HI_LIVE_H265_NALU_TYPE_E    enH265NaluType;/**h265 nalu types*/
    HI_LIVE_G726_NALU_TYPE_E    enG726NaluType;/**g726 nalu types*/
} HI_LIVE_FRAME_TYPE_U;

/**rtp head info*/
typedef struct hiLIVE_RTP_HEAD_INFO_S
{
    HI_U32              u32HeadLen;/**rtp head length*/
    HI_LIVE_FRAME_TYPE_U        unFrameType;/**frame type*/
    HI_U8   u8SliceType;    /*0x01--start slice 0x02--middle slice 0x04--end slice*/
    HI_U32  u32Seqno;          /*Sequence number*/
    HI_U32  u32TimeStamp;  /*Timestamp in ms*/
    HI_U8 u8NaluType;/**nalu type*/
    HI_U8 u8LayerID;/**layerid*/
} HI_LIVE_RTP_HEAD_INFO_S;

/**stream control info struct*/
typedef struct hiSTREAM_CONTROL_S
{
    HI_LIVE_TRANS_TYPE_E enTransType;/**rtp trans type enum*/
    HI_S32 s32VideoRecvPort;/**video recv port*/
    HI_S32 s32AudioRecvPort;/**audio recv port*/
    HI_BOOL bVideo;/**video pos*/
    HI_BOOL bAudio;/**audio pos*/
} HI_LIVE_STREAM_CONTROL_S;

typedef HI_S32 (*HI_LIVE_ON_RECV)(const HI_U8* pu8BuffAddr, HI_U32 u32DataLen,
                               const HI_LIVE_RTP_HEAD_INFO_S* pstRtpInfo, HI_VOID* pHandle);

typedef HI_S32 (*HI_LIVE_ON_EVENT)( HI_VOID* pHandle, HI_LIVE_PLAYSTAT_TYPE_E enPlaystate);

typedef HI_S32 (*HI_SETUP_ON_MEDIA_CONTROL)(HI_VOID* pHandle, HI_LIVE_STREAM_CONTROL_S* pstControlInfo);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /*__HI_RTSP_CLIENT_COMM_H__*/
