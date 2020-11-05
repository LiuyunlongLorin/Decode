/*
 * Copyright (C) 2011 
*/
#include "nativeWindow.h"
#include "string.h"
#include <stdlib.h>
#include <android/native_window_jni.h>
#include <android/log.h>

#define TAG "AndroidNativeWindow"


void* AndroidNativeWindow_register(JNIEnv* env, jobject jsurface,int apiVersion,
    PF_ANWindow_setBuffersGeometry* pf_setBuffersGeometry,void* pPlayer)
{
    ANWindow* pANWindow = (ANWindow*)malloc(sizeof(ANWindow));
    if(!pANWindow)
    {
        return NULL;
    }
    memset(pANWindow,0,sizeof(ANWindow));
    pANWindow->mAndroidAPIVersion = apiVersion;
    if(pANWindow->mAndroidAPIVersion < 9)
    {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "AndroidNativeWindow_register not supported for android 2.2");
        return NULL;
    }

    pANWindow->pWindow = ANativeWindow_fromSurface(env, jsurface);
    return pANWindow;
}

int AndroidNativeWindow_getPixels(void* pHandle,int width, int height, void** pixels,int format)
{
    ANWindow* pANWindow = (ANWindow*)pHandle;
    pANWindow->mWindowWidth = width/32*32;
    pANWindow->mWindowHeight = height;
    pANWindow->mVideoWidth = width;
    pANWindow->mVideoHeight = height;
    pANWindow->mPixelFormat = format;

    int ret = ANativeWindow_setBuffersGeometry((pANWindow->pWindow),
        pANWindow->mWindowWidth, pANWindow->mWindowHeight, pANWindow->mPixelFormat);
    if(ret < 0)
    {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "AndroidNativeWindow_getPixels failed");
        return ret;
    }
    else
    {
        if(format == WINDOW_FORMAT_RGBA_8888)
        {
            *pixels = malloc(width*height*4);
            pANWindow->mBytesPerPixels = 4;
            pANWindow->pPixelCache = *pixels;
        }
        else if(format == WINDOW_FORMAT_RGB_565)
        {
            *pixels = malloc(width*height*2);
            pANWindow->mBytesPerPixels = 2;
            pANWindow->pPixelCache = *pixels;
        }
        pANWindow->mWindowLineSize = pANWindow->mWindowWidth*pANWindow->mBytesPerPixels;
        pANWindow->mVideoLineSize = pANWindow->mVideoWidth*pANWindow->mBytesPerPixels;
        if(*pixels)
        {
            return 0;
        }
        else
        {
            return -1;
        }
    }
}

int AndroidNativeWindow_update(void* pHandle,void* pPixels,int width,int height,int format)
{
    ANWindow* pANWindow = (ANWindow*)pHandle;
    ANativeWindow_Buffer buffer;
//    __android_log_print(ANDROID_LOG_ERROR, TAG, "ANativeWindow_lock start");
     if(ANativeWindow_lock((pANWindow->pWindow), &buffer, NULL) < 0)
     {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "ANativeWindow_lock failed");
         return -1;
    }

    int i = 0;
    char* pSrc = (char*)pPixels;
    char* pDst = (char*)(buffer.bits);
    {
        //__android_log_print(ANDROID_LOG_ERROR, TAG, "ANativeWindow fill:%d,%d,%d,%d",
        //buffer.width,buffer.height,buffer.stride,buffer.format);

        /*this case limit rgb565*/
        if(buffer.stride != pANWindow->mWindowWidth)
        {
            for(i = 0;i<height;i++)
            {
                memcpy(pDst,pSrc,pANWindow->mVideoLineSize);
                pDst += buffer.stride*2;
                pSrc += pANWindow->mVideoLineSize;
            }
        }
        else
        {
            if(pANWindow->mWindowLineSize == pANWindow->mVideoLineSize)
            {
                memcpy(pDst,pSrc,pANWindow->mWindowLineSize*height);
            }
            else
            {
                for(i = 0;i<height;i++)
                {
                    memcpy(pDst,pSrc,pANWindow->mWindowLineSize);
                    pDst += pANWindow->mWindowLineSize;
                    pSrc += pANWindow->mVideoLineSize;
                }
            }
        }
    }
    int ret = ANativeWindow_unlockAndPost(pANWindow->pWindow);
    if(ret < 0)
    {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "ANativeWindow_unlockAndPost Failed");
    }
    return ret;
}

int AndroidNativeWindow_unregister(void* pHandle)
{
    __android_log_print(ANDROID_LOG_ERROR, TAG, "AndroidNativeWindow_unregister start");
    ANWindow* pANWindow = (ANWindow*)pHandle;
    ANativeWindow_release(pANWindow->pWindow);
    free(pANWindow);
    __android_log_print(ANDROID_LOG_ERROR, TAG, "AndroidNativeWindow_unregister end");

    return 0;
}
