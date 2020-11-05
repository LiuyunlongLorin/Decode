#ifndef __HI_FILE_CACHE_BUF_H__
#define __HI_FILE_CACHE_BUF_H__
#include "hi_type.h"


void HI_FileClient_Flush_VideoCache(void);

int HI_FileClient_getVideoCacheCnt(void);

HI_S32 HI_FileClient_put_video(const void* pbuf, HI_U32 dataLen , HI_U32 pts, int payload);

int HI_FileClient_read_video_stream(void* ptr, HI_U32& dataSize, int64_t& pts, int& pType);

void HI_FileClient_Flush_AudioCache(void);

int HI_FileClient_getAudioCacheCnt(void);

HI_S32 HI_FileClient_put_audio(const void* pbuf, HI_U32 dataLen , HI_U32 pts);

int HI_FileClient_read_audio_stream(void* ptr, HI_U32& dataSize, int64_t& pts);

int HI_FileClient_Init_Proto(void);

int HI_FileClient_DeInit_Proto(void);

#endif
