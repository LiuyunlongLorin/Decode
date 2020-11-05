#ifndef __HI_MBUF_MANAGER_H__
#define __HI_MBUF_MANAGER_H__

#include "hi_type.h"


#define MAX_VIDEO_CACHE_MBUF_SIZE (20*1024*1024)
#define MAX_AUDIO_CACHE_MBUF_SIZE (2*1024*1024)


HI_S32 HI_CacheBuf_Init(HI_BOOL bNeedAlloc);

HI_S32 HI_CacheBuf_DeInit(HI_VOID);

HI_S32 HI_CacheBuf_Put_Video(const HI_VOID* pbuf, HI_U32 dataLen , HI_U32 pts, HI_S32 payload);

HI_S32 HI_CacheBuf_Get_Video(const HI_VOID* ptr, HI_U32& dataSize, int64_t& pts, HI_S32& pType);

HI_S32 HI_CacheBuf_Put_Audio(const HI_VOID* pbuf, HI_U32 dataLen , HI_U32 pts);

HI_S32 HI_CacheBuf_Get_Audio(const HI_VOID* ptr, HI_U32& dataSize, int64_t& pts);

HI_BOOL HI_CacheBuf_IsCacheFull(HI_VOID);

HI_S32 HI_CacheBuf_GetVidCacheSize(HI_VOID);

HI_VOID HI_CacheBuf_Flush(HI_VOID);

HI_U32 HI_CacheBuf_GetLastCachedVidPts(HI_VOID);

HI_S32 HI_CacheBuf_SeekTo(HI_U32 timeMs);

#endif
