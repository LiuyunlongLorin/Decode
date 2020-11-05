#ifndef _HI_H265_HANDLR_H_
#define _HI_H265_HANDLR_H_

#include "HiMediaHandlr.h"
#include "HiH265Decoder.h"
#include "HiProtocol.h"
#include "pthread.h"




    class HiH265Handlr: public HiMediaHandlr
    {
    public:
        HiH265Handlr(jobject jsurface, JNIEnv* env, MediaPlayerPCMWriter* pcmWriter, int apiVer, HiProtocol* protocol);
        void notify(int msg, int ext1, int ext2);
        int open();
        int close();
        int start();
        int stop();
        virtual int getWidth(int* width);
        virtual int getHeight(int* height);
        static void* startPlayVideo(void* ptr);
        static void* startPlayAudio(void* ptr);
        void* decodeMovie(void* ptr);
        void decodeAudio(void* ptr);
        void setMaxDecodeResolution(int width, int height);
        void getYUVFrame(stOutVidFrameInfo* frame);
        int getSnapData(void* ptr, int* ypitch, int* upitch, int* vpitch, int* uoffset, int* voffset, int64_t* pts );

    protected:
        virtual ~HiH265Handlr();
        void configAudioRender(int chnlCnt, int sampleRate, int sample_fmt);
        int configNativeWindowRender(int width, int height);

    private:
        void* pANWindow;
        JNIEnv* mjenv;
        MediaPlayerPCMWriter* mPCMWriter;
        void* mPixels;
        HiH265Decoder* mDecoder;
        int mWidth;
        int mHeight;
        int mPixFormat;
        int audConfigFlag;
        int vidConfigFlag;
        int mChnCnt;
        int mSampleRate;
        int mSampleFmt;
        int mOpened;
        pthread_t mAudThread;
        pthread_t mVidThread;
        int mRunning;
        int mMaxWidth;
        int mMaxHeight;
        int mSetVidResFlag;
        int mVidLostFlag;
        pthread_mutex_t m_snaplock;
        YUVFrame* yuvFrame;
        AVFrame* mFrame;
        void* mSwsCtx;
    };
#endif
