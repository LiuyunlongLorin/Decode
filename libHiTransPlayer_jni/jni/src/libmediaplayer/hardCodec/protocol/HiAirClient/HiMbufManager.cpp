#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include "HiMbufManager.h"
#include "Hies_proto.h"
#include "HiMLoger.h"
#include "comm_utils.h"
#include "HiMbuffer.h"

/*********************  LOG macro  ***********************/
#define TAG "HiMbufManager"


//#define ENTER_FUNC Printf("+++++++ enter function\n")
//#define EXIT_FUNC Printf("------- exit function\n")


extern AudFrameInfo mAudframeBuf[MAX_AUDIO_BUF_NUM];
extern FrameInfo mframeBuf[MAX_VIDEO_BUF_NUM];
static HI_HANDLE vidMbufHandle = 0;
static HI_HANDLE audMbufHandle = 0;
static HI_U32 mMaxVidCacheSize = 0;
static HI_U32 mMaxAudCacheSize = 0;



static HI_BOOL mbMbufAlloced = HI_FALSE;
static HI_VOID* mpMbufVidPtr = NULL;
static HI_VOID* mpMbufAudPtr = NULL;



static HI_S32 HI_CacheBuf_AllocMbuf(HI_VOID)
{
    mpMbufVidPtr = NULL;
    mpMbufAudPtr = NULL;

    MMLOGI(TAG, "need malloc mbuf for store big resolution \n");

    mpMbufVidPtr = malloc(MAX_VIDEO_CACHE_MBUF_SIZE);
    if(!mpMbufVidPtr)
    {
        MMLOGE(TAG, "malloc mpMbufVidPtr failed!!\n");
        return HI_FAILURE;
    }
    memset(mpMbufVidPtr, 0x00, MAX_VIDEO_CACHE_MBUF_SIZE);

    mpMbufAudPtr = malloc(MAX_AUDIO_CACHE_MBUF_SIZE);
    if(!mpMbufAudPtr)
    {
        MMLOGE(TAG, "malloc mpMbufAudPtr failed!!\n");
        free(mpMbufVidPtr);
        return HI_FAILURE;
    }
    memset(mpMbufAudPtr, 0x00, MAX_AUDIO_CACHE_MBUF_SIZE);
    return HI_SUCCESS;
}

static HI_VOID HI_CacheBuf_FreeMbuf(HI_VOID)
{
    MMLOGI(TAG, "need free mbuf for store big resolution \n");
    if(mpMbufVidPtr)
    {
        free(mpMbufVidPtr);
        mpMbufVidPtr = NULL;
    }

    if(mpMbufVidPtr)
    {
        free(mpMbufVidPtr);
        mpMbufVidPtr = NULL;
    }
}

HI_S32 HI_CacheBuf_Init(HI_BOOL bNeedAlloc)
{
    HI_S32 ret = 0;
    HI_VOID* pVidMbufPtr = NULL;
    HI_VOID* pAudMbufPtr = NULL;

    if(bNeedAlloc)
    {
        ret = HI_CacheBuf_AllocMbuf();
        if(HI_SUCCESS != ret)
        {
            MMLOGE(TAG, "HI_CacheBuf_AllocMbuf failed ret : %x\n",ret);
            return HI_FAILURE;
        }
        mMaxVidCacheSize = MAX_VIDEO_CACHE_MBUF_SIZE;
        mMaxAudCacheSize = MAX_AUDIO_CACHE_MBUF_SIZE;
        pVidMbufPtr = mpMbufVidPtr;
        pAudMbufPtr = mpMbufAudPtr;
        mbMbufAlloced = HI_TRUE;
    }
    else
    {
        mMaxVidCacheSize = sizeof(FrameInfo)*MAX_VIDEO_BUF_NUM;
        mMaxAudCacheSize = sizeof(AudFrameInfo)*MAX_AUDIO_BUF_NUM;
        pVidMbufPtr = mframeBuf;
        pAudMbufPtr = mAudframeBuf;
        mbMbufAlloced = HI_FALSE;
    }

    MMLOGI(TAG, "HiCacheSource vid cache ptr :0x%08x size: %d  aud ptr: 0x%08x size: %d\n", pVidMbufPtr
        , mMaxVidCacheSize, pAudMbufPtr, mMaxAudCacheSize);

    ret = HI_MBuf_Create(&vidMbufHandle, pVidMbufPtr, mMaxVidCacheSize);
    if(HI_SUCCESS != ret)
    {
        MMLOGE(TAG, "HI_MBuf_Create vid error ret : %x\n",ret);
        if(bNeedAlloc)
            HI_CacheBuf_FreeMbuf();
        return HI_FAILURE;
    }

    ret = HI_MBuf_Create(&audMbufHandle, pAudMbufPtr, mMaxAudCacheSize);
    if(HI_SUCCESS != ret)
    {
        MMLOGE(TAG, "HI_MBuf_Create aud error ret : %x\n",ret);
        if(bNeedAlloc)
            HI_CacheBuf_FreeMbuf();
        ret=HI_MBuf_Destroy(vidMbufHandle);
        if(HI_SUCCESS != ret)
        {
            MMLOGE(TAG, "HI_MBuf_Destroy error ret : %x\n",ret);
            return HI_FAILURE;
        }
        return HI_FAILURE;
    }
    MMLOGI(TAG, "HI_CacheBuf_Init success!!\n");
    return HI_SUCCESS;
}

HI_S32 HI_CacheBuf_DeInit(HI_VOID)
{
    HI_S32 ret = 0;

    ret = HI_MBuf_Destroy(vidMbufHandle);
    if(HI_SUCCESS != ret)
    {
        MMLOGE(TAG, "HI_MBuf_Destroy vidMbufHandle error ret : %x\n",ret);
    }
    vidMbufHandle = 0;

    ret = HI_MBuf_Destroy(audMbufHandle);
    if(HI_SUCCESS != ret)
    {
        MMLOGE(TAG, "HI_MBuf_Destroy audMbufHandle error ret : %x\n",ret);
    }
    audMbufHandle = 0;
    if(mbMbufAlloced)
        HI_CacheBuf_FreeMbuf();
    MMLOGI(TAG, "HI_CacheBuf_DeInit success!!\n");
    return HI_SUCCESS;
}

HI_S32 HI_CacheBuf_Put_Video(const HI_VOID* pbuf, HI_U32 u32dataLen , HI_U32 u32pts, HI_S32 payload)
{
    HiFrameInfo frameInfo;

    memset(&frameInfo, 0x00, sizeof(HiFrameInfo));
    frameInfo.LenData = u32dataLen;
    frameInfo.pData = (HI_U8*)pbuf;
    frameInfo.payload = payload;
    frameInfo.pts = u32pts;
    //MMLOGI(TAG," put video frame  len : %d pts: %d pt: %d \n", dataLen,pts, payload);

    HI_S32 ret = HI_MBUF_WriteStream(vidMbufHandle, &frameInfo);
    if(HI_SUCCESS != ret)
    {
        MMLOGE(TAG, "HI_MBUF_WriteStream video error ret : %x\n",ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 HI_CacheBuf_Get_Video(const HI_VOID* pbuf, HI_U32& u32dataSize, int64_t& pts, HI_S32& s32payloadType)
{
    HiFrameInfo frameInfo;

    if(!vidMbufHandle)
    {
        MMLOGE(TAG, "video mbuf handle have been destroyed\n");
        return HI_FAILURE;
    }
    memset(&frameInfo, 0x00, sizeof(HiFrameInfo));
    frameInfo.LenData = u32dataSize;
    frameInfo.pData = (HI_U8*)pbuf;
    HI_S32 ret = HI_Mbuf_ReadStream(vidMbufHandle, &frameInfo);
    if(HI_SUCCESS != ret)
    {
        //MMLOGE(TAG, "HI_Mbuf_ReadStream  video error ret : %x\n",ret);
        return HI_FAILURE;
    }

    pts = frameInfo.pts;
    s32payloadType = frameInfo.payload;
    u32dataSize = frameInfo.LenData;

    MMLOGI(TAG," get video frame  len : %d pts: %lld pt: %d \n", frameInfo.LenData, pts, s32payloadType);
    return HI_SUCCESS;
}

HI_S32 HI_CacheBuf_Put_Audio(const HI_VOID* pbuf, HI_U32 u32dataLen , HI_U32 u32pts)
{
    HiFrameInfo frameInfo;

    memset(&frameInfo, 0x00, sizeof(HiFrameInfo));
    frameInfo.LenData = u32dataLen;
    frameInfo.pData = (HI_U8*)pbuf;
    frameInfo.payload = 0;
    frameInfo.pts = u32pts;
    //MMLOGI(TAG," put audio frame  len : %d pts: %d\n", dataLen,pts);

    HI_S32 ret = HI_MBUF_WriteStream(audMbufHandle, &frameInfo);
    if(HI_SUCCESS != ret)
    {
        MMLOGE(TAG, "HI_MBUF_WriteStream audio error ret : %x\n",ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 HI_CacheBuf_Get_Audio(const HI_VOID* pbuf, HI_U32& u32dataSize, int64_t& pts)
{
    HiFrameInfo frameInfo;

    if(!audMbufHandle)
    {
        MMLOGE(TAG, "audio mbuf handle have been destroyed\n");
        return HI_FAILURE;
    }
    memset(&frameInfo, 0x00, sizeof(HiFrameInfo));
    frameInfo.LenData = u32dataSize;
    frameInfo.pData = (HI_U8*)pbuf;
    HI_S32 ret = HI_Mbuf_ReadStream(audMbufHandle, &frameInfo);
    if(HI_SUCCESS != ret)
    {
        //MMLOGE(TAG, "HI_Mbuf_ReadStream  audio error ret : %x\n",ret);
        return HI_FAILURE;
    }

    pts = frameInfo.pts;
    u32dataSize = frameInfo.LenData;

    //MMLOGI(TAG," get audio frame  len : %d pts: %d \n", frameInfo.LenData, pts);
    return HI_SUCCESS;
}

HI_BOOL HI_CacheBuf_IsCacheFull(HI_VOID)
{
    if(HI_MBUF_GetFreeBufferSize(vidMbufHandle) < MAX_VIDEO_FRAME_SIZE)
    {
        MMLOGI(TAG, "vid cache buf is full \n");
        return HI_TRUE;
    }

    if(HI_MBUF_GetFreeBufferSize(audMbufHandle) < MAX_AUDIO_FRAME_SIZE)
    {
        MMLOGI(TAG, "aud cache buf is full \n");
        return HI_TRUE;
    }
    return HI_FALSE;
}

HI_S32 HI_CacheBuf_GetVidCacheSize(HI_VOID)
{
    HI_U32 framNum = HI_MBUF_GetCachedFrameNum(vidMbufHandle);
    //MMLOGI(TAG, "vid cache frameNum: %d \n", framNum);
    if(1 == framNum)
    {
        return 0;
    }
    HI_U32 freesize = HI_MBUF_GetFreeBufferSize(vidMbufHandle);
    return mMaxVidCacheSize-freesize;
}

HI_VOID HI_CacheBuf_Flush(HI_VOID)
{
    HI_MBUF_Reset(vidMbufHandle);
    HI_MBUF_Reset(audMbufHandle);
}

HI_U32 HI_CacheBuf_GetLastCachedVidPts(HI_VOID)
{
    return HI_MBUF_GetLastPTS(vidMbufHandle);
}

HI_S32 HI_CacheBuf_SeekTo(HI_U32 timeMs)
{
    HI_S32 ret = HI_SUCCESS;
    HI_U32 realVidSeekMs = 0;
    MMLOGI(TAG, "HI_CacheBuf_SeekTo : %d \n", timeMs);
    ret = HI_MBUF_VidSeekTo(vidMbufHandle, timeMs, &realVidSeekMs);
    if(ret < 0)
    {
        MMLOGI(TAG, "vid seek time is not in video cache range\n");
        return HI_FAILURE;
    }
    ret = HI_MBUF_AudSeekTo(audMbufHandle, realVidSeekMs);
    if(ret < 0)
    {
        MMLOGI(TAG, "aud seek time is not in audio cache range\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}
