#ifndef _HI_PROTOCOL_H_
#define _HI_PROTOCOL_H_

//using namespace android;
#include "mediaPlayerListener.h"
#include <stdlib.h>
#include "HiMLoger.h"
#include "comm_utils.h"

#define TAG "HiProtocol"

#define PROTO_VIDEO_MASK  (0x01)
#define PROTO_AUDIO_MASK   (0x02)
#define PROTO_DATA_MASK   (0x04)
#define YUV_MAX_YLEN (1920*1080*2)
#define YUV_MAX_UVLEN (1920*540)
#define MAX_URL_LEN (1024)

typedef struct hiYUVFrame
{
    int mYpitch;
    int mUpitch;
    int mVpitch;
    int mYuvPts;
    unsigned char ydata[YUV_MAX_YLEN];
    unsigned char udata[YUV_MAX_UVLEN];
    unsigned char vdata[YUV_MAX_UVLEN];
}YUVFrame;

typedef enum tagHI_PROTO_TYPE_E
{
    PROTO_TYPE_RTSP_CLIENT,
    PROTO_TYPE_FILE_CLIENT,
    PROTO_TYPE_REMOTE_CLIENT,
    PROTO_TYPE_UDP_CLIENT,
    PROTO_TYPE_BUTT
}HI_PROTO_TYPE_E;


class HiProtocol{
public:
    HiProtocol(const char* url)
    {
        mlistener=NULL;
        bConnected = HI_FALSE;
        mMediaMask=0;
        mProtoType=PROTO_TYPE_BUTT;
        memset(mUrl, 0x00, MAX_URL_LEN);
        int ret=strlen(url);
        if(ret>0)
        {strncpy(mUrl, url, MAX_URL_LEN-1);}
     }
    virtual int connect(){MMLOGI(TAG, "HiProtocol::connect \n");return 0;};

    virtual int disconnect(){MMLOGI(TAG, "HiProtocol::disconnect \n");return 0;};
    virtual void setListener(MediaPlayerListener* listener){mlistener = listener;};
    virtual int getSPSPPS(void* sps, int *spsSize, void* pps, int* ppsSize)
        {MMLOGI(TAG, "HiProtocol::getSPSPPS \n");return 0;};
    virtual int getVideoWidth(){MMLOGI(TAG, "HiProtocol::getVideoWidth \n");return 0;};
    virtual int getVideoHeight(){MMLOGI(TAG, "HiProtocol::getVideoHeight \n");return 0;};
    virtual int readVideoStream(void* ptr, unsigned int& dataSize, int64_t& pts, int& ptype)
        {MMLOGI(TAG, "HiProtocol::readVideoStream \n");return 0;};
    virtual int readAudioStream(void* ptr, unsigned int& dataSize, int64_t& pts)
        {MMLOGI(TAG, "HiProtocol::readAudioStream \n");return 0;};
    virtual int getMediaMask(int& mediaMask)
        {MMLOGI(TAG, "HiProtocol::getMediaMask \n");return 0;};
    virtual int getVideoType(eProtoVidType& type)
        {MMLOGI(TAG, "HiProtocol::getVideoType \n");return 0;};
    virtual int getAudioType(stProtoAudAttr& audAttr)
        {MMLOGI(TAG, "HiProtocol::getAudioType \n");return 0;};
    virtual void setVideoMbufLimit(unsigned int dropLimit, unsigned int clearLimit)
        {MMLOGI(TAG, "HiProtocol::setVideoMbufLimit \n");};
    virtual int seekTo(int mSec)
        {MMLOGI(TAG, "HiProtocol::seekTo \n");return 0;};
    virtual int getDuration(int& mSec)
        {MMLOGI(TAG, "HiProtocol::getDuration \n");return 0;};
    HI_PROTO_TYPE_E getProtoType()
        {return mProtoType;};
    virtual int getMediaInfo(ProtoMediaInfo& mediaInfo)
        {MMLOGI(TAG, "HiProtocol::getMediaInfo \n");return 0;};
    virtual int selectStreamIndex(unsigned char index)
        {MMLOGI(TAG, "HiProtocol::selectStreamIndex \n");return 0;};
    virtual int play()
        {MMLOGI(TAG, "HiProtocol::play \n");return 0;};
    virtual int stop()
        {MMLOGI(TAG, "HiProtocol::stop \n");return 0;};

    HI_CHAR mUrl[MAX_URL_LEN];
    MediaPlayerListener* mlistener;
    HI_BOOL bConnected;
    HI_BOOL bStarted;
    HI_S32 mMediaMask;
    HI_PROTO_TYPE_E mProtoType;

    virtual ~HiProtocol(){};
};

#endif
