#include <sys/types.h>
//#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/log.h"
#include "../arm_neon/yuv2rgb_neon.h"
#include "Hies_client.h"
#include "Hies_proto.h"

} // end of extern C
#include <android/log.h>
#include "mediaplayer.h"
//#include "ahplayerdef.h"
#include "yuv2rgb_neon.h"
#include "HiMLoger.h"
#include "HiffmpegHandlr.h"
#include "HiH265Handlr.h"
#include "mediaPlayerListener.h"
#include "HiRtspClient.h"
#include "HiFileClient.h"
#include "HiAirPlayClient.h"


#define TAG "HiMediaPlayer"
MediaPlayer::MediaPlayer()
{
    pthread_mutex_init(&mLock, NULL);
    pthread_mutex_init(&mListenerLock, NULL);
    mListener = NULL;
    mPCMWriter = NULL;
    mCurrentState = MEDIA_PLAYER_IDLE;
    mAPIVersion = -1;
    mClient = NULL;
    mMediaHandlr = NULL;
    mMaxWidth = 0;
    mMaxHeight = 0;
    mSaveVideoFlag = 0;
    mSetVideoResFlag = 0;
    mCacheSource = NULL;
    mbStatChecking = false;
    mbInBuffering = 0;
    mbEOS = 0;
    mStatThrd = 0;
    mjsurface = NULL;
    mAudioLatency = 0;
    mjenv = NULL;
    mDuration = 0;
    MMLOGI(TAG, "EATAPLAYER version: 0.2.3\n");
}


MediaPlayer::~MediaPlayer()
{
    pthread_mutex_destroy(&mListenerLock);
    pthread_mutex_destroy(&mLock);
    mjsurface = NULL;
    mPCMWriter = NULL;
    mjenv = NULL;
    mClient = NULL;
    mMediaHandlr = NULL;
    mListener = NULL;
    mCacheSource = NULL;

}

/*配置音视频播放的一些参数*/
/* config some parameters of audio and video*/
status_t MediaPlayer::prepare()
{
    status_t ret = 0;
    int apiVer = -1;
    int width = 0, height = 0;
    eProtoVidType type = Protocol_Video_BUTT;

    MMLOGI(TAG, "prepare");
    pthread_mutex_lock(&mLock);

    if (MEDIA_PLAYER_PREPARED == mCurrentState)
    {
        MMLOGI(TAG,  " already prepared:%d", mCurrentState);
        return NO_ERROR;
    }

    if (mCurrentState != MEDIA_PLAYER_INITIALIZED)
    {
        MMLOGE(TAG,  "prepare failed, state: %d is not right", mCurrentState);
        goto Failed;
    }

    apiVer = getAPIVersion();

    if (apiVer == -1)
    {
        MMLOGE(TAG, "API version not initlized \n");
        goto Failed;
    }

    if (mjsurface == NULL)
    {
        MMLOGE(TAG, "native surface have not been set \n");
        goto Failed;
    }

    if (!mClient)
    {
        MMLOGE(TAG, "mClient is null ");
        goto Failed;
    }

    ret = mClient->play();
    if (ret != NO_ERROR)
    {
        MMLOGI(TAG, "protocol play failed\n");
        goto Failed;
    }
    ret = mClient->getVideoType(type);

    if (ret < 0)
    {
        MMLOGE(TAG, "getVideoType failed(%d)", ret);
        goto Failed;
    }

    if (type == Protocol_Video_H264)
    {
        mMediaHandlr = new HiffmpegHandlr(mjsurface, mjenv, mPCMWriter, mAPIVersion, mClient);
    }
    else if (type == Protocol_Video_H265)
    {
        mMediaHandlr = new HiH265Handlr(mjsurface, mjenv, mPCMWriter, mAPIVersion, mClient);

        if (mSetVideoResFlag)
        { (static_cast<HiH265Handlr*>(mMediaHandlr))->setMaxDecodeResolution(mMaxWidth, mMaxHeight); }
    }

    if (!mMediaHandlr)
    {
        MMLOGE(TAG, "mMediaHandlr is null  ");
        goto Failed;
    }

    if (!mListener)
    {
        MMLOGE(TAG, "mListener have not been set ");
        goto DeHandlr;
    }

    mMediaHandlr->setListener(this);
    ret = mMediaHandlr->open();

    if (ret < 0)
    {
        MMLOGE(TAG, "openDecoder failed(%d)", ret);
        goto DeHandlr;
    }

    mMediaHandlr->getWidth(&width);
    mMediaHandlr->getHeight(&height);
    mClient->getDuration(mDuration);
    MMLOGE(TAG, " video width: %d height: %d duration: %d\n", width, height, mDuration);
    mListener->notify(MEDIA_PLAYBACK_INFO, MEDIA_PLAYBACK_PREPARED, -1);
    mCurrentState = MEDIA_PLAYER_PREPARED;
    MMLOGI(TAG, "prepare OK");
    pthread_mutex_unlock(&mLock);
    return NO_ERROR;

DeHandlr:
    delete mMediaHandlr;
    mMediaHandlr = NULL;
ProtoStop:
    mClient->stop();
Failed:
    mCurrentState = MEDIA_PLAYER_ERROR;
    pthread_mutex_unlock(&mLock);
    return INVALID_OPERATION;
}

/*设定player事件监听回调对象*/
/*set listener object for player event*/
status_t MediaPlayer::setListener(MediaPlayerListener* listener)
{
    MMLOGI(TAG, "setListener: %08x", listener);
    mListener = listener;
    return NO_ERROR;
}

status_t  MediaPlayer::setPCMWriter(MediaPlayerPCMWriter* writer)
{
    MMLOGI(TAG, "setPCMWriter: %08x", writer);
    mPCMWriter = writer;
    return NO_ERROR;
}

int MediaPlayer::getAPIVersion() const
{
    return mAPIVersion;
}
void MediaPlayer::setAPIVersion(int version)
{
    mAPIVersion = version;
}

void MediaPlayer::setVideoMbufLimit(unsigned int dropLimit, unsigned int clearLimit)
{
    pthread_mutex_lock(&mLock);

    if (!mClient || mCurrentState >= MEDIA_PLAYER_PREPARED)
    {
        MMLOGE(TAG, "setVideoMbufLimit, status is not right :%d", mCurrentState);
        pthread_mutex_unlock(&mLock);
        return;
    }

    mClient->setVideoMbufLimit(dropLimit, clearLimit);
    pthread_mutex_unlock(&mLock);
}

void MediaPlayer::setVideoMaxResolution(int width, int height)
{
    MMLOGI(TAG, "MediaPlayer setVideoMaxResolution width: %d height: %d\n", width, height);
    mSetVideoResFlag = 1;
    mMaxWidth = width;
    mMaxHeight = height;
}

void MediaPlayer::setSaveRemoteDataFlag(int flag)
{
    mSaveVideoFlag = flag;
}

/*设置服务器的URL*/
/* set the server url*/
status_t MediaPlayer::setDataSource(const char* url)
{
    int err = 0;
    pthread_mutex_lock(&mLock);
    MMLOGI(TAG, "setDataSource(%s)", url);

    if (mCurrentState != MEDIA_PLAYER_IDLE)
    {
        MMLOGE(TAG, "setDataSource, status is not right :%d", mCurrentState);
        goto Failed;
    }

    if (strlen(url) == 0)
    {
        MMLOGE(TAG, "Url is empty\n");
        goto Failed;
    }

    /*connect */
    if (strcasestr(url, "rtsp://"))
    {
        mClient = new HiRtspClient(url);

        if (mSaveVideoFlag)
        { (static_cast<HiRtspClient*>(mClient))->setVideoSaveFlag(mSaveVideoFlag); }
    }
    else if (strcasestr(url, ".mp4") || strcasestr(url, ".LRV"))
    {
        if (strcasestr(url, "http://"))
        {
            mCacheSource = new HiCacheSource(url);
            mCacheSource->setListener(this);
            err = mCacheSource->open();

            if (err != 0)
            {
                MMLOGI(TAG, "mCacheSource open failed\n");
                delete mCacheSource;
                goto Failed;
            }

            mClient = new HiAirPlayClient(url, mCacheSource);
        }
        else
        { mClient = new HiFileClient(url); }
    }
    else
    {
        MMLOGE(TAG, "input url could not been supported\n");
        goto Failed;
    }

    if (mListener)
    { mClient->setListener(this); }
    else
    {
        MMLOGI(TAG, "mListener is NULL\n");
        goto DeClient;
    }

    err = mClient->connect();

    if (err != NO_ERROR)
    {
        MMLOGI(TAG, "protocol connect failed\n");
        goto DeClient;
    }

    MMLOGI(TAG, "setdataSource OK");
    mCurrentState = MEDIA_PLAYER_INITIALIZED;
    pthread_mutex_unlock(&mLock);
    return NO_ERROR;

DeClient:

    if (mCacheSource)
    {
        mCacheSource->close();
        delete mCacheSource;
    }

    mCacheSource = NULL;

    if (mClient)
    { delete mClient; }

    mClient = NULL;
Failed:
    mCurrentState = MEDIA_PLAYER_ERROR;
    pthread_mutex_unlock(&mLock);
    return INVALID_OPERATION;
}

void* MediaPlayer::startStatusLooper(void* ptr)
{
    MediaPlayer* pHandlr = static_cast<MediaPlayer*>(ptr);

    if (pHandlr->StatusLooper(ptr) == NULL)
    { MMLOGI(TAG, "StatusLooper OK"); }

    return NULL;
}

void* MediaPlayer::StatusLooper(const void* args)
{
    int height = 0;
    int width = 0;
    int ret = 0;
    HI_U32 LowWaterLevel = 0;
    HI_U32 HighWaterLevel = 0;

    ret = getVideoHeight(&height);

    if (ret == INVALID_OPERATION)
    {
        MMLOGI(TAG, "getVideoHeight FAILED ret: %d ", ret);
    }

    ret = getVideoWidth(&width);

    if (ret == INVALID_OPERATION)
    {
        MMLOGI(TAG, "getVideoHeight FAILED ret: %d ", ret);
    }

    MMLOGI(TAG, "status video height: %d width: %d ", height, width);

    if (width >= 1920 && height >= 1080)
    {
        LowWaterLevel = HI_CACHE_LOW_WAETER_LEVEL;
        HighWaterLevel = HI_CACHE_HIGH_WAETER_LEVEL;
    }
    else
    {
        LowWaterLevel = HI_CACHE_TINYLOW_WAETER_LEVEL;
        HighWaterLevel = HI_CACHE_TINYHIGH_WAETER_LEVEL;
    }

    MMLOGI(TAG, "cache water level low: %d high: %d ", LowWaterLevel, HighWaterLevel);

    while (mbStatChecking)
    {
        if (mCacheSource->isReachEOF())
        {
            /*already cache to endof the file,just empty loop util seek to uncached point*/
            if (mbInBuffering)
            {
                mbInBuffering = 0;

                if (mCurrentState == MEDIA_PLAYER_PAUSED)
                {
                    ret = start();

                    if (ret == INVALID_OPERATION)
                    {
                        MMLOGI(TAG, "start FAILED ret: %d ", ret);
                    }
                }

                notify(MEDIA_PLAYBACK_INFO, MEDIA_PLAYBACK_BUFFERING_END, -1);
            }

            HI_U32 curCacheSize = mCacheSource->getCacheSize();

            if (!mbEOS && curCacheSize == 0)
            {
                notify(MEDIA_REACH_EOF, -1, -1);

                if (mCurrentState == MEDIA_PLAYER_RUNNING)
                {
                    ret = pause();

                    if (ret == INVALID_OPERATION)
                    {
                        MMLOGI(TAG, "pause FAILED ret: %d ", ret);
                    }
                }

                mbEOS = 1;
                MMLOGI(TAG, "cache Source end of stream");
            }

            usleep(100000);
            continue;
        }
        else if (mbEOS)
        { mbEOS = 0; }

        HI_U32 curCacheSize = mCacheSource->getCacheSize();
        MMLOGI(TAG, "curCacheSize : %d", curCacheSize);

        if (mCurrentState == MEDIA_PLAYER_RUNNING
            && curCacheSize < LowWaterLevel)
        {
            ret = pause();

            if (ret == INVALID_OPERATION)
            {
                MMLOGI(TAG, "pause FAILED ret: %d ", ret);
            }

            notify(MEDIA_PLAYBACK_INFO, MEDIA_PLAYBACK_BUFFERING_START, -1);
            mbInBuffering = 1;
            mCacheSource->ensureCacheFetching();
        }
        else if (mbInBuffering
                 && curCacheSize > HighWaterLevel)
        {
            mbInBuffering = 0;

            if (mCurrentState == MEDIA_PLAYER_PAUSED)
            {
                ret = start();

                if (ret == INVALID_OPERATION)
                {
                    MMLOGI(TAG, "start FAILED ret: %d ", ret);
                }
            }

            notify(MEDIA_PLAYBACK_INFO, MEDIA_PLAYBACK_BUFFERING_END, -1);
        }

        if (mbInBuffering && mCurrentState == MEDIA_PLAYER_PAUSED)
        {
            int loadingPercent  = (int) ((curCacheSize * 100) / HighWaterLevel);
            notify(MEDIA_PLAYBACK_INFO, MEDIA_PLAYBACK_LOADING_PERCENT, loadingPercent);
        }

        int64_t lastCacheTimeUs = mCacheSource->getLastCachedTimeUs();
        MMLOGI(TAG, "lastCacheTimePts : %lld duration: %d", lastCacheTimeUs, mDuration);

        if (mDuration > 0)
        {
            int cachePercent = (int)((lastCacheTimeUs * 100) / mDuration);
            notify(MEDIA_BUFFERING_UPDATE, cachePercent, -1);
        }
        else
        {
            ret = getDuration(&mDuration);

            if (ret == INVALID_OPERATION)
            {
                MMLOGI(TAG, "getDuration FAILED ret: %d ", ret);
            }
        }

        sleep(1);
    }

    notify(DETACHING, -1, -1);
    MMLOGI(TAG, "StatusLooper exited");
    return NULL;
}

/*从java层传递surface 对象到Cpp层，注册该surface, 以显示视频*/
/*deliver from java to cpp through jni, register this surface for video display*/
status_t MediaPlayer::setVideoSurface(JNIEnv* env, jobject jsurface, int apiVersion)
{
    /*注册surface 到Native Window, 并返回window实例*/
    /*register the surface to Native Window, return instance of window*/
    setAPIVersion(apiVersion);
    mjsurface = jsurface;
    mjenv = env;
    return NO_ERROR;
}

/*启动音视频播放*/
/*start the audio and video play */
status_t MediaPlayer::start()
{
    int ret = 0;
    MMLOGI(TAG, "start");

    if (!mMediaHandlr)
    {
        MMLOGI(TAG, "mMediaHandlr is null");
        return INVALID_OPERATION;
    }

    if (!mListener)
    {
        MMLOGI(TAG, "mListener is null");
        return INVALID_OPERATION;
    }

    pthread_mutex_lock(&mLock);

    if (mCurrentState == MEDIA_PLAYER_RUNNING)
    {
        MMLOGI(TAG, "player is already in running");
        pthread_mutex_unlock(&mLock);
        return NO_ERROR;
    }

    if (mCurrentState != MEDIA_PLAYER_PREPARED
        && mCurrentState != MEDIA_PLAYER_PAUSED
        && mCurrentState != MEDIA_PLAYER_STOPPED)
    {
        MMLOGI(TAG, "player start ,cur state uncorrect: %d", mCurrentState);
        pthread_mutex_unlock(&mLock);
        return INVALID_OPERATION;
    }

    if (mbEOS && mCurrentState == MEDIA_PLAYER_PAUSED)
    {
        mMediaHandlr->seekTo(40, (bool)mbEOS);
        mbEOS = 0;
    }

    ret = mMediaHandlr->start();

    if (ret != 0)
    {
        MMLOGI(TAG, "mMediaHandlr start failed");
        pthread_mutex_unlock(&mLock);
        return INVALID_OPERATION;
    }

    mListener->notify(MEDIA_PLAYBACK_INFO, MEDIA_PLAYBACK_PLAYED, -1);
    MMLOGI(TAG, "MediaPlayer play");
    mbInBuffering = 0;

    if (mCurrentState == MEDIA_PLAYER_PREPARED && mCacheSource)
    {
        mbStatChecking = true;
        pthread_create(&mStatThrd, NULL, startStatusLooper, this);
        MMLOGI(TAG, "startStatusLooper thread");
    }

    mCurrentState = MEDIA_PLAYER_RUNNING;
    pthread_mutex_unlock(&mLock);
    return NO_ERROR;
}

status_t MediaPlayer::stop()
{
    MMLOGI(TAG, "stop");

    if (!mMediaHandlr)
    {
        MMLOGI(TAG, "mMediaHandlr is null");
        return INVALID_OPERATION;
    }

    if (!mListener)
    {
        MMLOGI(TAG, "mListener is null");
        return INVALID_OPERATION;
    }

    //just pause
    pthread_mutex_lock(&mLock);

    if (mCurrentState == MEDIA_PLAYER_STOPPED)
    {
        MMLOGI(TAG, "already player stoped\n");
        pthread_mutex_unlock(&mLock);
        return NO_ERROR;
    }

    if (mCurrentState != MEDIA_PLAYER_RUNNING)
    {
        MMLOGI(TAG, "cur state: %d is not running, just return\n", mCurrentState);
        pthread_mutex_unlock(&mLock);
        return NO_ERROR;
    }

    mMediaHandlr->pause();
    mListener->notify(MEDIA_PLAYBACK_INFO, MEDIA_PLAYBACK_STOPPED, -1);
    mCurrentState = MEDIA_PLAYER_STOPPED;
    pthread_mutex_unlock(&mLock);
    return NO_ERROR;
}

bool MediaPlayer::isPlaying() const
{
    return mCurrentState == MEDIA_PLAYER_RUNNING;
}

/*获取视频宽度*/
/*get video width*/
status_t MediaPlayer::getVideoWidth(int* w)
{
    if (mCurrentState < MEDIA_PLAYER_PREPARED || mCurrentState == MEDIA_PLAYER_ERROR)
    {
        MMLOGI(TAG, "getVideoWidth, status is not right :%d", mCurrentState);
        return INVALID_OPERATION;
    }

    if (mMediaHandlr)
    {
        mMediaHandlr->getWidth(w);
    }
    else
    {
        *w = 0;
    }

    return NO_ERROR;
}
/*获取视频高度*/
/*get video height*/
status_t MediaPlayer::getVideoHeight(int* h)
{
    if (mCurrentState < MEDIA_PLAYER_PREPARED || mCurrentState == MEDIA_PLAYER_ERROR)
    {
        MMLOGI(TAG, "getVideoHeight, status is not right :%d", mCurrentState);
        return INVALID_OPERATION;
    }

    if (mMediaHandlr)
    { mMediaHandlr->getHeight(h); }
    else
    { *h = 0; }

    //*h = mVideoHeight;
    return NO_ERROR;
}

status_t MediaPlayer::reset()
{
    MMLOGI(TAG, "reset");

    if (mCacheSource)
    {
        MMLOGI(TAG, "stop status checking thread");
        mbStatChecking = false;

        //do not lock this line to avoid dead lock
        if (mCurrentState >= MEDIA_PLAYER_RUNNING && mStatThrd)
        {
            if (pthread_join(mStatThrd, 0) < 0)
            {
                MMLOGI(TAG, "pthread_join mStatThrd failed");
            }

            mStatThrd = 0;
        }
    }

    pthread_mutex_lock(&mLock);

    if (mCurrentState == MEDIA_PLAYER_IDLE)
    {
        MMLOGI(TAG, "player already reseted\n");
        pthread_mutex_unlock(&mLock);
        return NO_ERROR;
    }

    if (mClient)
    {
        mClient->disconnect();
    }

    if (mMediaHandlr)
    {
        MMLOGI(TAG, "mMediaHandlr destroy begin");
        mMediaHandlr->stop();
        mMediaHandlr->close();
        delete mMediaHandlr;
        mMediaHandlr = NULL;
    }

    if (mClient)
    {
        if (mCacheSource)
        {
            mCacheSource->close();
            delete mCacheSource;
            mCacheSource = NULL;
        }

        delete mClient;
        mClient = NULL;
    }

    mClient = NULL;
    mMediaHandlr = NULL;
    mMaxWidth = 0;
    mMaxHeight = 0;
    mSaveVideoFlag = 0;
    mSetVideoResFlag = 0;
    mCacheSource = NULL;
    mbStatChecking = false;
    mbInBuffering = 0;
    mbEOS = 0;
    mStatThrd = 0;
    mCurrentState = MEDIA_PLAYER_IDLE;
    pthread_mutex_unlock(&mLock);
    MMLOGI(TAG, "reseted");
    return INVALID_OPERATION;
}

/*到java的事件通知借口*/
/*the event notify interface to java*/
void MediaPlayer::notify(int msg, int ext1, int ext2)
{
    MMLOGI(TAG, "message received msg=%d, ext1=%d, ext2=%d", msg, ext1, ext2);

    //bool send = true;
    if (!mMediaHandlr)
    {
        MMLOGI(TAG, "mMediaHandlr is null");
        return ;
    }

    if (!mListener)
    {
        MMLOGI(TAG, "mListener is null");
        return ;
    }

    if (!mClient)
    {
        MMLOGI(TAG, "mClient is null");
        return ;
    }

    pthread_mutex_lock(&mListenerLock);

    switch (msg)
    {
        case MEDIA_ERROR:
            mCurrentState = MEDIA_PLAYER_ERROR;
            break;

        case MEDIA_REACH_EOF:
            mbEOS = 1;

            if (mClient->getProtoType() == PROTO_TYPE_FILE_CLIENT)
            {
                mMediaHandlr->pause();
                mCurrentState = MEDIA_PLAYER_PAUSED;
                mListener->notify(MEDIA_PLAYBACK_INFO, MEDIA_PLAYBACK_PAUSED, -1);
                MMLOGI(TAG, "MediaPlayer pause");
            }

            break;

        default:
            break;
    }

    //  if ((mListener != 0) && send)
    mListener->notify(msg, ext1, ext2);
    pthread_mutex_unlock(&mListenerLock);
}

status_t MediaPlayer::pause()
{
    if (!mMediaHandlr)
    {
        MMLOGI(TAG, "mMediaHandlr have not been prepared");
        return INVALID_OPERATION;
    }

    if (!mListener)
    {
        MMLOGI(TAG, "mListener have not been prepared");
        return INVALID_OPERATION;
    }

    pthread_mutex_lock(&mLock);

    if (mCurrentState == MEDIA_PLAYER_PAUSED)
    {
        MMLOGI(TAG, "player already paused");
        pthread_mutex_unlock(&mLock);
        return NO_ERROR;
    }

    if (mCurrentState == MEDIA_PLAYER_RUNNING)
    {
        mMediaHandlr->pause();
        mCurrentState = MEDIA_PLAYER_PAUSED;
        mListener->notify(MEDIA_PLAYBACK_INFO, MEDIA_PLAYBACK_PAUSED, -1);
        MMLOGI(TAG, "MediaPlayer pause");
    }
    else
    {
        MMLOGI(TAG, "current state: %d not support pause", mCurrentState);
        pthread_mutex_unlock(&mLock);
        return INVALID_OPERATION;
    }

    pthread_mutex_unlock(&mLock);
    return NO_ERROR;
}

status_t MediaPlayer::getDuration(int* msec)
{
    int duration = 0;
    pthread_mutex_lock(&mLock);

    if (mCurrentState != MEDIA_PLAYER_PREPARED
        && mCurrentState != MEDIA_PLAYER_PAUSED
        && mCurrentState != MEDIA_PLAYER_RUNNING)
    {
        MMLOGI(TAG, "current state: %d not support getDuration", mCurrentState);
        *msec = 0;
        pthread_mutex_unlock(&mLock);
        return INVALID_OPERATION;
    }

    if (mClient)
    { mClient->getDuration(duration); }

    *msec = duration;
    pthread_mutex_unlock(&mLock);
    MMLOGI(TAG, "getDuration: %d", duration);
    return NO_ERROR;
}

status_t MediaPlayer::getCurrentPosition(int* msec)
{
    int position = 0;
    pthread_mutex_lock(&mLock);

    if (mCurrentState != MEDIA_PLAYER_PREPARED
        && mCurrentState != MEDIA_PLAYER_PAUSED
        && mCurrentState != MEDIA_PLAYER_RUNNING)
    {
        MMLOGE(TAG, "current state: %d not support getCurrentPosition", mCurrentState);
        *msec = 0;
        pthread_mutex_unlock(&mLock);
        return INVALID_OPERATION;
    }

    if (mMediaHandlr)
    { mMediaHandlr->getCurPostion(position); }

    *msec = position;
    pthread_mutex_unlock(&mLock);
    MMLOGI(TAG, "getCurrentPosition: %d", position);
    return NO_ERROR;
}

status_t MediaPlayer::seekTo(int msec)
{
    MMLOGI(TAG, "seekTo: %d eos: %d", msec, mbEOS);
    int ret = 0;
    pthread_mutex_lock(&mLock);

    if (mCurrentState != MEDIA_PLAYER_RUNNING
        && mCurrentState != MEDIA_PLAYER_PREPARED
        && mCurrentState != MEDIA_PLAYER_PAUSED
        && mCurrentState != MEDIA_PLAYER_PLAYBACK_COMPLETE)
    {
        MMLOGI(TAG, "current state: %d not support seekTo", mCurrentState);
        pthread_mutex_unlock(&mLock);
        return INVALID_OPERATION;
    }

    if (msec == 0)
    { msec = 40; }

    if (mMediaHandlr)
    { mMediaHandlr->seekTo(msec, (bool)mbEOS); }

    pthread_mutex_unlock(&mLock);

    if (mbEOS && mCurrentState == MEDIA_PLAYER_PAUSED)
    {
        mbEOS = 0;
        ret = start();

        if (ret == INVALID_OPERATION)
        {
            MMLOGI(TAG, "start FAILED ret:%d", ret);
            return INVALID_OPERATION;
        }
    }

    return NO_ERROR;
}

//mode: 0/low delay; 1/fluent
void MediaPlayer::setLivePlayMode(int mode)
{
    MMLOGI(TAG, "setLivePlayMode: %d", mode);

}

int MediaPlayer::invoke(int msgId, int what, int extra) const
{
    return NO_ERROR;
}

/*设置录像状态*/


void MediaPlayer::setRecordFlag(int flag)
{
    MMLOGI(TAG, "setRecordFlag: %d", flag);


    if (mClient && mClient->getProtoType() == PROTO_TYPE_RTSP_CLIENT)
    { static_cast<HiRtspClient*>(mClient)->setRecordFlag(flag); }
}

/*启动录像模式*/
status_t MediaPlayer::getRecordVideo(void* ptr, int& dataSize, int64_t& pts, int& ptype)
{
    int iRet = -1;
    unsigned int uBufSize = dataSize;

    if (mClient && mClient->getProtoType() == PROTO_TYPE_RTSP_CLIENT)
    {
        iRet = static_cast<HiRtspClient*>(mClient)->getRecordVideo(ptr, uBufSize, pts, ptype);

        if (iRet != 0)
        {
            return iRet;
        }

        dataSize = uBufSize;
    }
    else
    {
        MMLOGE(TAG, "not support get record video");
        return iRet;
    }

    return NO_ERROR;
}


/*启动录像模式*/
status_t MediaPlayer::getRecordAudio(void* ptr, int& dataSize, int64_t& pts, int& ptype)
{
    int iRet = -1;
    unsigned int uBufSize = dataSize;

    if (mClient && mClient->getProtoType() == PROTO_TYPE_RTSP_CLIENT)
    {
        iRet = static_cast<HiRtspClient*>(mClient)->getRecordAudio(ptr, uBufSize, pts, ptype);

        if (iRet != 0)
        {
            return iRet;
        }

        dataSize = uBufSize;
    }
    else
    {
        MMLOGE(TAG, "not support get record audio");
        return iRet;
    }

    return NO_ERROR;
}
status_t MediaPlayer::getSnapData(void* ptr, int& ypitch, int& upitch, int& vpitch, int& uoffset, int& voffset,  int64_t&  pts)
{
    int iRet = -1;


    if (mMediaHandlr && mClient->getProtoType() == PROTO_TYPE_RTSP_CLIENT)
    {
        iRet = mMediaHandlr->getSnapData(ptr, &ypitch, &upitch, &vpitch, &uoffset, &voffset, &pts);

        if (0 != iRet)
        {
            return iRet;
        }

    }
    else
    {
        MMLOGE(TAG, "not support get snap data");
        return iRet;
    }


    return NO_ERROR;
}

status_t MediaPlayer::getTrackInfo(ProtoMediaInfo& mediaInfo)
{
    return mClient->getMediaInfo(mediaInfo);
}

status_t MediaPlayer::selectTrack(HI_U8 u8Index)
{
    return mClient->selectStreamIndex(u8Index);
}

