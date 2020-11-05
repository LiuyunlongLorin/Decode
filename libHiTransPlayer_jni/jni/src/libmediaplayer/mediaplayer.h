#ifndef FFMPEG_MEDIAPLAYER_H
#define FFMPEG_MEDIAPLAYER_H
/*
 * Copyright (C) 2011 xuyangpo/x54178
*/
#include <pthread.h>

#include <jni.h>

#include "mediaPlayerListener.h"
#include "HiProtocol.h"
#include "HiMediaHandlr.h"
#include "HiCacheSource.h"


//using namespace android;

typedef HI_S32 status_t;

#define NO_ERROR (0)
#define INVALID_OPERATION (-1)

enum media_info_type
{

    MEDIA_INFO_UNKNOWN = 1,
    MEDIA_INFO_VIDEO_TRACK_LAGGING = 700,
    MEDIA_INFO_BAD_INTERLEAVING = 800,
    MEDIA_INFO_NOT_SEEKABLE = 801,
    MEDIA_INFO_METADATA_UPDATE = 802,
    MEDIA_INFO_FRAMERATE_VIDEO = 900,
    MEDIA_INFO_FRAMERATE_AUDIO
};



enum media_player_states
{
    MEDIA_PLAYER_IDLE               = 1 << 0,
    MEDIA_PLAYER_INITIALIZED        = 1 << 1,
    MEDIA_PLAYER_PREPARED           = 1 << 2,
    MEDIA_PLAYER_RUNNING            = 1 << 3,
    MEDIA_PLAYER_PAUSED             = 1 << 4,
    MEDIA_PLAYER_ERROR             = 1 << 5,
    MEDIA_PLAYER_STOPPED            = 1 << 6,
    MEDIA_PLAYER_PLAYBACK_COMPLETE  = 1 << 7
};

class MediaPlayer : public MediaPlayerListener
{
public:
    MediaPlayer();
    ~MediaPlayer();
    status_t        setDataSource(const char* url);
    status_t        setVideoSurface(JNIEnv* env, jobject jsurface, int apiVersion);
    status_t        setListener(MediaPlayerListener* listener);
    status_t        setPCMWriter(MediaPlayerPCMWriter* writer);
    status_t        start();
    status_t        stop();
    status_t        pause();
    bool              isPlaying() const;
    status_t        getVideoWidth(int* w);
    status_t        getVideoHeight(int* h);
    status_t        seekTo(int msec);
    status_t        getCurrentPosition(int* msec);
    status_t        getDuration(int* msec);
    status_t        reset();
    status_t        prepare();
    int                 invoke(int msgId, int what, int extra) const;
    void              notify(int msg, int ext1, int ext2);
    int                getAPIVersion() const ;
    void              setAPIVersion(int version);
    void              setVideoMbufLimit(unsigned int dropLimit, unsigned int clearLimit);
    void              setVideoMaxResolution(int width, int height);
    //just for test
    void              setSaveRemoteDataFlag(int flag);
    static void*  startStatusLooper(void* ptr);
    void*            StatusLooper(const void* args);
    void              setLivePlayMode(int mode);
    void               setRecordFlag(int flag);
    status_t      getRecordVideo(void* ptr, int& dataSize, int64_t& pts, int& ptype);
    status_t      getRecordAudio(void* ptr, int& dataSize, int64_t& pts, int& ptype);
    status_t      getSnapData(void* ptr, int& ypitch, int& upitch, int& vpitch, int& uoffset, int& voffset,  int64_t&  pts);
    status_t getTrackInfo(ProtoMediaInfo& mediaInfo);
    status_t selectTrack(HI_U8 u8Index);

private:
    pthread_mutex_t             mLock;
    pthread_mutex_t             mListenerLock;
    MediaPlayerPCMWriter*        mPCMWriter;
    media_player_states         mCurrentState;
    int mAudioLatency;
    int mAPIVersion;
    jobject mjsurface;
    JNIEnv* mjenv;
    HiProtocol* mClient;
    HiMediaHandlr* mMediaHandlr;
    MediaPlayerListener*     mListener;
    int mMaxWidth;
    int mMaxHeight;
    int mSaveVideoFlag;
    int mSetVideoResFlag;
    int mDuration;
    bool mbStatChecking;
    pthread_t mStatThrd;
    int mbInBuffering;
    int mbEOS;
    HiCacheSource* mCacheSource;
};

#endif // FFMPEG_MEDIAPLAYER_H
