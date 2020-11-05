/*
 *  onLoad.cpp
*/
#define TAG "hi_ffmpeg_onLoad"

#include <stdlib.h>
#include <android/log.h>
#include "jniUtils.h"


#ifdef BUILD_WITH_PLAYER
extern int register_android_media_FFMpegPlayerAndroid(JNIEnv *hi_env);
#endif

static JavaVM *hi_sVm;


int jniThrowException(JNIEnv* hi_env, const char* hi_className, const char* hi_msg) {
    jclass exceptionClass = hi_env->FindClass(hi_className);
    if (exceptionClass == NULL) {
        __android_log_print(ANDROID_LOG_ERROR,
                TAG,
                "Unable to find exception class %s",
                        hi_className);
        return -1;
    }

    if (hi_env->ThrowNew(exceptionClass, hi_msg) != JNI_OK) {
        __android_log_print(ANDROID_LOG_ERROR,
                TAG,
                "Failed throwing '%s' '%s'",
                hi_className, hi_msg);
    }
    return 0;
}

JNIEnv* getJNIEnv() {
    JNIEnv* hi_env = NULL;
    jint getEnvSuccess = hi_sVm->GetEnv( (void**) &hi_env, JNI_VERSION_1_4);
    if(getEnvSuccess == JNI_OK)
    {
        return hi_env;
    }
    else if (getEnvSuccess == JNI_EDETACHED)
    {
        __android_log_print(ANDROID_LOG_ERROR,TAG,"AttachCurrentThread");
        jint attachSuccess = hi_sVm->AttachCurrentThread(&hi_env, NULL);
        if(attachSuccess == 0)
        {
            __android_log_print(ANDROID_LOG_ERROR,TAG,"AttachCurrentThread OK");
            return hi_env;
        }
        else
        {
            __android_log_print(ANDROID_LOG_ERROR,TAG,"AttachCurrentThread failed");
            return NULL;
        }//end if/else statement
    }
    else
    {
        __android_log_print(ANDROID_LOG_ERROR,TAG,"obtain JNIEnv failed");
        return NULL;
    }//end if/else statement
}
bool detachThreadFromJNI()
{
    JNIEnv* hi_env = NULL;
    jint getEnvSuccess = hi_sVm->GetEnv( (void**) &hi_env, JNI_VERSION_1_4);
    if(getEnvSuccess == JNI_OK)
    {
        hi_sVm->DetachCurrentThread();
        return true;
    }
    else
    {
        __android_log_print(ANDROID_LOG_ERROR,TAG,"detachThreadFromJNI: obtain JNIEnv failed");
        return false;
    }//end if/else statement
}


int jniRegisterNativeMethods(JNIEnv* hi_env,
                             const char* hi_className,
                             const JNINativeMethod* hi_gMethods,
                             int hi_numMethods)
{
    jclass hi_clazz;

    hi_clazz = hi_env->FindClass(hi_className);
    if (hi_clazz == NULL)
    {
        return -1;
    }

    if (hi_env->RegisterNatives(hi_clazz, hi_gMethods, hi_numMethods) < 0)
    {
        return -1;
    }

    return 0;
}

jint JNI_OnLoad(JavaVM* hi_vm, void* hi_reserved) {
    JNIEnv* hi_env = NULL;
    hi_sVm = hi_vm;

    if (hi_vm->GetEnv((void**) &hi_env, JNI_VERSION_1_4) != JNI_OK)
    {
        return JNI_ERR;
    }

#ifdef BUILD_WITH_PLAYER
    if(register_android_media_FFMpegPlayerAndroid(hi_env) != JNI_OK)
    {
        return JNI_ERR;
    }
#endif

    return JNI_VERSION_1_4;
}
