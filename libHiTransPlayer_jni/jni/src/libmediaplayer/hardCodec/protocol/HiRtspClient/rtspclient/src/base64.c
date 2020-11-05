/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      base64.c
* @brief     rtspclient base64 src file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "base64.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* __cplusplus */
static HI_U8  s_au8Basis64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static HI_U8 s_au8Index64[128] =
{
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,   62, 0xff, 0xff, 0xff,   63,
    52,   53,   54,   55,   56,   57,   58,   59,   60,   61, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff,    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11,   12,   13,   14,
    15,   16,   17,   18,   19,   20,   21,   22,   23,   24,   25, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff,   26,   27,   28,   29,   30,   31,   32,   33,   34,   35,   36,   37,   38,   39,   40,
    41,   42,   43,   44,   45,   46,   47,   48,   49,   50,   51, 0xff, 0xff, 0xff, 0xff, 0xff
};
#define CHAR64(c)  (((c) > 127) ? 0xff : s_au8Index64[(c)])  /*define char64(c)*/

HI_S32 HI_RTSPCLIENT_Base64Decode(const HI_U8* puchInput, HI_S32 s32InputLen, HI_U8* puchOutput, HI_S32 s32OutputLen)
{
    HI_S32     i = 0;
    HI_S32     s32OutPos = 0;
    HI_S32 s32Pad;
    HI_U8   uchTemp[4];
    if (NULL == puchInput)
    {
        return 0;
    }
    while ((i + 3) < s32InputLen)
    {
        s32Pad  = 0;
        uchTemp[0] = CHAR64(puchInput[i]);
        if ( 0xff == uchTemp[0] )
        {
            s32Pad++;
        }

        uchTemp[1] = CHAR64(puchInput[i + 1]);
        if (0xff == uchTemp[1] )
        {
            s32Pad++;
        }

        uchTemp[2] = CHAR64(puchInput[i + 2]);
        if (0xff == uchTemp[2] )
        {
            s32Pad++;
        }

        uchTemp[3] = CHAR64(puchInput[i + 3]);
        if (0xff == uchTemp[3] )
        {
            s32Pad++;
        }

        if ( 2 == s32Pad )
        {
            if (s32OutPos < (s32OutputLen - 2))
            {
                puchOutput[s32OutPos++] = (uchTemp[0] << 2) | ((uchTemp[1] & 0x30) >> 4);
                puchOutput[s32OutPos]   = (uchTemp[1] & 0x0f) << 4;
            }
        }
        else if (1 == s32Pad )
        {
            if (s32OutPos < (s32OutputLen - 3))
            {
                puchOutput[s32OutPos++] = (uchTemp[0] << 2) | ((uchTemp[1] & 0x30) >> 4);
                puchOutput[s32OutPos++] = ((uchTemp[1] & 0x0f) << 4) | ((uchTemp[2] & 0x3c) >> 2);
                puchOutput[s32OutPos]   = (uchTemp[2] & 0x03) << 6;
            }
        }
        else
        {
            if (s32OutPos < (s32OutputLen - 3))
            {
                puchOutput[s32OutPos++] = (uchTemp[0] << 2) | ((uchTemp[1] & 0x30) >> 4);
                puchOutput[s32OutPos++] = ((uchTemp[1] & 0x0f) << 4) | ((uchTemp[2] & 0x3c) >> 2);
                puchOutput[s32OutPos++] = ((uchTemp[2] & 0x03) << 6) | (uchTemp[3] & 0x3f);
            }
        }

        i += 4;
    }

    return s32OutPos;
}

HI_S32 HI_RTSPCLIENT_Base64Encode(const HI_U8* puchInput, HI_S32 s32InputLen, HI_U8* puchOutput, HI_S32 s32OutputLen)
{
    HI_S32 i = 0;
    HI_S32 s32OutPos = 0;
    HI_S32 s32Pad;
    if (NULL == puchInput)
    {
        return 0;
    }

    while (i < s32InputLen)
    {
        s32Pad = 3 - (s32InputLen - i);

        if (2 == s32Pad)
        {
            if (s32OutPos < (s32OutputLen - 3) )
            {
                puchOutput[s32OutPos  ] = s_au8Basis64[puchInput[i] >> 2];
                puchOutput[s32OutPos + 1] = s_au8Basis64[(puchInput[i] & 0x03) << 4];
                puchOutput[s32OutPos + 2] = '=';
                puchOutput[s32OutPos + 3] = '=';
            }
        }
        else if (1 == s32Pad )
        {
            if (s32OutPos < (s32OutputLen - 3) && (i + 1) < s32InputLen)
            {
                puchOutput[s32OutPos  ] = s_au8Basis64[puchInput[i] >> 2];
                puchOutput[s32OutPos + 1] = s_au8Basis64[((puchInput[i] & 0x03) << 4) | ((puchInput[i + 1] & 0xf0) >> 4)];
                puchOutput[s32OutPos + 2] = s_au8Basis64[(puchInput[i + 1] & 0x0f) << 2];
                puchOutput[s32OutPos + 3] = '=';
            }
        }
        else
        {
            if (s32OutPos < (s32OutputLen - 3) && (i + 2) < s32InputLen)
            {
                puchOutput[s32OutPos  ] = s_au8Basis64[puchInput[i] >> 2];
                puchOutput[s32OutPos + 1] = s_au8Basis64[((puchInput[i] & 0x03) << 4) | ((puchInput[i + 1] & 0xf0) >> 4)];
                puchOutput[s32OutPos + 2] = s_au8Basis64[((puchInput[i + 1] & 0x0f) << 2) | ((puchInput[i + 2] & 0xc0) >> 6)];
                puchOutput[s32OutPos + 3] = s_au8Basis64[puchInput[i + 2] & 0x3f];
            }
        }

        i += 3;
        s32OutPos += 4;
    }

    return s32OutPos;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
