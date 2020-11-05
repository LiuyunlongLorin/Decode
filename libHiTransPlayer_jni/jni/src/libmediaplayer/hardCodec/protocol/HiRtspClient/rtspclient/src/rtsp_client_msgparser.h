/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      hi_rtspclient_msgparser.h
* @brief     rtspclient message parser funcs head file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/
#ifndef __RTSPCLIENT_MSG_PARSER_H__
#define __RTSPCLIENT_MSG_PARSER_H__

#include "hi_type.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */
#define RTSP_METHOD_OPTIONS "OPTIONS"
#define RTSP_METHOD_DESCRIBE "DESCRIBE"
#define RTSP_METHOD_SETUP "SETUP"
#define RTSP_METHOD_PLAY "PLAY"
#define RTSP_METHOD_TEARDOWN "TEARDOWN"
#define RTSP_METHOD_SET_PARAMETER "SET_PARAMETER"
#define RTSP_METHOD_GET_PARAMETER "GET_PARAMETER"
#define RTSP_CLIENT_VERSION "Hisilicon Streaming Media Client"
#define RTSP_HEADER_SESSION "Session"
#define RTSP_TRACK_ID "trackID="
#define RTSP_CLIENT_STR "rtsp://"
#define RTSP_OPTIONS_REQ (0)
#define RTSP_DESCRIBE_REQ (1)
#define RTSP_SETUP_REQ (2)
#define RTSP_PLAY_REQ (3)
#define RTSP_TEARDOWN_REQ (4)
#define RTSP_AUTHOR_ERROR (401)
#define RTSP_CLIENT_LISTENER_PORT (554)
#define RTSP_CLIENT_IP_LEN (22)
#define RTSP_CLIENT_MAX_STREAMHEADLEN   (512)
#define RTSP_CLIENT_ITLV_HEAD (0x24)
#define RTSP_CLIENT_ADTS_HEAD_LENGTH (7)
#define RTSP_CLIENT_AAC_HEAD_LENGTH (16)
#define RTSP_VER_MAX_LEN  (16)
#define RTSP_METHOD_MAX_LEN   (32)
#define RTSP_URL_MAX_LEN   (256)
#define RTSP_MAX_ONLINE_USER     (16)
#define RTSP_IPADDE_LEN          (16)
#define RTSP_TIME_LEN            (32)
#define RTSP_SPS_MAX_LEN         (128)
#define RTSP_PPS_MAX_LEN         (128)
#define RTSP_VPS_MAX_LEN         (128)
#define RTSP_PASSWORD_MAX_LEN    (256)
#define RTSP_USERNAME_MAX_LEN    (256)
#define RTSP_AGENTBUF_MAX_LEN    (128)
#define RTSP_RANGE_MAX_LEN       (64)
#define RTSP_OBJ_MAX_LEN         (256)
#define RTSP_UNCARE_MAX_LEN      (256)
#define RTSP_IP_MAX_LEN          (16)
#define RTSP_COOK_MAX_LEN        (64)
#define RTSP_CHAR_MAX_LEN        (32)
#define RTSP_MAX_PROTOCOL_BUFFER (1024)
#define RTSP_MAX_STREAMNAME_LEN (1024)
#define RTSPCLIENT_SCANF_RET_ONE (1)
#define RTSPCLIENT_SCANF_RET_TWO (2)
#define RTSPCLIENT_SCANF_RET_THREE (3)
#define RTSPCLIENT_AUDIO_CHANNEL_SINGLE (1)
#define RTSPCLIENT_AUDIO_CHANNEL_DOUBLE (2)

/** audio sample rate*/
typedef enum hiRTSPCLIENT_AUDIO_SAMPLE_RATE_E
{
    HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_8     = 8000,   /* 8K Sample rate     */
    HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_11025 = 11025,   /* 11.025K Sample rate*/
    HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_16    = 16000,   /* 16K Sample rate    */
    HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_22050 = 22050,   /* 22.050K Sample rate*/
    HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_24    = 24000,   /* 24K Sample rate    */
    HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_32    = 32000,   /* 32K Sample rate    */
    HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_441   = 44100,   /* 44.1K Sample rate  */
    HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_48    = 48000,   /* 48K Sample rate    */
    HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_96    = 96000,   /* 96K Sample rate    */
    HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_BUTT
} HI_RTSPCLIENT_AUDIO_SAMPLE_RATE_E;
/**
aac low profile
8k     single chn 1588  double chn 1590
16k     single chn 1408  double chn  1410
22.05K  single chn 1388  double chn  1390
24K     single chn 1308  double chn  1310
32k     single chn 1288  double chn  1290
44.1k   single chn 1208  double chn  1210
48k   single chn 1188  double chn  1190
*/
typedef enum hiRTSPCLIENT_AUDIO_CONFIGNUM_E
{
    HI_RTSPCLIENT_AUDIO_CONFIGNUM_8_SINGLECHN     = 1588,   /**< 8K Sample rate config num    */
    HI_RTSPCLIENT_AUDIO_CONFIGNUM_16_SINGLECHN    = 1408,   /* *<16K Sample rate config num      */
    HI_RTSPCLIENT_AUDIO_CONFIGNUM_22050_SINGLECHN = 1388,   /**<22.050K Sample rate config num  */
    HI_RTSPCLIENT_AUDIO_CONFIGNUM_24_SINGLECHN    = 1308,   /* *<24K Sample rate config num      */
    HI_RTSPCLIENT_AUDIO_CONFIGNUM_32_SINGLECHN    = 1288,   /**< 32K Sample rate config num      */
    HI_RTSPCLIENT_AUDIO_CONFIGNUM_441_SINGLECHN   = 1208,   /**< 44.1K Sample rate config num    */
    HI_RTSPCLIENT_AUDIO_CONFIGNUM_48_SINGLECHN    = 1188,   /* *<48K Sample rate config num      */
    HI_RTSPCLIENT_AUDIO_CONFIGNUM_8_DOUBLECHN     = 1590,   /**< 8K Sample rate  config num      */
    HI_RTSPCLIENT_AUDIO_CONFIGNUM_16_DOUBLECHN    = 1410,   /**<16K Sample rate config num     */
    HI_RTSPCLIENT_AUDIO_CONFIGNUM_22050_DOUBLECHN = 1390,   /**< 22.050K Sample rate config num  */
    HI_RTSPCLIENT_AUDIO_CONFIGNUM_24_DOUBLECHN    = 1310,   /**<24K Sample rate  config num     */
    HI_RTSPCLIENT_AUDIO_CONFIGNUM_32_DOUBLECHN    = 1290,   /* *<32K Sample rate   config num    */
    HI_RTSPCLIENT_AUDIO_CONFIGNUM_441_DOUBLECHN   = 1210,   /* *<44.1K Sample rate  config num   */
    HI_RTSPCLIENT_AUDIO_CONFIGNUM_48_DOUBLECHN    = 1190,   /* *<48K Sample rate config num      */
    HI_RTSPCLIENT_AUDIO_CONFIGNUM_BUTT
} HI_RTSPCLIENT_AUDIO_CONFIGNUM_E;


/**
 * @brief  rtspclient get user agent version.
 * @param[in,out] pszVerStr HI_CHAR* : user agent version string
 * @return  void
 */
HI_VOID RTSPCLIENT_MSGParser_GetPlayerVers(HI_CHAR* pszVerStr);

/**
 * @brief  rtspclient get sps pps data.
 * @param[in] pszInBuf HI_CHAR* : msg in data
 * @param[in,out] pszSpsBase64 HI_CHAR* : sps base64 msg
 * @param[in,out] pu32SpsLen HI_U32* : sps base64 length
 * @param[in,out] pszPpsBase64 HI_CHAR* : pps base64 msg
 * @param[in,out] pu32PpsLen HI_U32* : pps base64 length
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 RTSPCLIENT_MSGParser_GetSPSPPS(HI_CHAR* pszInBuf, HI_CHAR* pszSpsBase64, HI_U32* pu32SpsLen , HI_CHAR* pszPpsBase64, HI_U32* pu32PpsLen);

/**
 * @brief  rtspclient get time out config from server.
 * @param[in] pszInBuf HI_CHAR* : in msg buffer
  * @param[in,out] ps32Timeout HI_S32* : time out
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 RTSPCLIENT_MSGParser_GetTimeout(HI_CHAR* pszInBuf, HI_S32* ps32Timeout);

/**
 * @brief  rtspclient get session id.
 * @param[in] pMsgStr HI_CHAR* : in msg buffer
  * @param[in,out] pszSessId HI_CHAR* : session id
 * @return   0 success
 * @return  -1 failure
 */
HI_S32 RTSPCLIENT_MSGParser_GetSessID(const HI_CHAR* pMsgStr, HI_CHAR* pszSessId);

/**
 * @brief  rtspclient get arg name.
 * @param[in] pszBuffer const HI_CHAR* : in msg buffer
 * @param[in] pszArgName HI_CHAR* : arg name string
 * @return   0 success
 * @return  -1 failure
 */
HI_CHAR* RTSPCLIENT_MSGParser_SDP_GetPos(const HI_CHAR* pszBuffer, HI_CHAR* pszArgName);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /*__RTSPCLIENT_MSG_PARSER_H__*/
