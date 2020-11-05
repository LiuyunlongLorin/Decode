#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "pthread.h"
#include "Hies_proto.h"
#include "HiMLoger.h"
#include "comm_utils.h"
#include "HiMbuffer.h"
#include "HiFileCacheBuf.h"

#define true            1
#define false            0
#define TAG "FileClientMbuf"


static HI_HANDLE hVidMbufHandle = 0;
static HI_HANDLE hAudMbufHandle = 0;


int HI_FileClient_Init_Proto(void)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32AudMbufSize = 512*1024;
    s32Ret = HI_MBuf_Create(&hAudMbufHandle, NULL, u32AudMbufSize);
    if(s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "FileClient create audio media buffer failed\n");
        return HI_FAILURE;
    }

    HI_U32 u32VidMbufSize = 5*1024*1024;
    s32Ret = HI_MBuf_Create(&hVidMbufHandle, NULL, u32VidMbufSize);
    if(s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "FileClient create video media buffer failed\n");
        HI_MBuf_Destroy(hAudMbufHandle);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

int HI_FileClient_DeInit_Proto(void)
{
    HI_S32 s32Ret = HI_SUCCESS;

    s32Ret = HI_MBuf_Destroy(hAudMbufHandle);
    if(s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "FileClient HI_MBuf_Destroy aud failed\n");
        return HI_FAILURE;
    }
    hAudMbufHandle = 0;

    s32Ret = HI_MBuf_Destroy(hVidMbufHandle);
    if(s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "FileClient HI_MBuf_Destroy failed\n");
        return HI_FAILURE;
    }
    hVidMbufHandle = 0;

    MMLOGI(TAG, "HI_FileClient_DeInit_Proto  success out \n");
    return 0;
}

void HI_FileClient_Flush_AudioCache(void)
{
    HI_S32 s32Ret = HI_SUCCESS;
    s32Ret = HI_MBUF_Reset(hAudMbufHandle);
    if(s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "FileClient audio HI_MBUF_Reset failed\n");
    }
}

HI_S32 HI_FileClient_put_audio(const void* pbuf, HI_U32 dataLen , HI_U32 pts)
{
    HI_S32 s32Ret = HI_SUCCESS;

    HiFrameInfo stCurFrame;
    memset(&stCurFrame, 0x00, sizeof(stCurFrame));

    stCurFrame.pData = (HI_U8*)pbuf;
    stCurFrame.LenData = dataLen;
    stCurFrame.pts =  pts;
    //MMLOGE(TAG, "write audio frame pts: %d len: %d \n", stCurFrame.pts, dataLen);
    s32Ret = HI_MBUF_WriteStream(hAudMbufHandle, &stCurFrame);
    if (s32Ret == HI_RET_MBUF_FULL)
    {
        //MMLOGI(TAG, "audio buf write full");
        return HI_ERR_BUF_FULL;
    }
    else if (s32Ret != HI_SUCCESS)
    {
        MMLOGI(TAG, "audio HI_MBUF_WriteStream faied Ret: %d!!!\n", s32Ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

int HI_FileClient_read_audio_stream(void* ptr, HI_U32& dataSize, int64_t& pts)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HiFrameInfo stFrameInfo;
    memset(&stFrameInfo, 0x00, sizeof(stFrameInfo));

    stFrameInfo.pData = (HI_U8*)ptr;
    stFrameInfo.LenData= dataSize;

    s32Ret = HI_Mbuf_ReadStream(hAudMbufHandle, &stFrameInfo);
    if (s32Ret == HI_RET_MBUF_EMTPTY)
    {
        //MMLOGI(TAG, "audio buf write full");
        return HI_ERR_BUF_EMPTY;
    }
    else if (s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "fileclient audio HI_Mbuf_ReadStream failed\n");
        return HI_FAILURE;
    }

    if (s32Ret != HI_SUCCESS)
    {
        //MMLOGE(TAG, "fileclient audio HI_Mbuf_ReadStream failed\n");
        return s32Ret;
    }
    //MMLOGE(TAG, "read audio frame pts: %d len: %d payload: %d\n", stFrameInfo.pts, stFrameInfo.LenData, stFrameInfo.payload);
    dataSize = stFrameInfo.LenData;
    pts = stFrameInfo.pts;
    return HI_SUCCESS;
}

int HI_FileClient_getAudioCacheCnt()
{
    return HI_MBUF_GetCachedFrameNum(hAudMbufHandle);
}

void HI_FileClient_Flush_VideoCache(void)
{
    HI_S32 s32Ret = HI_SUCCESS;
    s32Ret = HI_MBUF_Reset(hVidMbufHandle);
    if(s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "FileClient HI_MBUF_Reset failed\n");
    }
}

HI_S32 HI_FileClient_put_video(const void* pbuf, HI_U32 dataLen , HI_U32 pts, int payload)
{
    HI_S32 s32Ret = HI_SUCCESS;

    HiFrameInfo stCurFrame;
    memset(&stCurFrame, 0x00, sizeof(stCurFrame));

    stCurFrame.pData = (HI_U8*)pbuf;
    stCurFrame.LenData = dataLen;
    stCurFrame.pts =  pts;
    stCurFrame.payload = payload;
    //MMLOGE(TAG, "write video frame pts: %d len: %d pt: %d\n", stCurFrame.pts, dataLen, payload);
    s32Ret = HI_MBUF_WriteStream(hVidMbufHandle, &stCurFrame);
    if (s32Ret == HI_RET_MBUF_FULL)
    {
        //MMLOGI(TAG, "video buf write full");
        return HI_ERR_BUF_FULL;
    }
    else if (s32Ret != HI_SUCCESS)
    {
        MMLOGI(TAG, "video HI_MBUF_WriteStream faied Ret: %d!!!\n", s32Ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

int HI_FileClient_getVideoCacheCnt()
{
    return HI_MBUF_GetCachedFrameNum(hVidMbufHandle);
}

int HI_FileClient_read_video_stream(void* ptr, HI_U32& dataSize, int64_t& pts, int& pType)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HiFrameInfo stFrameInfo;
    memset(&stFrameInfo, 0x00, sizeof(stFrameInfo));

    stFrameInfo.pData = (HI_U8*)ptr;
    stFrameInfo.LenData= dataSize;

    s32Ret = HI_Mbuf_ReadStream(hVidMbufHandle, &stFrameInfo);
    if (s32Ret == HI_RET_MBUF_EMTPTY)
    {
        //MMLOGI(TAG, "video buf read empty");
        return HI_ERR_BUF_EMPTY;
    }
    else if (s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "fileclient video HI_Mbuf_ReadStream failed\n");
        return HI_FAILURE;
    }
    //MMLOGE(TAG, "file client read video frame pts: %d len: %d payload: %d\n", stFrameInfo.pts, stFrameInfo.LenData, stFrameInfo.payload);
    dataSize = stFrameInfo.LenData;
    pts = stFrameInfo.pts;
    pType = stFrameInfo.payload;
    return HI_SUCCESS;
}
