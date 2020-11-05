/**
* Copyright (C), 2016-2030, Hisilicon Tech. Co., Ltd.
* All rights reserved.
*
* @file      hi_rtsp_client_err.h
* @brief     rtspclient module header file
* @author    HiMobileCam middleware develop team
* @date      2016.09.10
*/

#ifndef __HI_RTSP_CLIENT_ERR_H__
#define __HI_RTSP_CLIENT_ERR_H__
#include "hi_error_def.h"
#include "hi_defs.h"


#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

/* __cplusplus */
/** \addtogroup     RTSPCLIENT */
/** @{ */  /** <!-- [RTSPCLIENT] */

typedef enum hiAPP_RTSPSERVER_ERR_CODE_E
{
    /*general error code*/
    APP_RTSPCLIENT_ERR_HANDLE_INVALID = 0x40,                     /**<rtspclient handle invalid*/
    APP_RTSPCLIENT_ERR_INVALID_ARG = 0x41,                             /**<param is null or invalid*/
    APP_RTSPCLIENT_ERR_MALLOC_FAIL = 0x42,                             /**<malloc memory fail*/
    APP_RTSPCLIENT_ERR_CREATE_FAIL = 0x43,                               /**<create rtspclient fail*/
    APP_RTSPCLIENT_ERR_DESTORY_FAIL = 0x44,                            /**<destory rtspclient  fail*/
    APP_RTSPCLIENT_ERR_START_FAIL = 0x45,                                 /**<start rtspclient fail*/
    APP_RTSPCLIENT_ERR_STOP_FAIL  = 0x46,                                  /**<stop rtspclient fail*/
    APP_RTSPCLIENT_ERR_CREATE_AGAIN  = 0x48,                        /**<rtspclient have been created*/
    APP_RTSPCLIENT_ERR_NOT_CREATE = 0x49,                              /**<rtspclient have been created*/
    /*client related error code*/
    APP_RTSPCLIENT_ERR_CLIENT_REMOVE_FAIL = 0x50,               /**<remove stream fail*/
    APP_RTSPCLIENT_ERR_CLIENT_ADD_FAIL = 0x51,                        /**<add stream fail*/
    APP_RTSPCLIENT_ERR_CLIENT_EXISTED = 0x52,                            /**<add stream existed*/
    APP_RTSPCLIENT_ERR_CLIENT_NOT_EXIST = 0x53,                      /**<remove or find stream not existed*/

    APP_RTSPCLIENT_BUTT = 0xFF
} HI_APP_RTSPCLIENT_ERR_CODE_E;


/*general error code*/
#define HI_ERR_RTSPCLIENT_NULL_PTR                                       HI_APP_DEF_ERR(HI_APPID_RTSPCLIENT,APP_ERR_LEVEL_ERROR,APP_ERR_NULL_PTR)
#define HI_ERR_RTSPCLIENT_HANDLE_INVALID               HI_APP_DEF_ERR(HI_APPID_RTSPCLIENT,APP_ERR_LEVEL_ERROR,APP_RTSPCLIENT_ERR_HANDLE_INVALID)
#define HI_ERR_RTSPCLIENT_ILLEGAL_PARAM                          HI_APP_DEF_ERR(HI_APPID_RTSPCLIENT,APP_ERR_LEVEL_ERROR,APP_RTSPCLIENT_ERR_INVALID_ARG)
#define HI_ERR_RTSPCLIENT_MALLOC_FAIL                       HI_APP_DEF_ERR(HI_APPID_RTSPCLIENT,APP_ERR_LEVEL_ERROR,APP_RTSPCLIENT_ERR_MALLOC_FAIL)
#define HI_ERR_RTSPCLIENT_CREATE_FAIL                        HI_APP_DEF_ERR(HI_APPID_RTSPCLIENT,APP_ERR_LEVEL_ERROR,APP_RTSPCLIENT_ERR_CREATE_FAIL)
#define HI_ERR_RTSPCLIENT_DESTROY_FAIL                     HI_APP_DEF_ERR(HI_APPID_RTSPCLIENT,APP_ERR_LEVEL_ERROR,APP_RTSPCLIENT_ERR_DESTORY_FAIL)
#define HI_ERR_RTSPCLIENT_START_FAIL                           HI_APP_DEF_ERR(HI_APPID_RTSPCLIENT,APP_ERR_LEVEL_ERROR,APP_RTSPCLIENT_ERR_START_FAIL)
#define HI_ERR_RTSPCLIENT_STOP_FAIL                              HI_APP_DEF_ERR(HI_APPID_RTSPCLIENT,APP_ERR_LEVEL_ERROR,APP_RTSPCLIENT_ERR_STOP_FAIL)
#define HI_ERR_RTSPCLIENT_CREATE_AGAIN                    HI_APP_DEF_ERR(HI_APPID_RTSPCLIENT,APP_ERR_LEVEL_ERROR,APP_RTSPCLIENT_ERR_CREATE_AGAIN)
#define HI_ERR_RTSPCLIENT_NOT_CREATE                         HI_APP_DEF_ERR(HI_APPID_RTSPCLIENT,APP_ERR_LEVEL_ERROR,APP_RTSPCLIENT_ERR_NOT_CREATE)

/*stream related error code*/
#define HI_ERR_RTSPCLIENT_CLIENT_REMOVE_FAIL                          HI_APP_DEF_ERR(HI_APPID_RTSPCLIENT,APP_ERR_LEVEL_ERROR,APP_RTSPCLIENT_ERR_CLIENT_REMOVE_FAIL )
#define HI_ERR_RTSPCLIENT_CLIENT_ADD_FAIL                       HI_APP_DEF_ERR(HI_APPID_RTSPCLIENT,APP_ERR_LEVEL_ERROR,APP_RTSPCLIENT_ERR_CLIENT_ADD_FAIL )
#define HI_ERR_RTSPCLIENT_CLIENT_EXISTED                        HI_APP_DEF_ERR(HI_APPID_RTSPCLIENT,APP_ERR_LEVEL_ERROR,APP_RTSPCLIENT_ERR_CLIENT_EXISTED)
#define HI_ERR_RTSPCLIENT_CLIENT_NOT_EXIST                      HI_APP_DEF_ERR(HI_APPID_RTSPCLIENT,APP_ERR_LEVEL_ERROR,APP_RTSPCLIENT_ERR_CLIENT_NOT_EXIST)

/** @}*/  /** <!-- ==== RTSPCLIENT End ====*/

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif /* __HI_RTSP_CLIENT_ERR_H__ */
