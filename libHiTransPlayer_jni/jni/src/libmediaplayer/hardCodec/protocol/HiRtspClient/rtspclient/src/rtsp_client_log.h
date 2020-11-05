/**
  * Copyright (C), 2016-2020, Hisilicon Tech. Co., Ltd.
  * All rights reserved.
  *
  * @file         rtsp_client_log.h
  * @brief      middleware log function.
  * @author   HiMobileCam middleware develop team
  * @date      2016.06.29
  */
#ifndef __RTSP_CLIENT_LOG_H__
#define __RTSP_CLIENT_LOG_H__

#include "hi_mw_type.h"
#include "hi_defs.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */


/*
 * @brief  log level type.
 */
typedef enum hiRTSP_CLIENT_LOG_E
{
    RTSP_CLIENT_LOG_DEBUG = 0,
    RTSP_CLIENT_LOG_INFO,
    RTSP_CLIENT_LOG_WARN,
    RTSP_CLIENT_LOG_ERR,
    RTSP_CLIENT_LOG_FATAL,
    RTSP_CLIENT_LOG_BUTT
} RTSP_CLIENT_LOG_E;

typedef HI_VOID(*RTSP_CLIENT_OUTPUT_FUNC)(const HI_CHAR* fmt, ... );

/**
*   @brief  set enabled log level, logs with equal or higher level than enabled will be output
*   @param[in] enLevel : RTSPCLIENT_LOG_E: enabled level
*   @retval  0 success,others failed
*/
HI_S32 RTSP_CLIENT_LOG_SetEnabledLevel(RTSP_CLIENT_LOG_E enLevel);

/**
*   @brief  use this API to set log output function instread of 'printf'
*   @param[in] pFunc : RTSPCLIENT_OUTPUT_FUNC: output function implements by user
*   @retval  0 success,others failed
*/
HI_S32 RTSP_CLIENT_LOG_SetOutputFunc(RTSP_CLIENT_OUTPUT_FUNC pFunc);

/**
*   @brief  output log
*   @param[in] pszModName : HI_CHAR*: module name
*   @param[in] enLevel : RTSPCLIENT_LOG_E: log level
*   @param[in] pszFmt : HI_CHAR*: log content, accept multi-parameters
*   @retval  0 success,others failed
*/
HI_S32 RTSP_CLIENT_LOG_Printf(const HI_CHAR* pszModName, RTSP_CLIENT_LOG_E enLevel, const HI_CHAR* pszFmt, ...) __attribute__((format(printf,3,4)));


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* __RTSP_CLIENT_LOG_H__ */
