/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      hi_rtsp_client_stream_ext.h
* @brief     rtspclient stream on demand connect process funcs head file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/
#ifndef __HI_RTSP_CLIENT_STREAM_SESSION_EXT_H__
#define __HI_RTSP_CLIENT_STREAM_SESSION_EXT_H__

#include <pthread.h>
#include "hi_mw_type.h"
#include "hi_list.h"
#include "hi_rtsp_client_err.h"
#include "hi_rtsp_client_comm.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

typedef enum hiRTSPCLIENT_METHOD_E
{
    HI_RTSPCLIENT_METHOD_OPTIONS = 0 ,/*OPTIONS METHOD*/
    HI_RTSPCLIENT_METHOD_DESCRIBE,/*DESCRIBE METHOD*/
    HI_RTSPCLIENT_METHOD_SETUP,/*SETUP METHOD*/
    HI_RTSPCLIENT_METHOD_PLAY,/*PLAY METHOD*/
    HI_RTSPCLIENT_METHOD_PAUSE ,/*PAUSE METHOD*/
    HI_RTSPCLIENT_METHOD_TEARDOWN ,/*TEARDOWN  METHOD*/
    HI_RTSPCLIENT_METHOD_GET_PARAM ,/*GET_PARAM METHOD*/
    HI_RTSPCLIENT_METHOD_SET_PARAM ,/*SET_PARAM METHOD*/
    HI_RTSPCLIENT_METHOD_BUTT/*BUTT METHOD*/
} HI_RTSPCLIENT_METHOD_E;

/**response notify callback*/
typedef struct hi_RTSP_CLIENT_LISTENER_S
{
    HI_S32 (*onNotifyResponse)( HI_MW_PTR pClient, const HI_CHAR* pszResponse, HI_RTSPCLIENT_METHOD_E enMethod);
} HI_RTSP_CLIENT_LISTENER_S;

/**
 * @brief send msg option.
 * @param[in] pClient HI_MW_PTR : client handle
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 HI_RTSPCLIENT_Stream_Send_Option(HI_MW_PTR pClient);

/**
 * @brief send msg describe.
 * @param[in] pClient HI_MW_PTR : client handle
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 HI_RTSPCLIENT_Stream_Send_Describe(HI_MW_PTR pClient);

/**
 * @brief send msg setup.
 * @param[in] pClient HI_MW_PTR : client handle
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 HI_RTSPCLIENT_Stream_Send_SetUp(HI_MW_PTR pClient,HI_LIVE_STREAM_TYPE_E enStreamType);

/**
 * @brief send msg play.
 * @param[in] pClient HI_MW_PTR : client handle
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 HI_RTSPCLIENT_Stream_Send_Play(HI_MW_PTR pClient);

/**
 * @brief send msg teardown.
 * @param[in] pClient HI_MW_PTR : client handle
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 HI_RTSPCLIENT_Stream_Send_Teardown(HI_MW_PTR pClient);

/**
 * @brief set useragent.
 * @param[in] pClient HI_MW_PTR : client handle
 * @param[in] pszUserAgent HI_CHAR* : useragent
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 HI_RTSPCLIENT_Stream_SetUserAgent(HI_MW_PTR pClient, HI_CHAR* pszUserAgent);

/**
 * @brief set state onDestroy.
 * @param[in] pClient HI_MW_PTR : client handle
 * @param[in] stListener HI_RTSP_CLIENT_LISTENER_S :Client onDestroy listener
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 HI_RTSPCLIENT_Stream_SetClientCallback(HI_MW_PTR pClient, HI_RTSP_CLIENT_LISTENER_S* pstListener);

/**
 * @brief set state listener.
 * @param[in] pClient HI_MW_PTR : client handle
 * @param[in] pfnStateListener LIVE_ON_EVENT : listener
 * @return   0 success
 * @return  -1 failure
 */
HI_S32  HI_RTSPCLIENT_Stream_SetStateListener(HI_MW_PTR pClient, HI_LIVE_ON_EVENT pfnStateListener);

/**
 * @brief get media info.
 * @param[in] pClient HI_MW_PTR : return rtspserver handle
 * @param[in,out] pstStreamInfo HI_LIVE_STREAM_INFO_S : rtsp stream info
 * @return   0 success
 * @return  -1 failure
 */
HI_S32  HI_RTSPCLIENT_Stream_GetMediaInfo(HI_MW_PTR pClient, HI_LIVE_STREAM_INFO_S* pstStreamInfo);

/**
 * @brief create client instance.
 * @param[in,out] ppClient HI_MW_PTR : return rtspclient handle
 * @param[in] pstRTSPConfig HI_RTSP_CONFIG_S : rtsp config info
 * @return   0 success
 * @return  -1 failure
 */

HI_S32 HI_RTSPCLIENT_Stream_Create(HI_MW_PTR* ppClient, HI_RTSPCLIENT_CONFIG_S* pstConfig);

/**
 * @brief destroy client instance.
 * @param[in] pClient HI_MW_PTR : client handle
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 HI_RTSPCLIENT_Stream_Destroy(HI_MW_PTR pClient);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /*__HI_RTSP_CLIENT_STREAM_SESSION_EXT_H__*/
