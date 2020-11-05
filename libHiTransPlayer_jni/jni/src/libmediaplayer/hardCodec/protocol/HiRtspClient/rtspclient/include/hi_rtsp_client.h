/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      hi_rtsp_client.h
* @brief     rtsp client module header file
* @author    HiMobileCam middleware develop team
* @date      2016.08.10
*/

#ifndef __HI_RTSP_CLIENT_H__
#define __HI_RTSP_CLIENT_H__

#include "hi_mw_type.h"
#include "hi_rtsp_client_comm.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */


/** \addtogroup     RTSPCLIENT */
/** @{ */  /** <!-- [RTSPCLIENT] */
#define MODULE_NAME_RTSPCLIENT           "RTSPCLIENT"


/** RTSP config object type*/
typedef struct hiRTSPCLIENT_CONFIG_S
{
    HI_CHAR szUrl[RTSPCLIENT_MAX_URI_LEN];/*connect url*/
    HI_LIVE_STREAM_TYPE_E enStreamType;/*stream type enum*/
    HI_LIVE_USER_INFO_S stUserInfo;/*struct for user info */
    HI_LIVE_ON_RECV pfnOnRecvVideo;/*callback for recv video data */
    HI_LIVE_ON_RECV pfnOnRecvAudio;/*callback for recv audio data */
    HI_SETUP_ON_MEDIA_CONTROL pfnOnMediaControl;/*callback for media control */
    HI_VOID* argPriv;/*private arg*/
} HI_RTSPCLIENT_CONFIG_S;

/**
 * @brief create client instance.
 * @param[in,out] ppClient HI_MW_PTR : return rtspclient handle
 * @param[in] pstRTSPConfig HI_RTSPCLIENT_CONFIG_S : rtsp config info
 * @return   0 success
 * @return  error num failure
 */
HI_S32 HI_RTSPCLIENT_Create(HI_MW_PTR* ppClient, HI_RTSPCLIENT_CONFIG_S* pstConfig);

/**
 * @brief destroy client instance.
 * @param[in] pClient HI_MW_PTR : rtspserver handle
 * @return   0 success
 * @return  error num failure
 */
HI_S32 HI_RTSPCLIENT_Destroy(HI_MW_PTR pClient);

/**
 * @brief make client running, could handle client connecting and request.
 * @param[in] pClient HI_MW_PTR : rtspclient handle
 * @return   0 success
 * @return  error num failure
 */
HI_S32 HI_RTSPCLIENT_Start(HI_MW_PTR pClient);

/**
 * @brief make client stoped, deny client connecting and request.
 * @param[in] pClient HI_MW_PTR : rtspclient handle
 * @return   0 success
 * @return  error num failure
 */
HI_S32 HI_RTSPCLIENT_Stop(HI_MW_PTR pClient);

/**
 * @brief set rtspclient state listener.
 * @param[in] pClient HI_MW_PTR : rtspclient handle
 * @return   0 success
 * @return  error num failure
 */
HI_S32  HI_RTSPCLIENT_SetListener(HI_MW_PTR pClient, HI_LIVE_ON_EVENT pfnStateListener);

/**
 * @brief get rtspclient media info.
 * @param[in] pClient HI_MW_PTR : rtspclient handle
 * @param[in,out] pstStreamInfo HI_LIVE_STREAM_INFO_S : media info
 * @return   0 success
 * @return  error num failure
 */
HI_S32  HI_RTSPCLIENT_GetMediaInfo(HI_MW_PTR pClient, HI_LIVE_STREAM_INFO_S* pstStreamInfo);

/**
 * @brief get rtspclient media info.
 * @param[in] pClient HI_MW_PTR : rtspclient handle
 * @param[in] pszUserAgent HI_CHAR*   : user agent string
 * @return   0 success
 * @return  error num failure
 */
HI_S32  HI_RTSPCLIENT_SetUserAgent(HI_MW_PTR pClient, HI_CHAR* pszUserAgent);

/** @}*/  /** <!-- ==== RTSPCLIENT End ====*/

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /*__HI_RTSP_CLIENT_H__*/
