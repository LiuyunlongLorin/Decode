#ifndef _HI_MEIDIA_HANDLR_H_
#define _HI_MEIDIA_HANDLR_H_
#include <jni.h>
#include "mediaPlayerListener.h"
#include "HiMLoger.h"
#include "HiProtocol.h"


//using namespace android;

#define STREAM_MUSIC 3
#define CHANNEL_CONFIGURATION_MONO 2
#define CHANNEL_CONFIGURATION_STEREO 3
#define ENCODING_PCM_8BIT 3
#define ENCODING_PCM_16BIT 2
#define MODE_STREAM 1

#define MAX_POS_UPDATE_DURATION (10)
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
#define HI_AV_SYNC

class HiMediaHandlr{
public:
    HiMediaHandlr(jobject jsurface, int apiVer, HiProtocol* protocol);
    virtual int open(){return 0;};
    virtual int close(){return 0;};
    virtual int start(){return 0;};
    virtual int stop(){return 0;};
    virtual void setListener(MediaPlayerListener* listener){mListener = listener;}
    virtual int getWidth(int * width){return 0;};
    virtual int getHeight(int* height){return 0;};
    int getCurPostion(int& mPos);
    virtual void seekTo(int mSec, bool bAtEos);
    void pause();
    virtual int getSnapData(void* ptr, int* ypitch, int* upitch, int* vpitch, int* uoffset, int* voffset, int64_t* pts ){return -1;};
    virtual ~HiMediaHandlr(){};

protected:
    void updateCurPostion(int64_t& pts);
    void avsyncAudioUpdate(int64_t pts);

public:
    MediaPlayerListener* mListener;
    jobject mjsurface;
    int mApiVersion;
    HiProtocol* mProto;

    int mMediaMask;

    int mVidNeedSeek;
    int64_t mSeekTimeUs;
    int mAudNeedSeek;
    int mCurPosMs;
    int mPosUpdateCnt;
    int mPaused;
#ifdef HI_AV_SYNC
    int mbFristVidFrame;
    int mAudioLatency;
    int64_t mTimeSourceDelta;
    int64_t mLastAudioTsUs;
    int64_t mLastAudioRealTsUs;
#endif
};
#endif
