/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      base64.h
* @brief     rtspclient base64 head file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/

#ifndef __BASE64_H__
#define __BASE64_H__

#include <stdio.h>
#include "hi_type.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

HI_S32 HI_RTSPCLIENT_Base64Decode(const HI_U8* puchInput, HI_S32 s32InputLen, HI_U8* puchOutput, HI_S32 s32OutputLen);

HI_S32 HI_RTSPCLIENT_Base64Encode(const HI_U8* puchInput, HI_S32 s32InputLen, HI_U8* puchOutput, HI_S32 s32OutputLen);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
#endif  /* __BASE64_H__ */
