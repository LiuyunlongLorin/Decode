#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include "HiMLoger.h"
#include "comm_utils.h"
#include "HiMbuffer.h"

#define TAG "HiMbuffer"

#define MBUF_LOCK(pMutex) \
        do{\
            pthread_mutex_lock(pMutex);\
        }while(0)

#define MBUF_UNLOCK(pMutex) \
        do{\
            pthread_mutex_unlock(pMutex);\
        }while(0)


#define MBUF_STREAM_RCV_LEN (32)

#define SIZE_OF_PAYLOAD_HEAD (sizeof(HI_U32)+sizeof(HI_U32)+sizeof(HI_U32))

typedef struct tagHI_MBUF_HANDLE_S
{
    HI_U8 *pBaseAddr;       /*buffer start addr*/
    HI_U32  u32BufLen;      /*buffer total len, by bytes*/
    HI_U32  u32RsvByte;     /*reserved bytes*/
    HI_U8  *pWrPtr;           /*Current write pointer*/
    HI_U8  *pLastWrPtr;     /*用于判断是否环回*/
    HI_U8 *pLoopPtr;            /* 标记发生环回的位置 */
    HI_U32 u32LastTimeStamp;    /*用于比对读取速度的上一次时间戳*/
    HI_U32 u32LostStreamTimes;  /*丢帧个数*/
    HI_U32 totalFrameNum;

    HI_U32  u32FirstPts;     /*写入的第一帧视频pts*/
    pthread_mutex_t lock;       /* 状态锁*/
    /*below is read element*/
    HI_U8 *pReadPtr;
    HI_U8 *pLastReadPtr;
    HI_BOOL bUseInputBuffer;
}HI_MBUF_HANDLE_S;

HI_U32 HI_MBUF_GetCachedFrameNum(HI_HANDLE mbufHandle)
{
    HI_MBUF_HANDLE_S* pTranMbufHandle = (HI_MBUF_HANDLE_S*)mbufHandle;;
    return pTranMbufHandle->totalFrameNum;
}

HI_U32 HI_MBUF_GetFreeBufferSize(HI_HANDLE mbufHandle)
{
    HI_MBUF_HANDLE_S* pTranMbufHandle = (HI_MBUF_HANDLE_S*)mbufHandle;;

    HI_U8  *pReadPtr = HI_NULL;
    pReadPtr = pTranMbufHandle->pReadPtr;
    if(pTranMbufHandle->pWrPtr == pTranMbufHandle->pBaseAddr && pTranMbufHandle->pLoopPtr == HI_NULL)
    {
        return pTranMbufHandle->u32BufLen;
    }
    if(pReadPtr <= pTranMbufHandle->pWrPtr)
    {
        return pTranMbufHandle->u32BufLen - (HI_U32)(pTranMbufHandle->pWrPtr - pReadPtr);
    }
    else
    {
        return (HI_U32)(pReadPtr - pTranMbufHandle->pWrPtr);
    }
}

static HI_U32 GetAvailableSpace(HI_HANDLE mbufHandle)
{
    HI_MBUF_HANDLE_S* pTranMbufHandle = HI_NULL;
    pTranMbufHandle = (HI_MBUF_HANDLE_S*)mbufHandle;
    HI_U8* pEndPtr = pTranMbufHandle->pBaseAddr + pTranMbufHandle->u32BufLen;

    HI_U32 u32FreeSpace = 0;

    if (pTranMbufHandle->pWrPtr < pTranMbufHandle->pReadPtr)
    {
        /*0******w***************r********/
        u32FreeSpace = pTranMbufHandle->pReadPtr - pTranMbufHandle->pWrPtr;
    }
    else if(pTranMbufHandle->pWrPtr == pTranMbufHandle->pReadPtr)
    {
        u32FreeSpace = pTranMbufHandle->u32BufLen;
    }
    else
    {
        /*0*******r***************w********/
        if (pEndPtr <= pTranMbufHandle->pWrPtr)
        {
            u32FreeSpace = 0;
        }
        else
        {
            u32FreeSpace = pEndPtr - pTranMbufHandle->pWrPtr;
        }
        u32FreeSpace = (u32FreeSpace > (pTranMbufHandle->pReadPtr-pTranMbufHandle->pBaseAddr)) ?
            u32FreeSpace : (pTranMbufHandle->pReadPtr-pTranMbufHandle->pBaseAddr);
    }
        //MMLOGE(TAG, "ava space: %d \n", u32FreeSpace);
    return u32FreeSpace;
}

static HI_BOOL NeedLoop(HI_MBUF_HANDLE_S* pTranMbufHandle, HI_U32 u32TotalLen)
{
    HI_U8* pEndPtr = pTranMbufHandle->pBaseAddr + pTranMbufHandle->u32BufLen;

    HI_U8* pNextWritePtr = pTranMbufHandle->pWrPtr + u32TotalLen;
    if (pNextWritePtr >= pEndPtr)
    {
        return HI_TRUE;
    }

    return HI_FALSE;
}

HI_S32 HI_MBUF_WriteStream(HI_HANDLE mbufHandle, const HiFrameInfo* pstFrame)
{
    HI_S32 s32Ret = 0;
    HI_MBUF_HANDLE_S* pTranMbufHandle = HI_NULL;
    HI_U32 u32WriteDataTmp = 0;

    pTranMbufHandle = (HI_MBUF_HANDLE_S*)mbufHandle;
    MBUF_LOCK(&pTranMbufHandle->lock);
    HI_U32 u32Total = SIZE_OF_PAYLOAD_HEAD + pstFrame->LenData;
    if (u32Total >= GetAvailableSpace(mbufHandle))
    {
        MBUF_UNLOCK(&pTranMbufHandle->lock);
        return HI_RET_MBUF_FULL;
    }

    if (NeedLoop(pTranMbufHandle, u32Total))
    {
        /*update the looppos and writepos*/
        pTranMbufHandle->pLoopPtr = pTranMbufHandle->pWrPtr;
        pTranMbufHandle->pWrPtr = pTranMbufHandle->pBaseAddr;
        if(pTranMbufHandle->pReadPtr == pTranMbufHandle->pLoopPtr)//if read get to  write when loop,loop readpos too
        {
            pTranMbufHandle->pReadPtr = pTranMbufHandle->pBaseAddr;
        }
        MMLOGE(TAG, "loop happened ptr: 0x%08x \n", pTranMbufHandle->pLoopPtr);
    }

    u32WriteDataTmp = (HI_U32)pstFrame->payload;
    memcpy(pTranMbufHandle->pWrPtr, &u32WriteDataTmp,sizeof(HI_U32));

    u32WriteDataTmp = pstFrame->LenData;
    memcpy(pTranMbufHandle->pWrPtr + sizeof(HI_U32), &u32WriteDataTmp, sizeof(HI_U32));

    u32WriteDataTmp = pstFrame->pts;
    memcpy(pTranMbufHandle->pWrPtr + sizeof(HI_U32) + sizeof(HI_U32), &u32WriteDataTmp, sizeof(HI_U32));

    memcpy(pTranMbufHandle->pWrPtr + SIZE_OF_PAYLOAD_HEAD , pstFrame->pData, pstFrame->LenData);
    pTranMbufHandle->pLastWrPtr = pTranMbufHandle->pWrPtr;
    pTranMbufHandle->pWrPtr = pTranMbufHandle->pWrPtr + SIZE_OF_PAYLOAD_HEAD + pstFrame->LenData;
    pTranMbufHandle->u32LastTimeStamp = pstFrame->pts;
    if(!pTranMbufHandle->u32FirstPts)
        pTranMbufHandle->u32FirstPts = pstFrame->pts;
    pTranMbufHandle->totalFrameNum++;

        //MMLOGE(TAG, "pWrPtr : %x ,pts : %d len : %d!!\n",pTranMbufHandle->pLastWrPtr,pstFrame->pts,pstFrame->LenData);

    MBUF_UNLOCK(&pTranMbufHandle->lock);
    return HI_SUCCESS;
}

HI_S32 HI_Mbuf_ReadStream(HI_HANDLE mbufHandle, HiFrameInfo* pstTranFrame)
{
    HI_MBUF_HANDLE_S* pTranMbufHandle = (HI_MBUF_HANDLE_S*)mbufHandle;
    HI_S32 s32Ret = 0;
    HI_U32 u32StreamLen = 0;
    HI_U32 u32ReadBuffer = 0;
    HI_U8* pNextRdPtr = HI_NULL;

    MBUF_LOCK(&pTranMbufHandle->lock);

    if ((pTranMbufHandle->pReadPtr >= pTranMbufHandle->pLoopPtr) && (pTranMbufHandle->pWrPtr < pTranMbufHandle->pReadPtr))
    {
        MMLOGE(TAG, "read loop ptr:%x!!\n",pTranMbufHandle->pReadPtr);
        pTranMbufHandle->pReadPtr = pTranMbufHandle->pBaseAddr;
    }

    if (pTranMbufHandle->pReadPtr != pTranMbufHandle->pWrPtr)
    {
        memcpy(&u32ReadBuffer, pTranMbufHandle->pReadPtr, sizeof(HI_U32));
        pstTranFrame->payload = u32ReadBuffer;

        memcpy(&u32ReadBuffer, pTranMbufHandle->pReadPtr + sizeof(HI_U32), sizeof(HI_U32));
        u32StreamLen = u32ReadBuffer;
        memcpy(&u32ReadBuffer, pTranMbufHandle->pReadPtr + sizeof(HI_U32) + sizeof(HI_U32), sizeof(HI_U32));
        pstTranFrame->pts = u32ReadBuffer;
        if(pstTranFrame->LenData < u32StreamLen)
        {
            MMLOGE(TAG, "mbuf  read input buf is too short: %d datalen: %d!!\n", pstTranFrame->LenData, u32StreamLen);
            MBUF_UNLOCK(&pTranMbufHandle->lock);
            return HI_FAILURE;
        }

        memcpy(pstTranFrame->pData, pTranMbufHandle->pReadPtr + SIZE_OF_PAYLOAD_HEAD, u32StreamLen);
        pstTranFrame->LenData = u32StreamLen;

        pTranMbufHandle->pLastReadPtr = pTranMbufHandle->pReadPtr;
            //MMLOGE(TAG, "pReadPtr : %x ,pts : %d len : %d!!\n",pTranMbufHandle->pLastReadPtr,pstTranFrame->pts,pstTranFrame->LenData);

    }
    else
    {
        MMLOGE(TAG, "mbuf there no data could be read!!\n");
        MBUF_UNLOCK(&pTranMbufHandle->lock);
        return HI_RET_MBUF_EMTPTY;
    }

    pNextRdPtr = pTranMbufHandle->pReadPtr + (SIZE_OF_PAYLOAD_HEAD + u32StreamLen);

    if ((pNextRdPtr >= pTranMbufHandle->pLoopPtr) && (pTranMbufHandle->pWrPtr < pNextRdPtr))
    {
        MMLOGE(TAG, "read loop ptr:%x!!\n",pNextRdPtr);
        pNextRdPtr = pTranMbufHandle->pBaseAddr;
    }

    pTranMbufHandle->pReadPtr = pNextRdPtr;
    pTranMbufHandle->totalFrameNum--;

    MBUF_UNLOCK(&pTranMbufHandle->lock);

    return HI_SUCCESS;
}

HI_S32 HI_MBUF_VidSeekTo(HI_HANDLE hMbufHandle, HI_U32 timeMs, HI_U32* realSeekMs)
{
    HI_U32 u32NextPTS = 0;
    HI_U32 u32NextPayload = 0;
    HI_U32 u32NextDataLen = 0;
    HI_MBUF_HANDLE_S* pTranMbufHandle = (HI_MBUF_HANDLE_S*)hMbufHandle;
    HI_U8* pPrevIFramePtr = NULL;
    HI_U32 u32PrevIFrameReadCnt = 0;
    HI_U32 u32ReadCnt = 0;
    HI_U32 u32PrevIFramePTS = 0;
    HI_BOOL bFind = HI_FALSE;
    HI_BOOL bFirstFrame = HI_FALSE;
    HI_S32 s32Ret = HI_SUCCESS;

    MBUF_LOCK(&pTranMbufHandle->lock);
    HI_U8* pReadPtr = pTranMbufHandle->pReadPtr;

    MMLOGI(TAG, "vid seek cur cache last pts: %d totalNum: %d!!\n", pTranMbufHandle->u32LastTimeStamp,
        pTranMbufHandle->totalFrameNum);

    if (pTranMbufHandle->pReadPtr == pTranMbufHandle->pWrPtr)
    {
        MBUF_UNLOCK(&pTranMbufHandle->lock);
        return HI_FAILURE;
    }

    while(1)
    {
        if(pReadPtr == pTranMbufHandle->pLoopPtr)
        {
            pReadPtr = pTranMbufHandle->pBaseAddr;
        }
        memcpy(&u32NextPayload, pReadPtr,sizeof(HI_U32));
        memcpy(&u32NextDataLen, pReadPtr + sizeof(HI_U32 ),sizeof(HI_U32));
        memcpy(&u32NextPTS, pReadPtr + sizeof(HI_U32) + sizeof(HI_U32),sizeof(HI_U32));

        if(u32NextDataLen == 0)
        {
            MMLOGE(TAG, "vid seek mbuf read out len 0!!\n");
            break;
        }
        HI_U8* pNextReadPtr = pReadPtr + (SIZE_OF_PAYLOAD_HEAD + u32NextDataLen);
        if(pReadPtr <= pTranMbufHandle->pWrPtr)//未环回
        {
            if(pNextReadPtr >= pTranMbufHandle->pWrPtr)
            {
                //already readto latest wrptr, just return
                MMLOGE(TAG, "seekto: read to end !!\n");
                break;
            }
        }
        else//环回
        {
            if(pNextReadPtr >= pTranMbufHandle->pLoopPtr)
            {
                MMLOGE(TAG, "error  seekto: nextReadPtr overflow!!\n");
                break;
            }
        }
        u32ReadCnt++;
        if(u32NextPayload == 5 || u32NextPayload == 19)
        {
            u32PrevIFramePTS = u32NextPTS;
            pPrevIFramePtr = pReadPtr;
            u32PrevIFrameReadCnt = u32ReadCnt;
        }
        if(!bFirstFrame)
        {
            bFirstFrame = HI_TRUE;
            if(timeMs < u32NextPTS)
            {
                MMLOGI(TAG, "vid seekto: seekTime smaller than frist pts: %d \n", u32NextPTS);
                break;
            }
        }
        MMLOGI(TAG, "vid seekto: next pts: %d pt: %d \n", u32NextPTS, u32NextPayload);
        if(timeMs <= u32NextPTS)
        {
            if(pPrevIFramePtr)
            {
                //find the right I Frame, update the readPtr
                pTranMbufHandle->pReadPtr = pPrevIFramePtr;
                pTranMbufHandle->pLastReadPtr = pPrevIFramePtr;
                pTranMbufHandle->totalFrameNum -= (u32PrevIFrameReadCnt-1);
                bFind = HI_TRUE;
                MMLOGE(TAG, "vid seek: find cached Frame pts: %d skip %d frames!!\n", u32PrevIFramePTS, (u32PrevIFrameReadCnt-1));
                *realSeekMs = u32PrevIFramePTS;
                break;
            }
            else
            {
                //prev have no I frame, so find to end
                pReadPtr = pNextReadPtr;
            }
        }
        else
        {
            pReadPtr = pNextReadPtr;
        }
    }
    pTranMbufHandle->u32FirstPts = 0;
    MBUF_UNLOCK(&pTranMbufHandle->lock);
    if(!bFind)
        s32Ret = HI_FAILURE;
    return s32Ret;
}

HI_U32 HI_MBUF_AudSeekTo(HI_HANDLE hMbufHandle, HI_U32 timeMs)
{
    HI_U32 u32NextPTS = 0;
    HI_U32 u32NextDataLen = 0;
    HI_U32 u32ReadCnt = 0;
    HI_MBUF_HANDLE_S* pTranMbufHandle = (HI_MBUF_HANDLE_S*)hMbufHandle;
    HI_BOOL bFind = HI_FALSE;
    HI_S32 s32Ret = HI_SUCCESS;
    HI_BOOL bFirstFrame = HI_FALSE;

    MBUF_LOCK(&pTranMbufHandle->lock);
    HI_U8* pReadPtr = pTranMbufHandle->pReadPtr;

    MMLOGE(TAG, "aud seek cur cache last pts: %d!!\n", pTranMbufHandle->u32LastTimeStamp);

    if (pTranMbufHandle->pReadPtr == pTranMbufHandle->pWrPtr)
    {
        MBUF_UNLOCK(&pTranMbufHandle->lock);
        return HI_FAILURE;
    }

    while(1)
    {
        if(pReadPtr == pTranMbufHandle->pLoopPtr)
        {
            pReadPtr = pTranMbufHandle->pBaseAddr;
        }
        memcpy(&u32NextDataLen, pReadPtr + sizeof(HI_U32 ),sizeof(HI_U32));
        memcpy(&u32NextPTS, pReadPtr + sizeof(HI_U32) + sizeof(HI_U32),sizeof(HI_U32));

        HI_U8* pNextReadPtr = pReadPtr + (SIZE_OF_PAYLOAD_HEAD + u32NextDataLen);
        if(pReadPtr <= pTranMbufHandle->pWrPtr)//未环回
        {
            if(pNextReadPtr >= pTranMbufHandle->pWrPtr)
            {
                //already readto latest wrptr, just return
                MMLOGE(TAG, "aud  seekto: already readto latest!!\n");
                break;
            }
        }
        else//环回
        {
            if(pNextReadPtr >= pTranMbufHandle->pLoopPtr)
            {
                MMLOGE(TAG, "aud seekto: nextReadPtr overflow!!\n");
                break;
            }
        }
        if(!bFirstFrame)
        {
            bFirstFrame = HI_TRUE;
            if(timeMs < u32NextPTS)
            {
                MMLOGI(TAG, "aud seekto: seekTime smaller than frist pts: %d \n", u32NextPTS);
                break;
            }
        }
        MMLOGI(TAG, "aud seekto: next pts: %d \n", u32NextPTS);
        u32ReadCnt++;
        if(timeMs <= u32NextPTS)
        {
            //find the right I Frame, update the readPtr
            pTranMbufHandle->pReadPtr = pReadPtr;
            pTranMbufHandle->pLastReadPtr = pReadPtr;
            pTranMbufHandle->totalFrameNum -= (u32ReadCnt-1);
            MMLOGE(TAG, "aud seek: find cached Frame pts: %d skip : %d frame!!\n", u32NextPTS, (u32ReadCnt-1));
            bFind = HI_TRUE;
            break;
        }
        else
        {
            pReadPtr = pReadPtr + (SIZE_OF_PAYLOAD_HEAD + u32NextDataLen);
        }
    }
    pTranMbufHandle->u32FirstPts = 0;
    MBUF_UNLOCK(&pTranMbufHandle->lock);
    if(!bFind)
        s32Ret = HI_FAILURE;
    return s32Ret;
}


HI_U32 HI_MBUF_GetLastPTS(HI_HANDLE hMbufHandle)
{
    HI_U32 u32NextPTS = 0;
    HI_MBUF_HANDLE_S* pTranMbufHandle = (HI_MBUF_HANDLE_S*)hMbufHandle;

    return pTranMbufHandle->u32LastTimeStamp;
}

HI_S32 HI_MBUF_Reset(HI_HANDLE hMbufHandle)
{
    HI_MBUF_HANDLE_S* pTranMbufHandle = (HI_MBUF_HANDLE_S*)hMbufHandle;

    MBUF_LOCK(&pTranMbufHandle->lock);
    memset((HI_VOID*)pTranMbufHandle->pBaseAddr, 0x00, pTranMbufHandle->u32BufLen);

    pTranMbufHandle->pLoopPtr = HI_NULL;
    pTranMbufHandle->pWrPtr = pTranMbufHandle->pBaseAddr;
    pTranMbufHandle->pLastWrPtr = pTranMbufHandle->pBaseAddr;
    pTranMbufHandle->u32LastTimeStamp = 0;
    pTranMbufHandle->u32LostStreamTimes = 0;
    pTranMbufHandle->pReadPtr = pTranMbufHandle->pBaseAddr;
    pTranMbufHandle->pLastReadPtr = pTranMbufHandle->pBaseAddr;

    pTranMbufHandle->totalFrameNum = 0;
    pTranMbufHandle->u32FirstPts = 0;

    MBUF_UNLOCK(&pTranMbufHandle->lock);

    MMLOGI(TAG, "HI_MBUF_Reset success!!\n");
    return HI_SUCCESS;
}


HI_S32 HI_MBuf_Create(HI_HANDLE* pMbufHandle, void* pBuf, HI_U32 bufSize)
{
    HI_MBUF_HANDLE_S* pTranMbufHandle = 0;
    int ret  = 0;

    pTranMbufHandle = (HI_MBUF_HANDLE_S*)malloc(sizeof(HI_MBUF_HANDLE_S));
    if(HI_NULL == pTranMbufHandle)
    {
        MMLOGE(TAG, "mbuf write Handle alloc memory error!\n");
        return HI_FAILURE;
    }
    memset(pTranMbufHandle,0,sizeof(HI_MBUF_HANDLE_S));

    if(!pBuf)
    {
        pBuf = malloc(bufSize);
        if(!pBuf)
        {
            MMLOGE(TAG, "HI_MBuf_Create alloc  buffer %d failed\n", bufSize);
            free(pTranMbufHandle);
            return HI_FAILURE;
        }
    }
    else
    {
        pTranMbufHandle->bUseInputBuffer = HI_TRUE;
    }

    pTranMbufHandle->pBaseAddr = (HI_U8*)pBuf;
    pTranMbufHandle->u32BufLen = bufSize;
    pTranMbufHandle->pLastWrPtr = pTranMbufHandle->pBaseAddr;
    pTranMbufHandle->pWrPtr = pTranMbufHandle->pBaseAddr;
    pTranMbufHandle->u32RsvByte = MBUF_STREAM_RCV_LEN;

    pTranMbufHandle->pReadPtr = pTranMbufHandle->pBaseAddr;
    pTranMbufHandle->pLastReadPtr = pTranMbufHandle->pBaseAddr;

    ret = pthread_mutex_init(&pTranMbufHandle->lock,HI_NULL);
    if(HI_SUCCESS != ret)
    {
        MMLOGE(TAG, "pthread_mutex_init error ret : %x\n",ret);
        free(pTranMbufHandle);
        return HI_FAILURE;
    }

    *pMbufHandle = (HI_U32)pTranMbufHandle;

    return ret;
}

HI_S32 HI_MBuf_Destroy(HI_HANDLE MbufHandle)
{
    HI_MBUF_HANDLE_S* pTranMbufHandle = (HI_MBUF_HANDLE_S*)MbufHandle;
    int ret  = 0;

    pthread_mutex_unlock(&pTranMbufHandle->lock);
    ret = pthread_mutex_destroy(&pTranMbufHandle->lock);
    if(HI_SUCCESS != ret && EBUSY != ret)
    {
        MMLOGE(TAG, "pthread_mutex_destroy lock error ret : %x\n",ret);
    }
    if(!pTranMbufHandle->bUseInputBuffer)
    {
        free(pTranMbufHandle->pBaseAddr);
    }
    free(pTranMbufHandle);
    return ret;
}
