/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      rtspclient_msgparser.c
* @brief     rtspclient message parser funcs src file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/
#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include "hi_rtsp_client.h"
#include "rtsp_client_stream.h"
#include "rtsp_client_socket.h"
#include "rtsp_client_msgparser.h"
#include "rtsp_client_log.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */
#define INVALID_STATUS "Invalid Status"
#define RTSPCLIENT_SPS_PPS_SET "sprop-parameter-sets="
#define RTSPCLIENT_TIMEOUT "timeout="
#define INVALID_STATUS_CODE  (-1)
#define RTSPCLIENT_NORMAL_TIMEOUT (60)

HI_VOID RTSPCLIENT_MSGParser_GetPlayerVers(HI_CHAR* pszVerStr)
{
    if (NULL != pszVerStr)
    {
        snprintf(pszVerStr, RTSP_AGENTBUF_MAX_LEN, "%s/1.0.0(%s)", RTSP_CLIENT_VERSION, __DATE__);
    }
    else
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "%s/1.0.0(%s)", RTSP_CLIENT_VERSION, __DATE__);
    }
    return;
}

HI_S32 RTSPCLIENT_MSGParser_GetSPSPPS(HI_CHAR* pszInBuf, HI_CHAR* pszSpsBase64,
                                      HI_U32* pu32SpsLen , HI_CHAR* pszPpsBase64, HI_U32* pu32PpsLen)
{
    HI_CHAR* pszSps = NULL;
    HI_CHAR* pszPps = NULL;
    HI_CHAR* pszSpsTmp = NULL;
    HI_CHAR* pszSpsPos = NULL;
    HI_U32 u32Len = 0;

    if ((NULL == pszInBuf )||(NULL == pszSpsBase64 ) || (NULL == pszPpsBase64))
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "the buffer is null\n");
        return HI_FAILURE;
    }

    pszSps = strstr(pszInBuf, RTSPCLIENT_SPS_PPS_SET);
    if ( NULL == pszSps )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "get sps pps fail\n");
        return HI_FAILURE;
    }

    u32Len = strlen(RTSPCLIENT_SPS_PPS_SET);
    pszSps = pszSps + u32Len;

    pszPps = strchr(pszSps, ',');
    if ( NULL == pszPps)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_MSGParser_GetSPSPPS errro the :%s\n", pszSps);
        return HI_SUCCESS;
    }

    pszPps += 1;
    pszSpsTmp = strstr(pszSps, ";");
    if (NULL == pszSpsTmp)
    {
        pszSpsTmp = strstr(pszSps, "\r\n");
    }
    else
    {
        pszSpsPos = strstr(pszSps, "\r\n");
        if (NULL != pszSpsPos)
        {
            pszSpsTmp = (pszSpsPos - pszSps) < (pszSpsTmp - pszSps) ? pszSpsPos : pszSpsTmp;
        }
    }

    if (pszSpsTmp != NULL)
    {
        *pu32SpsLen = pszPps - pszSps - 1;

        if (RTSPCLIENT_MAX_SPS_LEN > (*pu32SpsLen))
        {
            memcpy(pszSpsBase64, pszSps, pszPps - pszSps - 1);
            pszSpsBase64[RTSPCLIENT_MAX_SPS_LEN-1]='\0';
        }
        else
        {
            return HI_FAILURE;
        }

        *pu32PpsLen = pszSpsTmp - pszPps;
        if (RTSPCLIENT_MAX_PPS_LEN > (*pu32PpsLen))
        {
            memcpy(pszPpsBase64, pszPps, pszSpsTmp - pszPps);
            pszPpsBase64[RTSPCLIENT_MAX_PPS_LEN-1]='\0';
        }
        else
        {
            return HI_FAILURE;
        }
    }

    return HI_SUCCESS;

}

HI_S32 RTSPCLIENT_MSGParser_GetTimeout(HI_CHAR* pszInBuf, HI_S32* ps32Timeout)
{
    HI_CHAR* ptimeout = NULL;
    HI_U32 u32Len = 0;
    if (NULL == pszInBuf || NULL == ps32Timeout)
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "RTSPCLIENT_MSGParser_GetTimeout param null\n");
        return HI_FAILURE;
    }

    u32Len = strlen(RTSPCLIENT_TIMEOUT);

    ptimeout = strstr(pszInBuf, RTSPCLIENT_TIMEOUT);
    if (  NULL != ptimeout)
    {
        *ps32Timeout = atoi(ptimeout + u32Len);
    }
    else/*if do not have timeout set the normal value 60*/
    {
        *ps32Timeout = RTSPCLIENT_NORMAL_TIMEOUT;
    }

    return HI_SUCCESS;
}

HI_S32 RTSPCLIENT_MSGParser_GetSessID(const HI_CHAR* pMsgStr, HI_CHAR* pszSessId)
{
    HI_CHAR* pTemp = NULL;

    /*get sessionid*/
    pTemp = strcasestr(pMsgStr, RTSP_HEADER_SESSION);

    if ( NULL == pTemp )
    {
        RTSP_CLIENT_LOG_Printf(MODULE_NAME_RTSPCLIENT, RTSP_CLIENT_LOG_ERR, "there no sessid in req str\n");
        return HI_FAILURE;
    }

    if (RTSPCLIENT_SCANF_RET_ONE != sscanf(pTemp, "%*s %15s", pszSessId)  )
    {
        return HI_FAILURE;
    }
    pszSessId[RTSPCLIENT_MAX_SESSID_LEN-1] = '\0';

    return HI_SUCCESS;
}

HI_CHAR* RTSPCLIENT_MSGParser_SDP_GetPos(const HI_CHAR* pszBuffer, HI_CHAR* pszArgName)
{
    HI_CHAR* pszArgPos = NULL;

    if (NULL == pszBuffer || NULL == pszArgName)
    {
        return NULL;
    }

    pszArgPos = strstr((HI_CHAR*)pszBuffer, pszArgName);
    if (NULL != pszArgPos)
    {
        return pszArgPos + strlen(pszArgName);
    }

    return NULL;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
