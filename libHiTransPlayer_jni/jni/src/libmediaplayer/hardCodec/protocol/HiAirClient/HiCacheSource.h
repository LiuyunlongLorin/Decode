#ifndef  _HI_CACHE_SOURCE_H_
#define _HI_CACHE_SOURCE_H_

#include <pthread.h>
#include "comm_utils.h"
#include "HiffmpegDemux.h"
#include "mediaPlayerListener.h"

#define HI_CACHE_LOW_WAETER_LEVEL (1*1024*1024)
#define HI_CACHE_HIGH_WAETER_LEVEL (11*1024*1024)
#define HI_CACHE_TINYLOW_WAETER_LEVEL (512 * 1024)
#define HI_CACHE_TINYHIGH_WAETER_LEVEL (3*1024*1024)


class HiCacheSource{
public:
    HiCacheSource(const char* url);
    int open();
    int close();
    int start();
    int stop();
    void* fetchThread(void* ptr);
    static void* startFetchThread(void* ptr);
    int isReachEOF(void);
    HI_U32 getCacheSize(void);
    void ensureCacheFetching(void);
    int64_t getLastCachedTimeUs(void);
    int getSPSPPS(void* sps, int *spsSize, void* pps, int* ppsSize);
    int getVideoWidth();
    int getVideoHeight();
    int seekTo(int mSec);
    int getDuration(int& mSec);
    int readVideoStream(void* ptr, unsigned int& dataSize, int64_t& pts, int& ptype);
    int readAudioStream(void* ptr, unsigned int& dataSize, int64_t& pts);
    int getMediaMask(int& bHaveAudio, int& bHaveVideo);
    int getVideoType(enum AVCodecID& vidCodecId);
    int getAudioType(enum AVCodecID& audCodecId, int& chnNum, int& sampleRate);
    void notify(int msg,int ext1,int ext2);
    void setListener(MediaPlayerListener* listener);
    int selectStreamIndex(unsigned char index);
    int getMediaInfo(ProtoMediaInfo& mediaInfo);

private:
    HiffmpegDemux* mffDemux;
    HI_BOOL mEosFlag;
    HI_BOOL mbFetching;
    pthread_t mReadThrd;
    pthread_mutex_t mReadLock;

    HI_U64 mLastVidReadTime;
    int64_t mLastVidPts;
    HI_U64 mLastAudReadTime;
    int64_t mLastAudPts;
    int mbNetworkReq;
    char mUrl[HI_MAX_URL_LENGTH];
    HI_BOOL mbStarted;
    HI_BOOL mbOpened;
    HI_BOOL mbPaused;
    MediaPlayerListener* mlistener;
};

#endif
