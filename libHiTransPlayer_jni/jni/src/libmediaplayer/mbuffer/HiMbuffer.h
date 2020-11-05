#ifndef __HI_MEDIA_BUFFER_H__
#define __HI_MEDIA_BUFFER_H__


#ifdef __cplusplus
extern "C" {
#endif

#include "hi_type.h"

#define  HI_RET_MBUF_FULL (1)
#define  HI_RET_MBUF_EMTPTY (2)

typedef struct __HiFrameInfo
{
    HI_U8* pData;
    HI_U32 LenData;
    HI_S32 payload;
    HI_U32 pts;
}HiFrameInfo;

HI_S32 HI_MBuf_Create(HI_HANDLE* pMbufHandle, void* pBuf, HI_U32 bufSize);

HI_S32 HI_MBuf_Destroy(HI_HANDLE MbufHandle);

HI_S32 HI_MBUF_Reset(HI_HANDLE hMbufHandle);

HI_S32 HI_Mbuf_ReadStream(HI_HANDLE mbufHandle, HiFrameInfo* pstTranFrame);

HI_S32 HI_MBUF_WriteStream(HI_HANDLE mbufHandle, const HiFrameInfo* pstFrame);

HI_S32 HI_MBUF_VidSeekTo(HI_HANDLE hMbufHandle, HI_U32 timeMs, HI_U32* realSeekMs);

HI_U32 HI_MBUF_AudSeekTo(HI_HANDLE hMbufHandle, HI_U32 timeMs);

HI_U32 HI_MBUF_GetLastPTS(HI_HANDLE hMbufHandle);

HI_U32 HI_MBUF_GetCachedFrameNum(HI_HANDLE mbufHandle);

HI_U32 HI_MBUF_GetFreeBufferSize(HI_HANDLE mbufHandle);

#ifdef __cplusplus
}
#endif
#endif
