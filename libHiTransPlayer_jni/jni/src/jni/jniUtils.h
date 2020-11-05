#ifndef _HI_JNI_UTILS_H_
#define _HI_JNI_UTILS_H_

#include <stdlib.h>
#include <jni.h>

#ifdef __cplusplus
extern "C"
{
#endif

int jniThrowException(JNIEnv* hi_env, const char* hi_className, const char* hi_msg);

JNIEnv* getJNIEnv(void);
bool detachThreadFromJNI(void);

int jniRegisterNativeMethods(JNIEnv* hi_env,
                             const char* hi_className,
                             const JNINativeMethod* hi_gMethods,
                             int hi_numMethods);

#ifdef __cplusplus
}
#endif

#endif /* _HI_JNI_UTILS_H_ */
