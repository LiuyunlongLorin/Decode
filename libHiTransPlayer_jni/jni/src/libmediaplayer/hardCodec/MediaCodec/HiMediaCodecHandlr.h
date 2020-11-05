#ifndef _HI_MEDIA_CODEC_HANDLR_H_
#define _HI_MEDIA_CODEC_HANDLR_H_

#include "HiMediaHandlr.h"
#include "mediaPlayerListener.h"
#include "HiffmpegDecoder.h"
#include <pthread.h>
#include "media/NdkMediaCodec.h"

//namespace android{
#define FORMAT_VIDEO_HEVC "video/hevc"
#define FORMAT_VIDEO_AVC "video/avc"


class HiMediaCodecHandlr:public HiMediaHandlr{
public:
    HiMediaCodecHandlr(jobject jsurface, JNIEnv* env, int apiVer, HiProtocol* protocol, MediaPlayerPCMWriter* pcmWriter);
    int open();
    int close();
    int start();
    int stop();
    int getWidth(int * width);
    int getHeight(int* height);
    void configAudioRender(int chnlCnt, int sampleRate, int sample_fmt);
    static void* startDecVideo(void* ptr);
    static void* startRenderVideo(void* ptr);
    static void* startPlayAudio(void * ptr);
    void* decodeMovie(void* ptr);
    void* renderMovie(void* ptr);
    void* decodeAudio(void* ptr);
    int vidReadFrame(bool* bSyncIFrame, void* ptr, unsigned int& dataSize, int64_t& pts, int& ptype);
    void notify(int msg, int ext1, int ext2);
#ifdef HI_AV_SYNC
    bool syncWithAudio(int64_t timeUs);
#endif
protected:
    virtual ~HiMediaCodecHandlr();
    MediaPlayerPCMWriter* mPCMWriter;
    HiffmpegDecoder* mDecoder;
    int audConfigFlag;
    int mOpened;
    int mChnCnt;
    int mSampleRate;
    int mSampleFmt;
    AMediaCodec* mediaCodc;
    JNIEnv* mjenv;
    pthread_t mAudThread;
    pthread_t mVidThread;
    pthread_t mRendThread;
    pthread_mutex_t mVidFlushLock;
    int mRunning;
    int mbFirstInputFrame;
};
//}
#endif
