#ifndef __HI_LIVE_RECORD_H__
#define __HI_LIVE_RECORD_H__

#include "hi_type.h"
#include "HiMbuffer.h"

typedef enum enHi_RECORD_MEDIA_TYPE
{
    HI_RECORD_TYPE_VIDEO,
    HI_RECORD_TYPE_AUDIO,
    HI_RECORD_TYPE_BUTT
}Hi_RECORD_MEDIA_TYPE_E;


HI_S32 HI_LiveRecord_Create(HI_HANDLE* pHandle, HI_S32 s32MediaMask,
    HI_U32 u32VidBufSize, HI_U32 u32AudBufSize);


HI_S32 HI_LiveRecord_Destroy(HI_HANDLE hRecHandle);


HI_S32 HI_LiveRecord_WriteStream(HI_HANDLE hRecHandle,
    const HiFrameInfo* pFrmInfo, Hi_RECORD_MEDIA_TYPE_E enType);


HI_S32 HI_LiveRecord_ReadStream(HI_HANDLE hRecHandle,
    HiFrameInfo* pFrmInfo, Hi_RECORD_MEDIA_TYPE_E enType);

HI_S32 HI_LiveRecord_FlushStream(HI_HANDLE hRecHandle);

#endif
