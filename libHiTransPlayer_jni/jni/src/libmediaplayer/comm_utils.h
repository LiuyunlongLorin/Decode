#ifndef __TRANS_COMMON_UTILS_H__
#define __TRANS_COMMON_UTILS_H__

#ifdef __cplusplus
extern "C"{
#endif

#include "hi_type.h"
#include "libavcodec/avcodec.h"

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

#define ABS(a, b) ((a) > ((b))) ? ((a)-(b)) : ((b)-(a))

#define REVERSE(a) (((a)<0)?(-(a)):(a))

#define HI_MAX_URL_LENGTH (1024)

#define H264_I_FRAME_PAYLOAD (5)

#define H265_I_FRAME_PAYLOAD (19)

#define ERROR_PROTO_EXIT (1001)

#define MAX_TRACK_NUM (5)

#define INVALID_THREAD_ID (-1)

#define HI_ERR_BUF_FULL (0x1001)
#define HI_ERR_BUF_EMPTY (0x1002)

typedef enum _eProtoVideoType
{
        Protocol_Video_H264,
        Protocol_Video_H265,
        /*add new type */
        Protocol_Video_BUTT
}eProtoVidType;

typedef enum _eProtoAudioType
{
        Protocol_Audio_AAC,
        Protocol_Audio_G711A,
        Protocol_Audio_G711Mu,
        Protocol_Audio_G726LE,
        /*add new type */
        Protocol_Audio_BUTT
}eProtoAudType;

typedef enum tagTrackTypeEnum
{
    TRACK_TYPE_VIDEO_E,
    TRACK_TYPE_AUDIO_E,
    TRACK_TYPE_DATA_E,
    TRACK_TYPE_UNKOWN_E,
    TRACK_TYPE_BUTT_E
}TrackTypeEnum;

typedef struct _stProtoAudioAttr
{
    eProtoAudType type;
    HI_S32 sampleRate;
    HI_S32 bitwidth;
    HI_S32 chnNum;
    HI_U32 u32BitRate;
}stProtoAudAttr;

typedef struct _stProtoVideoAttr
{
    eProtoVidType type;
    HI_U32 width;
    HI_U32 height;
}stProtoVideoAttr;

typedef struct tagMediaTrackInfo
{
    TrackTypeEnum enTrackType;
    union Attr
    {
        stProtoVideoAttr stVideoAttr;
        stProtoAudAttr stAudioAttr;
    }  attr;
}MediaTrackInfo;

typedef struct tagProtoMediaInfo
{
    HI_U8 u8TrackNum;
    MediaTrackInfo stTrackInfo[MAX_TRACK_NUM];
}ProtoMediaInfo;

void yuv420_2_rgb565_neon(uint8_t  *dst_ptr,
               const uint8_t  *y_ptr,
               const uint8_t  *u_ptr,
               const uint8_t  *v_ptr,
                     int32_t   pic_width,
                     int32_t   pic_height,
                     int32_t   y_pitch,
                     int32_t   uv_pitch,
                     int32_t   dst_pitch);

void* audio_resample_init(AVFrame* audioFrame);

int audio_resample_convert(void* phandle, AVFrame* audioFrame, uint8_t* outputBuffer);

void audio_resample_deinit(void* phandle);

#define PRINT_DECODETIME
int ConvertProtoCodecTypeToAVCodecID(eProtoAudType, enum AVCodecID* AcodeID);

HI_S64 getSysTimeUs();

HI_U64 getTimeS();

HI_U64 getTimeUs();

HI_U32 getTimeMs();

void HI_LiveClient_Init_Loc(void);

int HI_LiveClient_Get_Loc(HI_U32 pts, HI_U64* time, HI_BOOL bErase);

void HI_LiveClient_Acq_Loc(HI_U32 pts);

void HI_LiveClient_Clear_Loc(void);

#ifdef PRINT_DECODETIME
int HI_Test_Statistic_Init(HI_U32* pHandle);

void HI_Test_Statistic_PrintInfo(HI_U32 handle);

int HI_Test_Statistic_Update(HI_U32 handle, HI_U64  data);

void HI_Test_Statistic_DeInit(HI_U32 handle);
#endif

#ifdef __cplusplus
}
#endif

#endif
