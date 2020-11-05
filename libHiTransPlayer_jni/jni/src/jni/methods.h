#ifndef _HI_METHODS_H_
#define _HI_METHODS_H_

#include <jni.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include "libavformat/avformat.h"

jclass AVFormatContext_getClass(JNIEnv *hi_env);
const char *AVInputFormat_getClassSignature(void);

jobject AVFormatContext_create(JNIEnv *hi_env, AVFormatContext *hi_fileContext);
jobject *AVRational_create(JNIEnv *hi_env, AVRational *hi_rational);
jobject *AVInputFormat_create(JNIEnv *hi_env, AVInputFormat *hi_format);
jobject AVCodecContext_create(JNIEnv *hi_env, AVCodecContext *hi_codecContext);


#ifdef __cplusplus
}
#endif

#endif /* _HI_METHODS_H_ */
