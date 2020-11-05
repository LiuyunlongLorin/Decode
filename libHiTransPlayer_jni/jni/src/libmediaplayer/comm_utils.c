#include "comm_utils.h"
#include "HiMLoger.h"
#include <sys/time.h>
#include "libswresample/swresample.h"
#include "libavutil/opt.h"
#include "../arm_neon/yuv2rgb_neon.h"

#define MAX_SAVE_FRAME_NUM (20)

#define TAG "com_utils"
typedef struct
{
    HI_U32 pts;
    HI_U64 putTime;
    int bBusy;
}stSavedFrameInfo;

static stSavedFrameInfo  saveFrameInfo[MAX_SAVE_FRAME_NUM] = {0};

#ifdef YUV2RGB_NEON
void yuv420_2_rgb565_neon(uint8_t  *dst_ptr,
               const uint8_t  *y_ptr,
               const uint8_t  *u_ptr,
               const uint8_t  *v_ptr,
                     int32_t   pic_width,
                     int32_t   pic_height,
                     int32_t   y_pitch,
                     int32_t   uv_pitch,
                     int32_t   dst_pitch)
{
    int i = 0;
    for (i = 0; i < pic_height; i++)
    {
        yv12_to_rgb565_neon((uint16_t*)(dst_ptr + dst_pitch * i),
                         y_ptr + y_pitch * i,
                         u_ptr + uv_pitch * (i / 2),
                         v_ptr + uv_pitch * (i / 2),
                         pic_width,
                         0);
    }
}
#endif

void* audio_resample_init(AVFrame* audioFrame)
{
    struct SwrContext *swr;
    // Set up SWR context once you've got codec information
    swr = swr_alloc();
    av_opt_set_int(swr, "in_channel_layout",  audioFrame->channel_layout, 0);
    av_opt_set_int(swr, "out_channel_layout", audioFrame->channel_layout,  0);
    av_opt_set_int(swr, "in_sample_rate",     audioFrame->sample_rate, 0);
    av_opt_set_int(swr, "out_sample_rate",    audioFrame->sample_rate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt",  AV_SAMPLE_FMT_FLTP, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16,  0);
    swr_init(swr);

    return (void*)swr;
}

int audio_resample_convert(void* phandle, AVFrame* audioFrame, uint8_t* outputBuffer)
{
    struct SwrContext *swr = (struct SwrContext*)phandle;
    int ret = 0;

    ret = swr_convert(swr, &outputBuffer, audioFrame->nb_samples, (const uint8_t**)audioFrame->extended_data, audioFrame->nb_samples);
    return ret;
}

void audio_resample_deinit(void* phandle)
{
    struct SwrContext *swr = (struct SwrContext*)phandle;

    swr_free(&swr);
}


int ConvertProtoCodecTypeToAVCodecID(eProtoAudType type, enum AVCodecID* AcodeID)
{
    int ret = 0;
    enum AVCodecID codeID = AV_CODEC_ID_NONE;

    switch(type)
    {
        case Protocol_Audio_AAC:
            codeID = AV_CODEC_ID_AAC;
            break;

        case Protocol_Audio_G711A:
            codeID = AV_CODEC_ID_PCM_ALAW;
            break;

        case Protocol_Audio_G711Mu:
            codeID = AV_CODEC_ID_PCM_MULAW;
            break;

        case Protocol_Audio_G726LE:
            codeID = AV_CODEC_ID_ADPCM_G726LE;
            break;

            default:
                MMLOGE(TAG, "could not find corresponding avcodeID :%d \n", type);
                ret = -1;
                break;
    }
    *AcodeID = codeID;
    return ret;
}

HI_U64 getTimeS()
{
    struct timeval pTime;
    gettimeofday(&pTime, NULL);
    return (pTime.tv_sec + (pTime.tv_usec / 1000000.0));
}

HI_S64 getSysTimeUs()
{
    struct timeval pTime;
    gettimeofday(&pTime, NULL);
    return ((pTime.tv_sec*1000000) + (pTime.tv_usec));
}

HI_U64 getTimeUs()
{
    struct timeval pTime;
    gettimeofday(&pTime, NULL);
    return ((pTime.tv_sec*1000000) + (pTime.tv_usec));
}

HI_U32 getTimeMs()
{
    HI_U32 time = (getTimeUs()/1000);
    return time;
}

void HI_LiveClient_Init_Loc(void)
{
    memset(saveFrameInfo, 0x00, MAX_SAVE_FRAME_NUM*sizeof(stSavedFrameInfo));
}

void HI_LiveClient_Clear_Loc(void)
{
    int i =0;
    for(; i<MAX_SAVE_FRAME_NUM; i++)
    {
        saveFrameInfo[i].bBusy = 0;
    }
}

void HI_LiveClient_Acq_Loc(HI_U32 pts)
{
    int i =0;
    for(; i<MAX_SAVE_FRAME_NUM; i++)
    {
        if(!saveFrameInfo[i].bBusy)
            break;
    }
    if(i == MAX_SAVE_FRAME_NUM)
    {
        MMLOGI(TAG, "save buf is full \n");
        return;
    }
    saveFrameInfo[i].pts =  pts;
    saveFrameInfo[i].putTime =  getTimeUs();
    saveFrameInfo[i].bBusy = 1;
    //MMLOGI(TAG, "put  pts: %d time: %lld\n", pts, saveFrameInfo[i].putTime/1000);
}

int HI_LiveClient_Get_Loc(HI_U32 pts, HI_U64* time, HI_BOOL bErase)
{
    int i =0;
    for(; i<MAX_SAVE_FRAME_NUM; i++)
    {
        if(saveFrameInfo[i].bBusy && saveFrameInfo[i].pts == pts)
            break;
    }
    if(i == MAX_SAVE_FRAME_NUM)
    {
        return -1;
    }
    *time = saveFrameInfo[i].putTime;
    if(bErase)
        saveFrameInfo[i].bBusy = 0;
    //MMLOGI(TAG, "get time: %lld\n",  saveFrameInfo[i].putTime/1000);
    return 0;
}

typedef struct
{
    HI_U64 dataMax;
    HI_U64 dataMin ;
    HI_U64 dataSum;
    HI_U64 dataAva;
    HI_U64 count;
    HI_BOOL bFristFlag;
}stStatisticInfo;

int HI_Test_Statistic_Init(HI_U32* pHandle)
{
    HI_VOID* pData = HI_NULL;
    pData = malloc(sizeof(stStatisticInfo));
    if(!pData)
        return -1;

    memset(pData, 0x00, sizeof(stStatisticInfo));
    *pHandle = (HI_U32)pData;
    return 0;
}

int HI_Test_Statistic_Update(HI_U32 handle, HI_U64  data)
{
    stStatisticInfo* pData = (stStatisticInfo*)handle;

    if(!pData)
        return -1;

    pData->count++;
    if(!pData->bFristFlag)
    {
        pData->dataMin = data;
        pData->dataMax = data;
        pData->bFristFlag = HI_TRUE;
    }
    else
    {
        if(pData->dataMin > data)
            pData->dataMin = data;
        if(pData->dataMax < data)
            pData->dataMax = data;
    }
    pData->dataSum += data;

    return 0;
}

void HI_Test_Statistic_PrintInfo(HI_U32 handle)
{
    stStatisticInfo* pData = (stStatisticInfo*)handle;
    if(!pData)
        return ;
    if(pData->count)
        pData->dataAva = (pData->dataSum/pData->count) +1;
    MMLOGI(TAG, "Min: %lld Max: %lld  Ava: %lld\n", pData->dataMin, pData->dataMax, pData->dataAva);
}

void HI_Test_Statistic_DeInit(HI_U32 handle)
{
    stStatisticInfo* pData = (stStatisticInfo*)handle;
    if(pData)
        free(pData);
}

