#include "HiMLoger.h"
#include "HiProtocol.h"
#include "HiLiveRecord.h"

#define TAG "HiLiveRecord"

#define HI_REC_H265_NALU_IDR   (19)
#define HI_REC_H264_NALU_IDR   (5)

typedef struct stHi_LIVE_RECORD_CTX_S
{
    HI_U32 hRecordAudHdl;
    HI_U32 hRecordVidHdl;
    HI_BOOL bLostUntilIFrame;
}Hi_LIVE_RECORD_CTX_S;


HI_S32 HI_LiveRecord_Create(HI_HANDLE* pHandle, HI_S32 s32MediaMask,
    HI_U32 u32VidBufSize, HI_U32 u32AudBufSize)
{
    HI_S32 s32Ret = HI_SUCCESS;

    Hi_LIVE_RECORD_CTX_S* pCtx = (Hi_LIVE_RECORD_CTX_S*)malloc(sizeof(Hi_LIVE_RECORD_CTX_S));
    if(!pCtx)
    {
        MMLOGE(TAG, "HiLiveRecord malloc context failed\n");
        return HI_FAILURE;
    }
    memset(pCtx, 0x00, sizeof(Hi_LIVE_RECORD_CTX_S));
    if(s32MediaMask & PROTO_AUDIO_MASK)
    {
        s32Ret = HI_MBuf_Create(&pCtx->hRecordAudHdl, NULL, u32AudBufSize);
        if(s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HiLiveRecord create audio media buffer failed\n");
            free(pCtx);
            return HI_FAILURE;
        }
    }

    if(s32MediaMask & PROTO_VIDEO_MASK)
    {
        s32Ret = HI_MBuf_Create(&pCtx->hRecordVidHdl, NULL, u32VidBufSize);
        if(s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HiLiveRecord create video media buffer failed\n");
            if(s32MediaMask & PROTO_AUDIO_MASK)
            {
                HI_MBuf_Destroy(pCtx->hRecordAudHdl);
            }
            free(pCtx);
            return HI_FAILURE;
        }
    }
    pCtx->bLostUntilIFrame = HI_TRUE;
    *pHandle = (HI_HANDLE)pCtx;
    return HI_SUCCESS;
}

HI_S32 HI_LiveRecord_Destroy(HI_HANDLE hRecHandle)
{
    stHi_LIVE_RECORD_CTX_S* pCtx = (stHi_LIVE_RECORD_CTX_S*)hRecHandle;
    if(pCtx->hRecordAudHdl)
    {
        HI_MBuf_Destroy(pCtx->hRecordAudHdl);
    }

    if(pCtx->hRecordVidHdl)
    {
        HI_MBuf_Destroy(pCtx->hRecordVidHdl);
    }

    free(pCtx);
    return HI_SUCCESS;
}

HI_S32 HI_LiveRecord_WriteStream(HI_HANDLE hRecHandle,
    const HiFrameInfo* pFrmInfo, Hi_RECORD_MEDIA_TYPE_E enType)
{
    stHi_LIVE_RECORD_CTX_S* pCtx = (stHi_LIVE_RECORD_CTX_S*)hRecHandle;
    HI_S32 s32Ret = HI_SUCCESS;

    HI_HANDLE hMbufHandle = 0;
    if(enType == HI_RECORD_TYPE_VIDEO)
    {
        hMbufHandle = pCtx->hRecordVidHdl;
    }
    else if(enType == HI_RECORD_TYPE_AUDIO)
    {
        hMbufHandle = pCtx->hRecordAudHdl;
    }

    if(!hMbufHandle)
    {
        MMLOGE(TAG, "write failed, no correspond record  mbuffer type:%d !!!\n", enType);
        return HI_FAILURE;
    }

    if(pCtx->bLostUntilIFrame
        && (pFrmInfo->payload== HI_REC_H265_NALU_IDR
        || pFrmInfo->payload == HI_REC_H264_NALU_IDR))
    {
        pCtx->bLostUntilIFrame = HI_FALSE;
    }
    if(pCtx->bLostUntilIFrame)
    {
        MMLOGI(TAG, "HI_LiveRecord_WriteStream lost until I frame \n");
        return HI_SUCCESS;
    }

    s32Ret = HI_MBUF_WriteStream(hMbufHandle, pFrmInfo);
    if (s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "HI_MBUF_WriteStream faied Ret: %d!!!\n", s32Ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 HI_LiveRecord_ReadStream(HI_HANDLE hRecHandle,
    HiFrameInfo* pFrmInfo, Hi_RECORD_MEDIA_TYPE_E enType)
{
    stHi_LIVE_RECORD_CTX_S* pCtx = (stHi_LIVE_RECORD_CTX_S*)hRecHandle;
    HI_S32 s32Ret = HI_SUCCESS;

    HI_HANDLE hMbufHandle = 0;
    if(enType == HI_RECORD_TYPE_VIDEO)
    {
        hMbufHandle = pCtx->hRecordVidHdl;
    }
    else if(enType == HI_RECORD_TYPE_AUDIO)
    {
        hMbufHandle = pCtx->hRecordAudHdl;
    }

    if(!hMbufHandle)
    {
        MMLOGE(TAG, "read failed, no correspond record  mbuffer type:%d !!!\n", enType);
        return HI_FAILURE;
    }

    s32Ret = HI_Mbuf_ReadStream(hMbufHandle, pFrmInfo);
    if (s32Ret != HI_SUCCESS)
    {
        //MMLOGE(TAG, "HI_MBUF_ReadStream faied Ret: %d!!!\n", s32Ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;

}

HI_S32 HI_LiveRecord_FlushStream(HI_HANDLE hRecHandle)
{
    stHi_LIVE_RECORD_CTX_S* pCtx = (stHi_LIVE_RECORD_CTX_S*)hRecHandle;

    if(pCtx->hRecordAudHdl)
    {
        HI_MBUF_Reset(pCtx->hRecordAudHdl);
    }

    if(pCtx->hRecordVidHdl)
    {
        HI_MBUF_Reset(pCtx->hRecordVidHdl);
    }

    pCtx->bLostUntilIFrame = HI_TRUE;
    return HI_SUCCESS;
}
