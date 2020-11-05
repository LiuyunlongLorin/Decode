
#ifndef __HIES_PROTO__
#define __HIES_PROTO__
#include "libavcodec/avcodec.h"
#include "hi_type.h"

#define MAX_AUDIO_BUF_NUM  40
#define MAX_VIDEO_BUF_NUM  20
typedef int BOOL;


#ifdef __cplusplus
extern "C" {
#endif
// 1 second of 48khz 32bit audio
#define MAX_AUDIO_FRAME_SIZE 192000
#define MAX_VIDEO_FRAME_SIZE (1024*1024)

#define H264_PAYLOAD_TYPE_I_FRAME (5)
#define H264_PAYLOAD_TYPE_P_FRAME (1)

/*视频帧数据结构*/
/*video frame struct*/
typedef struct _FrameInfo
{
    int64_t pts;
    HI_U32 datalen;
    int payload;
    unsigned char data[MAX_VIDEO_FRAME_SIZE];
    BOOL busy;
}FrameInfo;

/*音频帧数据结构*/
/*audio frame struct*/
typedef struct _AudFrameInfo
{
    int64_t pts;
    HI_U32 datalen;
    int payload;
    HI_U8 data[MAX_AUDIO_FRAME_SIZE];
    BOOL busy;
}AudFrameInfo;


#ifdef __cplusplus
}
#endif
#endif
