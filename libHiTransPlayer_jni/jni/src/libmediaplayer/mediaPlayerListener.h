#ifndef _CAM_MEDIA_PLAYER_LISTENER_
#define _CAM_MEDIA_PLAYER_LISTENER_
// ----------------------------------------------------------------------------
// ref-counted object for callbacks

#define MEDIA_PLAYBACK_INFO 0x2
#define MEDIA_PLAYBACK_PREPARED 0x1
#define MEDIA_PLAYBACK_PAUSED (7)
#define MEDIA_PLAYBACK_PLAYED (6)
#define MEDIA_PLAYBACK_STOPPED (8)


#define MEDIA_PLAYBACK_BUFFERING_START  (701)
// MediaPlayer is resuming playback after filling buffers.
#define MEDIA_PLAYBACK_BUFFERING_END  (702)

#define MEDIA_PLAYBACK_LOADING_PERCENT  (711)

#define MEDIA_BUFFERING_UPDATE  (3)
#define MEDIA_ERROR 100
#define MEDIA_ASR_CHANGE 300
#define MEDIA_REACH_EOF 400
#define DETACHING 0x0

enum media_error_type {
    // 0xx
    MEDIA_ERROR_UNKNOWN = 1,
    // 1xx
    MEDIA_ERROR_SERVER_DIED = 100,
    // 2xx
    MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK = 200
    // 3xx
};

class MediaPlayerListener
{
public:
    virtual void notify(int msg, int ext1, int ext2) = 0;
};

class MediaPlayerPCMWriter
{
public:
    virtual void writePCM(unsigned char* buf,int bufCount) = 0;
    virtual int configAudioTrack(int streamType, int sampleRate, int channelConfig,int bytesPerSample,int trackMode ) = 0;
    virtual void flush() = 0;
};

#endif
