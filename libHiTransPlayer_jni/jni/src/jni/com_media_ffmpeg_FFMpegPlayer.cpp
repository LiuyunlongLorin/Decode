/**
*  com_media_ffmpeg_FFMpegPlayer.cpp
**/

#include <android/log.h>
#include "jniUtils.h"
#include "methods.h"
#include "mediaplayer.h"
#include "HiMLoger.h"

#define TAG "HI_JNI_FFMediaPlayer"

struct hi_fields_t
{
    jfieldID    context;
    jfieldID    jniCtx;
    jmethodID   post_event;
    jmethodID   write_pcm;
    jmethodID   config_audioTrack;
    jmethodID   audio_flush;
};




static hi_fields_t hi_fields;
static JNIEnv* hi_jniEnv = NULL;
static const char* const hi_kClassPathName = "com/hisilicon/camplayer/HiCamPlayer";
static const char* const hi_kFramePathName = "com/hisilicon/camplayer/HiCamPlayer$RecFrameInfo";
static const char* const hi_kYuvPathName = "com/hisilicon/camplayer/HiCamPlayer$YuvFrameInfo";
static const char* const hi_kExcepClass = "java/lang/RuntimeException";




//used to limit only one mediaplayer obj exist one time
static int playerObjCnt = 0;

/*Android Audio Track control class, include the attribute of AudioTrack Configration, and writting in PCM audio data*/
class JNIWritePCM: public MediaPlayerPCMWriter
{
public:
    JNIWritePCM(JNIEnv* hi_env, jobject hi_thiz, jobject hi_weak_thiz);
    ~JNIWritePCM();
    void writePCM(unsigned char* hi_buf, int hi_bufCount);
    int configAudioTrack(int hi_streamType, int hi_sampleRate, int hi_channelConfig, int hi_bytesPerSample, int hi_trackMode );
    void flush();

private:
    JNIWritePCM();
    jclass      mClass;     // Reference to MediaPlayer class
    jobject     mObject;    // Weak ref to MediaPlayer Java object to call on

};
JNIWritePCM::JNIWritePCM(JNIEnv* hi_env, jobject hi_thiz, jobject hi_weak_thiz)
{

    mClass = NULL;
    mObject = NULL;
    jclass clazz = hi_env->GetObjectClass(hi_thiz);

    if (clazz == NULL)
    {
        jniThrowException(hi_env, "java/lang/Exception", hi_kClassPathName);
        return;
    }

    mClass = (jclass)hi_env->NewGlobalRef(clazz);
    mObject  = hi_env->NewGlobalRef(hi_weak_thiz);
}
JNIWritePCM::~JNIWritePCM()
{
    JNIEnv* hi_env = getJNIEnv();

    if (hi_env == NULL)
    { hi_env = hi_jniEnv; }

    hi_env->DeleteGlobalRef(mObject);
    hi_env->DeleteGlobalRef(mClass);
}
int JNIWritePCM::configAudioTrack(int hi_streamType, int hi_sampleRate, int hi_channelConfig, int hi_bytesPerSample, int hi_trackMode )
{
    MMLOGI( TAG,
            "setting audio track:streamType %d,sampleRate %d,channels %d,format %d,trackMode %d",
            hi_streamType, hi_sampleRate, hi_channelConfig, hi_bytesPerSample, hi_trackMode);
    JNIEnv* hi_env = getJNIEnv();

    if (hi_env == NULL)
    { hi_env = hi_jniEnv; }

    return hi_env->CallStaticIntMethod(mClass, hi_fields.config_audioTrack, hi_streamType, hi_sampleRate,
                                       hi_channelConfig, hi_bytesPerSample, hi_trackMode);
}

void JNIWritePCM::flush(void)
{
    JNIEnv* hi_env = getJNIEnv();

    if (hi_env == NULL)
    { hi_env = hi_jniEnv; }

    hi_env->CallStaticVoidMethod(mClass, hi_fields.audio_flush);
}

void JNIWritePCM::writePCM(unsigned char* hi_buf, int hi_bufCount)
{
    //    MMLOGI( TAG, "start writePCM");
    JNIEnv* hi_env = getJNIEnv();

    if (hi_env == NULL)
    { hi_env = hi_jniEnv; }

    if (hi_buf == NULL)
    {
        detachThreadFromJNI();
        MMLOGI( TAG,
                "Detach current thread for thread end!!\n");
        return;
    }

    jbyteArray array = hi_env->NewByteArray( hi_bufCount);
    hi_env->SetByteArrayRegion(array, 0, hi_bufCount, (jbyte*)hi_buf);
    hi_env->CallStaticVoidMethod(mClass, hi_fields.write_pcm, array);
    hi_env->DeleteLocalRef(array);
}
// ----------------------------------------------------------------------------
// ref-counted object for callbacks
/*the interfaces to callback event of player */
class HiMediaPlayerListener: public MediaPlayerListener
{
public:
    HiMediaPlayerListener(JNIEnv* hi_env, jobject hi_thiz, jobject hi_weak_thiz);
    ~HiMediaPlayerListener();
    void notify(int hi_msg, int hi_ext1, int hi_ext2);
private:
    HiMediaPlayerListener();
    jclass      mClass;     // Reference to MediaPlayer class
    jobject     mObject;    // Weak ref to MediaPlayer Java object to call on
};

HiMediaPlayerListener::HiMediaPlayerListener(JNIEnv* hi_env, jobject hi_thiz, jobject hi_weak_thiz)
{
    mClass = NULL;
    mObject = NULL;
    jclass clazz = hi_env->GetObjectClass(hi_thiz);

    if (clazz == NULL)
    {
        jniThrowException(hi_env, "java/lang/Exception", hi_kClassPathName);
        return;
    }

    mClass = (jclass)hi_env->NewGlobalRef(clazz);
    mObject  = hi_env->NewGlobalRef(hi_weak_thiz);
}

HiMediaPlayerListener::~HiMediaPlayerListener()
{
    JNIEnv* hi_env = getJNIEnv();

    if (hi_env == NULL)
    { hi_env = hi_jniEnv; }

    hi_env->DeleteGlobalRef(mObject);
    hi_env->DeleteGlobalRef(mClass);
}

void HiMediaPlayerListener::notify(int hi_msg, int hi_ext1, int hi_ext2)
{
    JNIEnv* hi_env = getJNIEnv();

    if (hi_env == NULL)
    { hi_env = hi_jniEnv; }

    if ((hi_msg == 0) && (hi_ext1 == -1) && (hi_ext2 == -1))
    {
        detachThreadFromJNI();
        MMLOGI( TAG, "Detach current thread for thread end!!\n");
        return;
    }

    hi_env->CallStaticVoidMethod(mClass, hi_fields.post_event, mObject, hi_msg, hi_ext1, hi_ext2, 0);
}

// ----------------------------------------------------------------------------

class HiJNIMediaPlayerContext
{
public:
    HiJNIMediaPlayerContext(HiMediaPlayerListener* hi_listener, JNIWritePCM* hi_pcmWriter)
        : mPlayerListener(hi_listener),
          mPcmWriter(hi_pcmWriter),
          mSurface(NULL) {};

    HiMediaPlayerListener*      mPlayerListener;
    jobject     mSurface;
    JNIWritePCM* mPcmWriter;
};

// ----------------------------------------------------------------------------

static MediaPlayer* getMediaPlayer(JNIEnv* hi_env, jobject hi_thiz)
{
    return (MediaPlayer*)hi_env->GetIntField(hi_thiz, hi_fields.context);
}

static void setMediaPlayer(JNIEnv* hi_env, jobject hi_thiz, MediaPlayer* hi_player)
{
    MediaPlayer* old = (MediaPlayer*)hi_env->GetIntField(hi_thiz, hi_fields.context);

    if (old != NULL)
    {
        MMLOGI( TAG, "freeing cur native mediaplayer object");
        delete old;
    }

    hi_env->SetIntField(hi_thiz, hi_fields.context, (int)hi_player);
}


static void process_media_player_call(JNIEnv* hi_env, jobject hi_thiz, status_t hi_opStatus, const char* hi_exception, const char* hi_message)
{
    if (hi_exception != NULL)
    {
        if ( hi_opStatus == (status_t) INVALID_OPERATION )
        {
            jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        }
        else if ( hi_opStatus != (status_t) NO_ERROR )
        {
            if (strlen(hi_message) > 230)
            {
                jniThrowException( hi_env, hi_exception, hi_message);
            }
            else
            {
                char msg[256];
                jniThrowException( hi_env, hi_exception, msg);
            }
        }
    }
}


/* set the server url*/
static void
com_media_ffmpeg_FFMpegPlayer_setDataSource(JNIEnv* hi_env, jobject hi_thiz, jstring hi_path)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return;
    }

    if (hi_path == NULL)
    {
        jniThrowException(hi_env, "java/lang/IllegalArgumentException", NULL);
        return;
    }

    const char* hi_pathStr = hi_env->GetStringUTFChars(hi_path, NULL);

    if (hi_pathStr == NULL)    // Out of memory
    {
        jniThrowException(hi_env, hi_kExcepClass, "Out of memory");
        return;
    }

    MMLOGI( TAG, "setDataSource: path %s", hi_pathStr);
    status_t hi_opStatus = hi_mp->setDataSource(hi_pathStr);

    // Make sure that local ref is released before a potential exception
    hi_env->ReleaseStringUTFChars(hi_path, hi_pathStr);

    process_media_player_call(
        hi_env, hi_thiz, hi_opStatus, "java/io/IOException",
        "setDataSource failed." );
}

/*deliver from java to cpp through jni, register this surface for video display*/
static void
com_media_ffmpeg_FFMpegPlayer_setVideoSurface(JNIEnv* hi_env, jobject hi_thiz, jobject hi_jsurface, int hi_apiVersion)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return;
    }

    if (hi_jsurface == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return;
    }

    HiJNIMediaPlayerContext* hi_pCtx = (HiJNIMediaPlayerContext*)hi_env->GetIntField(hi_thiz, hi_fields.jniCtx);

    if (hi_pCtx->mSurface)
    {
        MMLOGI(TAG, "DeleteGlobalRef of surface!!");
        hi_env->DeleteGlobalRef(hi_pCtx->mSurface);
    }

    MMLOGI(TAG, "NewGlobalRef of surface!!");
    hi_pCtx->mSurface = (jobject)hi_env->NewGlobalRef(hi_jsurface);
    hi_mp->setAPIVersion(hi_apiVersion);
    process_media_player_call(hi_env, hi_thiz, hi_mp->setVideoSurface(hi_env, hi_pCtx->mSurface, hi_apiVersion),
                              "java/io/IOException", "Set video surface failed.");
}

/* config some parameters of audio and video*/
static void
com_media_ffmpeg_FFMpegPlayer_prepare(JNIEnv* hi_env, jobject hi_thiz)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return;
    }

    process_media_player_call(hi_env, hi_thiz, hi_mp->prepare(), "java/io/IOException", "Prepare failed.");
}

/*start play, create thread for play*/
static void
com_media_ffmpeg_FFMpegPlayer_start(JNIEnv* hi_env, jobject hi_thiz)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return;
    }

    process_media_player_call(hi_env, hi_thiz, hi_mp->start(), "java/lang/IllegalStateException", "start failed");
}

/*stop play, destroy thread for play*/
static void
com_media_ffmpeg_FFMpegPlayer_stop(JNIEnv* hi_env, jobject hi_thiz)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return;
    }

    process_media_player_call(hi_env, hi_thiz, hi_mp->stop(), NULL, NULL);
}

/*pause play*/
static void
com_media_ffmpeg_FFMpegPlayer_pause(JNIEnv* hi_env, jobject hi_thiz)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return;
    }

    process_media_player_call( hi_env, hi_thiz, hi_mp->pause(), NULL, NULL );
}

static jboolean
com_media_ffmpeg_FFMpegPlayer_isPlaying(JNIEnv* hi_env, jobject hi_thiz)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        MMLOGI(TAG, "native player is not exist, isPlaying return false");
        return false;
    }

    const jboolean hi_is_playing = hi_mp->isPlaying();
    return hi_is_playing;
}

/*no use*/
static void
com_media_ffmpeg_FFMpegPlayer_seekTo(JNIEnv* hi_env, jobject hi_thiz, int hi_msec)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return;
    }

    process_media_player_call( hi_env, hi_thiz, hi_mp->seekTo(hi_msec), NULL, NULL );
}

/*get video width*/
static int
com_media_ffmpeg_FFMpegPlayer_getVideoWidth(JNIEnv* hi_env, jobject hi_thiz)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        MMLOGI( TAG, "native player NULL: getVideoWidth throw java/lang/IllegalStateException");
        return 0;
    }

    int hi_w;

    if (0 != hi_mp->getVideoWidth(&hi_w))
    {
        hi_w = 0;
    }

    return hi_w;
}

/*get video height*/
static int
com_media_ffmpeg_FFMpegPlayer_getVideoHeight(JNIEnv* hi_env, jobject hi_thiz)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        MMLOGI( TAG, "native player NULL: getVideoHeight throw java/lang/IllegalStateException");
        return 0;
    }

    int h;

    if (0 != hi_mp->getVideoHeight(&h))
    {
        h = 0;
    }

    return h;
}

/*no use*/
static int
com_media_ffmpeg_FFMpegPlayer_getCurrentPosition(JNIEnv* hi_env, jobject hi_thiz)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        MMLOGI( TAG, "native player NULL: getCurrentPosition throw java/lang/IllegalStateException");
        return 0;
    }

    int hi_msec;
    process_media_player_call( hi_env, hi_thiz, hi_mp->getCurrentPosition(&hi_msec), NULL, NULL );
    return hi_msec;
}

/*real stream, no use*/
static int
com_media_ffmpeg_FFMpegPlayer_getDuration(JNIEnv* hi_env, jobject hi_thiz)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        MMLOGI( TAG, "native player NULL: getDuration throw java/lang/IllegalStateException");
        return 0;
    }

    int hi_msec;
    process_media_player_call( hi_env, hi_thiz, hi_mp->getDuration(&hi_msec), NULL, NULL );
    return hi_msec;
}

/*no use*/
static void
com_media_ffmpeg_FFMpegPlayer_reset(JNIEnv* hi_env, jobject hi_thiz)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return;
    }

    process_media_player_call(hi_env, hi_thiz, hi_mp->reset(), NULL, NULL);
}

static int
com_media_ffmpeg_FFMpegPlayer_invoke(JNIEnv* hi_env, jobject hi_thiz, int hi_msgId, int hi_what, int hi_extra)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return -1;
    }

    int ret = hi_mp->invoke(hi_msgId, hi_what, hi_extra);
    return ret;
}


/*get the reference of variables and static methods from java */
static void
com_media_ffmpeg_FFMpegPlayer_native_init(JNIEnv* hi_env)
{
    MMLOGI( TAG, "native_init");
    jclass hi_clazz;

    hi_clazz = hi_env->FindClass(hi_kClassPathName);

    if (hi_clazz == NULL)
    {
        jniThrowException(hi_env, hi_kExcepClass, "Can't find com/hisilicon/dvplayer/HiCamPlayer");
        return;
    }

    hi_fields.context = hi_env->GetFieldID(hi_clazz, "mNativeContext", "I");

    if (hi_fields.context == NULL)
    {
        jniThrowException(hi_env, hi_kExcepClass, "Can't find HiCamPlayer.mNativeContext");
        return;
    }

    hi_fields.jniCtx = hi_env->GetFieldID(hi_clazz, "mNativeField", "I");

    if (hi_fields.jniCtx == NULL)
    {
        jniThrowException(hi_env, hi_kExcepClass, "Can't find HiCamPlayer.mNativeField");
        return;
    }

    hi_fields.post_event = hi_env->GetStaticMethodID(hi_clazz, "postEventFromNative",
                           "(Ljava/lang/Object;IIILjava/lang/Object;)V");

    if (hi_fields.post_event == NULL)
    {
        jniThrowException(hi_env, hi_kExcepClass, "Can't find FFMpegMediaPlayer.postEventFromNative");
        return;
    }

    hi_fields.write_pcm = hi_env->GetStaticMethodID(hi_clazz, "writePCM",
                          "([B)V");
    hi_fields.config_audioTrack = hi_env->GetStaticMethodID(hi_clazz, "configATrack",
                                  "(IIIII)I");
    hi_fields.audio_flush = hi_env->GetStaticMethodID(hi_clazz, "audioFlush",
                            "()V");

    if (hi_fields.write_pcm == NULL)
    {
        jniThrowException(hi_env, hi_kExcepClass, "Can't find HiCamPlayer.writePCM");
        return;
    }

    playerObjCnt = 0;
}

/*init instance of player, and some interface for inverse callback*/
static void
com_media_ffmpeg_FFMpegPlayer_native_setup(JNIEnv* hi_env, jobject hi_thiz, jobject hi_weak_this)
{
    MMLOGI( TAG, "native_setup");

    if (playerObjCnt > 0)
    {
        MMLOGE(TAG, "MediaPlayer already be instanced, limit one instance");
        jniThrowException(hi_env, "java/lang/IllegalStateException", "player already instance");
        return;
    }

    MediaPlayer* hi_mp = new MediaPlayer();
    MMLOGI(TAG, "new MediaPlayer()");

    if (hi_mp == NULL)
    {
        jniThrowException(hi_env, hi_kExcepClass, "Out of memory");
        return;
    }

    playerObjCnt ++;
    // create new listener and give it to MediaPlayer
    HiMediaPlayerListener* listener = new HiMediaPlayerListener(hi_env, hi_thiz, hi_weak_this);
    hi_mp->setListener(listener);
    JNIWritePCM* PCMWriter = new JNIWritePCM(hi_env, hi_thiz, hi_weak_this);
    hi_mp->setPCMWriter(PCMWriter);
    HiJNIMediaPlayerContext* jniPlayerCtx = new HiJNIMediaPlayerContext(listener, PCMWriter);
    hi_env->SetIntField(hi_thiz, hi_fields.jniCtx, (int)jniPlayerCtx);
    MMLOGI( TAG, "new JNIWritePCM HiMediaPlayerListener()");
    // Stow our new C++ MediaPlayer in an opaque field in the Java object.
    setMediaPlayer(hi_env, hi_thiz, hi_mp);
    MMLOGI( TAG, "setMediaPlayer");
}

static void
com_media_ffmpeg_FFMpegPlayer_release(JNIEnv* hi_env, jobject hi_thiz)
{
    MMLOGI( TAG, "release");
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return;
    }

    hi_mp->reset();

    HiJNIMediaPlayerContext* hi_pCtx = (HiJNIMediaPlayerContext*)hi_env->GetIntField(hi_thiz, hi_fields.jniCtx);

    if (hi_pCtx->mSurface)
    {
        MMLOGI( TAG, "DeleteGlobalRef of surface!!");
        hi_env->DeleteGlobalRef(hi_pCtx->mSurface);
        hi_pCtx->mSurface = NULL;
    }

    if (hi_pCtx->mPlayerListener)
    {
        delete hi_pCtx->mPlayerListener;
        hi_pCtx->mPlayerListener = NULL;
    }

    if (hi_pCtx->mPcmWriter)
    {
        delete hi_pCtx->mPcmWriter;
        hi_pCtx->mPcmWriter = NULL;
    }

    delete hi_pCtx;
    hi_env->SetIntField(hi_thiz, hi_fields.jniCtx, 0);
    setMediaPlayer(hi_env, hi_thiz, NULL);
    playerObjCnt --;
    MMLOGI( TAG, "released OK");
}

static void com_media_ffmpeg_FFMpegPlayer_setVideoMbufLimit(JNIEnv* hi_env, jobject hi_thiz,
        int hi_dropLimit, int hi_clearLimit)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return;
    }

    hi_mp->setVideoMbufLimit(hi_dropLimit, hi_dropLimit);
}

static void com_media_ffmpeg_FFMpegPlayer_setMaxResolution(JNIEnv* hi_env, jobject hi_thiz, int hi_width, int hi_height)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return;
    }

    hi_mp->setVideoMaxResolution(hi_width, hi_height);
}

static void com_media_ffmpeg_FFMpegPlayer_setSaveDataFlag(JNIEnv* hi_env, jobject hi_thiz, int hi_flag)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return;
    }

    hi_mp->setSaveRemoteDataFlag(hi_flag);
}

static void com_media_ffmpeg_FFMpegPlayer_setLivePlayMode(JNIEnv* hi_env, jobject hi_thiz, int hi_mode)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return;
    }

    hi_mp->setLivePlayMode(hi_mode);
}

static void com_media_ffmpeg_FFMpegPlayer_setRecordFlag(JNIEnv* hi_env, jobject hi_thiz, int hi_flag)
{
    MediaPlayer* hi_mp = getMediaPlayer(hi_env, hi_thiz);

    if (hi_mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return;
    }

    hi_mp->setRecordFlag(hi_flag);
}

static jobject getRecordFrameInfo(JNIEnv* hi_env, jobject hi_thiz, int iDataSize, int64_t pts, int iType)
{
    jclass clazz = hi_env->FindClass(hi_kFramePathName);

    if (clazz == NULL)
    {
        jniThrowException(hi_env, hi_kExcepClass, "can not find hi_kFramePathName");
        return NULL;
    }

    jobject mobject = hi_env->AllocObject(clazz);

    if (mobject == NULL)
    {
        jniThrowException(hi_env, hi_kExcepClass, "AllocObject hi_kFramePathName failed");
        return NULL;
    }

    jfieldID ptsField = hi_env->GetFieldID(clazz, "pts", "J");

    if (ptsField == 0)
    {
        jniThrowException(hi_env, hi_kExcepClass, "can not find RecFrameInfo pts field");
        return NULL;
    }

    jfieldID frameSizeField = hi_env->GetFieldID(clazz, "frameSize", "I");

    if (frameSizeField == 0)
    {
        jniThrowException(hi_env, hi_kExcepClass, "can not find RecFrameInfo frameSize field");
        return NULL;
    }

    jfieldID payloadField = hi_env->GetFieldID(clazz, "payload", "I");

    if (payloadField == 0)
    {
        jniThrowException(hi_env, hi_kExcepClass, "can not find RecFrameInfo payload field");
        return NULL;
    }

    hi_env->SetIntField(mobject, frameSizeField, iDataSize);
    hi_env->SetLongField(mobject, ptsField, pts);
    hi_env->SetIntField(mobject, payloadField, iType);
    return mobject;
}
static jobject com_media_ffmpeg_FFMpegPlayer_getRecordVideo(JNIEnv* hi_env, jobject hi_thiz, jbyteArray byteBuf)
{

    int iRet = 0;
    int64_t pts;
    int iDataSize = 0;
    int iType = 0;
    /* get the buffer address*/
    void* dst = hi_env->GetDirectBufferAddress(byteBuf);

    if (dst == NULL)
    {
        jniThrowException(hi_env, hi_kExcepClass, "could not GetDirectBufferAddress");
        return NULL;
    }

    jlong bufSize = hi_env->GetDirectBufferCapacity(byteBuf);
    iDataSize = bufSize;
    /* read video data*/
    MediaPlayer* mp = getMediaPlayer(hi_env, hi_thiz);

    if (mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return NULL;
    }

    iRet = mp->getRecordVideo(dst, iDataSize, pts, iType);

    if (iRet != NO_ERROR)
    {
        return NULL;
    }


    return getRecordFrameInfo(hi_env, hi_thiz, iDataSize, pts, iType);



}
static jobject com_media_ffmpeg_FFMpegPlayer_getRecordAudio(JNIEnv* hi_env, jobject hi_thiz, jbyteArray byteBuf)
{

    int iRet = 0;
    int64_t pts;
    int iDataSize = 0;
    int iType = 0;
    /* get buffer address*/
    void* dst = hi_env->GetDirectBufferAddress(byteBuf);

    if (dst == NULL)
    {
        jniThrowException(hi_env, hi_kExcepClass, "could not GetDirectBufferAddress");
        return NULL;
    }

    jlong bufSize = hi_env->GetDirectBufferCapacity(byteBuf);
    iDataSize = bufSize;
    /* read audio data*/
    MediaPlayer* mp = getMediaPlayer(hi_env, hi_thiz);

    if (mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return NULL;
    }

    iRet = mp->getRecordAudio(dst, iDataSize, pts, iType);

    if (iRet != NO_ERROR)
    {
        return NULL;
    }


    return getRecordFrameInfo(hi_env, hi_thiz, iDataSize, pts, iType);



}

static jobject com_media_ffmpeg_FFMpegPlayer_getSnapData(JNIEnv* hi_env, jobject hi_thiz, jbyteArray byteBuf)
{

    int iRet = 0;
    int64_t pts;
    int iWitdh = 0;
    int iHeight = 0;
    int iYpitch = 0;
    int iUpitch = 0;
    int iVpitch = 0;
    int iUoffset = 0;
    int iVoffset = 0;
    /* get buffer address */
    void* dst = hi_env->GetDirectBufferAddress(byteBuf);

    if (dst == NULL)
    {
        jniThrowException(hi_env, hi_kExcepClass, "could not GetDirectBufferAddress");
        return NULL;
    }

    jlong bufSize = hi_env->GetDirectBufferCapacity(byteBuf);

    MediaPlayer* mp = getMediaPlayer(hi_env, hi_thiz);

    if (mp == NULL )
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return NULL;
    }

    iRet = mp->getVideoWidth(&iWitdh);

    if (0 != iRet)
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return NULL;
    }

    iRet = mp->getVideoHeight(&iHeight);

    if (0 != iRet)
    {
        jniThrowException(hi_env, "java/lang/IllegalStateException", NULL);
        return NULL;
    }

    /* get snap data*/
    iRet = mp->getSnapData(dst, iYpitch, iUpitch, iVpitch, iUoffset, iVoffset, pts);

    if (iRet != NO_ERROR)
    {
        return NULL;
    }

    /*fill output frameInfo*/
    jclass clazz = hi_env->FindClass(hi_kYuvPathName);

    if (clazz == NULL)
    {
        jniThrowException(hi_env, hi_kExcepClass, "can not find hi_kYuvPathName");
        return NULL;
    }

    jobject mobject = hi_env->AllocObject(clazz);

    if (mobject == NULL)
    {
        jniThrowException(hi_env, hi_kExcepClass, "Alloc Object hi_kYuvPathName failed");
        return NULL;
    }

    jfieldID ptsField = hi_env->GetFieldID(clazz, "pts", "J");

    if (ptsField == 0)
    {
        jniThrowException(hi_env, hi_kExcepClass, "can not find YuvFrameInfo pts field");
        return NULL;
    }

    jfieldID ypitchField = hi_env->GetFieldID(clazz, "ypitch", "I");

    if (ypitchField == 0)
    {
        jniThrowException(hi_env, hi_kExcepClass, "can not find YuvFrameInfo ypitch field");
        return NULL;
    }

    jfieldID upitchField = hi_env->GetFieldID(clazz, "upitch", "I");

    if (upitchField == 0)
    {
        jniThrowException(hi_env, hi_kExcepClass, "can not find YuvFrameInfo upitch field");
        return NULL;
    }

    jfieldID vpitchField = hi_env->GetFieldID(clazz, "vpitch", "I");

    if (vpitchField == 0)
    {
        jniThrowException(hi_env, hi_kExcepClass, "can not find YuvFrameInfo vpitch field");
        return NULL;
    }

    jfieldID widthField = hi_env->GetFieldID(clazz, "width", "I");

    if (widthField == 0)
    {
        jniThrowException(hi_env, hi_kExcepClass, "can not find YuvFrameInfo width field");
        return NULL;
    }

    jfieldID heightField = hi_env->GetFieldID(clazz, "height", "I");

    if (heightField == 0)
    {
        jniThrowException(hi_env, hi_kExcepClass, "can not find YuvFrameInfo width field");
        return NULL;
    }

    jfieldID uoffsetField = hi_env->GetFieldID(clazz, "uoffset", "I");

    if (uoffsetField == 0)
    {
        jniThrowException(hi_env, hi_kExcepClass, "can not find YuvFrameInfo uoffset field");
        return NULL;
    }

    jfieldID voffsetField = hi_env->GetFieldID(clazz, "voffset", "I");

    if (voffsetField == 0)
    {
        jniThrowException(hi_env, hi_kExcepClass, "can not find YuvFrameInfo voffset field");
        return NULL;
    }

    hi_env->SetLongField(mobject, ptsField, pts);
    hi_env->SetIntField(mobject, ypitchField, iYpitch);
    hi_env->SetIntField(mobject, upitchField, iUpitch);
    hi_env->SetIntField(mobject, vpitchField, iVpitch);
    hi_env->SetIntField(mobject, widthField, iWitdh);
    hi_env->SetIntField(mobject, heightField, iHeight);
    hi_env->SetIntField(mobject, uoffsetField, iUoffset);
    hi_env->SetIntField(mobject, voffsetField, iVoffset);
    return mobject;
}


static JNINativeMethod hi_gMethods[] =
{
    {"prepare",             "()V",                              (void*)com_media_ffmpeg_FFMpegPlayer_prepare},
    {"native_init",         "()V",                              (void*)com_media_ffmpeg_FFMpegPlayer_native_init},
    {"_setDataSource",      "(Ljava/lang/String;)V",            (void*)com_media_ffmpeg_FFMpegPlayer_setDataSource},
    {"setVideoMbufLimit",   "(II)V",                            (void*)com_media_ffmpeg_FFMpegPlayer_setVideoMbufLimit},
    {"getCurrentPosition",  "()I",                              (void*)com_media_ffmpeg_FFMpegPlayer_getCurrentPosition},
    {"setSaveDataFlag",     "(I)V",                             (void*)com_media_ffmpeg_FFMpegPlayer_setSaveDataFlag},
    {"_setVideoSurface",    "(Landroid/view/Surface;I)V",       (void*)com_media_ffmpeg_FFMpegPlayer_setVideoSurface},
    {"_start",              "()V",                              (void*)com_media_ffmpeg_FFMpegPlayer_start},
    {"_release",            "()V",                              (void*)com_media_ffmpeg_FFMpegPlayer_release},
    {"getVideoWidth",       "()I",                              (void*)com_media_ffmpeg_FFMpegPlayer_getVideoWidth},
    {"native_setup",        "(Ljava/lang/Object;)V",            (void*)com_media_ffmpeg_FFMpegPlayer_native_setup},
    {"_reset",              "()V",                              (void*)com_media_ffmpeg_FFMpegPlayer_reset},
    {"getVideoHeight",      "()I",                              (void*)com_media_ffmpeg_FFMpegPlayer_getVideoHeight},
    {"seekTo",              "(I)V",                             (void*)com_media_ffmpeg_FFMpegPlayer_seekTo},
    {"_pause",              "()V",                              (void*)com_media_ffmpeg_FFMpegPlayer_pause},
    {"isPlaying",           "()Z",                              (void*)com_media_ffmpeg_FFMpegPlayer_isPlaying},
    {"setLivePlayMode",     "(I)V",                             (void*)com_media_ffmpeg_FFMpegPlayer_setLivePlayMode},
    {"_stop",               "()V",                              (void*)com_media_ffmpeg_FFMpegPlayer_stop},
    {"getDuration",         "()I",                              (void*)com_media_ffmpeg_FFMpegPlayer_getDuration},
    {"_invoke",             "(III)I",                           (void*)com_media_ffmpeg_FFMpegPlayer_invoke},
    {"setMaxResolution",    "(II)V",                            (void*)com_media_ffmpeg_FFMpegPlayer_setMaxResolution},
    {"_setRecordFlag",      "(I)V",                             (void*)com_media_ffmpeg_FFMpegPlayer_setRecordFlag},
    {"_getRecordVideo",     "(Ljava/nio/ByteBuffer;)Lcom/hisilicon/camplayer/HiCamPlayer$RecFrameInfo;",         (void*)com_media_ffmpeg_FFMpegPlayer_getRecordVideo},
    {"_getRecordAudio",     "(Ljava/nio/ByteBuffer;)Lcom/hisilicon/camplayer/HiCamPlayer$RecFrameInfo;",         (void*)com_media_ffmpeg_FFMpegPlayer_getRecordAudio},
    {"_getSnapData",      "(Ljava/nio/ByteBuffer;)Lcom/hisilicon/camplayer/HiCamPlayer$YuvFrameInfo;",    (void*)com_media_ffmpeg_FFMpegPlayer_getSnapData}

};

int register_android_media_FFMpegPlayerAndroid(JNIEnv* hi_env)
{
    hi_jniEnv = hi_env;
    return jniRegisterNativeMethods(hi_env, hi_kClassPathName, hi_gMethods, sizeof(hi_gMethods) / sizeof(hi_gMethods[0]));
}
