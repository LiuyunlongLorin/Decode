#ifndef  _HI_AIR_PLAY_CLIENT_H_
#define _HI_AIR_PLAY_CLIENT_H_

#include <pthread.h>
#include "HiProtocol.h"
#include "HiCacheSource.h"

class HiAirPlayClient:public HiProtocol{
public:
    HiAirPlayClient(const char* url, HiCacheSource* pCacheSource);
    virtual int getSPSPPS(void* sps, int *spsSize, void* pps, int* ppsSize);
    virtual int getVideoWidth();
    virtual int getVideoHeight();
    virtual int readVideoStream(void* ptr, unsigned int& dataSize, int64_t& pts, int& ptype);
    virtual int readAudioStream(void* ptr, unsigned int& dataSize, int64_t& pts);
    virtual int connect();
    virtual int disconnect();
    virtual int getMediaMask(int& mediaMask);
    virtual int requestIFrame();
    void notify(int msg,int ext1,int ext2);
    virtual ~HiAirPlayClient();
    virtual int getVideoType(eProtoVidType& type);
    virtual int getAudioType(stProtoAudAttr& audAttr);
    virtual int seekTo(int mSec);
    virtual int getDuration(int& mSec);
    virtual int selectStreamIndex(unsigned char index);
    virtual int getMediaInfo(ProtoMediaInfo& mediaInfo);
    virtual int play();
    virtual int stop();

private:
    HI_U32 mClientHandle;
    pthread_t mReadThrd;

    HI_U64 mLastVidReadTime;
    int64_t mLastVidPts;
    HI_U64 mLastAudReadTime;
    int64_t mLastAudPts;
    int mbNetworkReq;
    HiCacheSource* mSource;
};

#endif
