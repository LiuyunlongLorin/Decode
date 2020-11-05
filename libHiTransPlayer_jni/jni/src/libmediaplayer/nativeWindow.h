#ifndef __NATIVE_WINDOW__
#define __NATIVE_WINDOW__


#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include <jni.h>

#include <android/native_window.h>

typedef int PF_ANWindow_setBuffersGeometry(void* pHandle, int width, int height, int format);
typedef struct __AndroidNativeWindow
{
    ANativeWindow* pWindow;
    void* pPixelCache;
    int mVideoWidth;
    int mVideoHeight;
    int mPixelFormat;
    int mBytesPerPixels;
    int mWindowWidth;
    int mWindowHeight;
    int mVideoLineSize;
    int mWindowLineSize;
    int mAndroidAPIVersion;//<9����2.3���µĲ���ϵͳ��>9����2.3���Ժ�Ĳ���ϵͳ
    PF_ANWindow_setBuffersGeometry* pf_setBuffersGeometry;
    void* pPlayer;
}ANWindow;

void* AndroidNativeWindow_register(JNIEnv* env, jobject jsurface,int apiVersion,PF_ANWindow_setBuffersGeometry* pf_setBuffersGeometry,void* pPlayer);

int AndroidNativeWindow_getPixels(void* pHandle,int width, int height, void** pixels,int format);

int AndroidNativeWindow_update(void* pHandle,void* pPixels,int width,int height,int format);

int AndroidNativeWindow_unregister(void* pHandle);

#ifdef __cplusplus
}
#endif
#endif
