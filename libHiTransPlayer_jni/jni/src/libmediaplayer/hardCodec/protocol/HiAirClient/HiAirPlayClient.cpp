#include <unistd.h>
#include "HiAirPlayClient.h"
#include "HiMLoger.h"
#include "comm_utils.h"
#include "Hies_proto.h"


#define TAG "HiAirPlayClient"


extern volatile int clientProtoExit;

HiAirPlayClient::HiAirPlayClient(const char* url, HiCacheSource* pCacheSource)
:HiProtocol(url)
{
    bConnected = HI_FALSE;
    bStarted = HI_FALSE;
    mMediaMask = 0;
    mSource = pCacheSource;

    mLastVidPts = 0;
    mLastAudPts = 0;
    mLastVidReadTime = 0;
    mLastAudReadTime = 0;
    mReadThrd = 0;
    mProtoType = PROTO_TYPE_REMOTE_CLIENT;
    mClientHandle=0;
    mbNetworkReq=0;
}

HiAirPlayClient::~HiAirPlayClient()
{

}

int HiAirPlayClient::play()
{
    if(mSource && mSource->start() < 0)
    {
        return HI_FAILURE;
    }
    bStarted = HI_TRUE;
    return HI_SUCCESS;
}

int HiAirPlayClient::stop()
{
    if(mSource && mSource->stop() < 0)
    {
        return HI_FAILURE;
    }
    bStarted = HI_FALSE;
    return HI_SUCCESS;
}

int HiAirPlayClient::connect()
{
    int ret  = 0;
    MMLOGI(TAG, "HiAirPlayClient connect enter\n");

    int bHaveAudio = 0;
    int bHaveVideo = 0;

    if(mSource->getMediaMask(bHaveAudio, bHaveVideo) < 0)
    {
        MMLOGE(TAG, "ffmpeg demux getMediaMask\n");
        return HI_FAILURE;
    }

    enum AVCodecID  audCodecId=AV_CODEC_ID_NONE;
    enum AVCodecID  vidCodecId=AV_CODEC_ID_NONE;

    if(bHaveVideo)
    {
        mMediaMask |= PROTO_VIDEO_MASK;
        MMLOGE(TAG, "airclient have video\n");
        if(mSource->getVideoType(vidCodecId) < 0)
        {
            MMLOGE(TAG, "airclient getVideoType failed\n");
            return HI_FAILURE;
        }
    }

    if(bHaveAudio)
    {
        mMediaMask |= PROTO_AUDIO_MASK;
        MMLOGE(TAG, "airclient have audio\n");
        int chnNum, sampleRate;
        if(mSource->getAudioType(audCodecId, chnNum, sampleRate) < 0)
        {
            MMLOGE(TAG, "airclient getAudioType failed\n");
            return HI_FAILURE;
        }
    }

    if(mMediaMask & PROTO_VIDEO_MASK
        && vidCodecId != AV_CODEC_ID_H264
        && vidCodecId != AV_CODEC_ID_HEVC)
    {
        MMLOGE(TAG, "video formate are not supported vid: %d\n", vidCodecId);
        return HI_FAILURE;
    }

    if(mMediaMask & PROTO_AUDIO_MASK
        && audCodecId != AV_CODEC_ID_AAC
        && audCodecId != AV_CODEC_ID_PCM_ALAW
        && audCodecId != AV_CODEC_ID_PCM_MULAW
        && audCodecId != AV_CODEC_ID_ADPCM_G726)
    {
        MMLOGE(TAG, "audio formate are not supported aud: %d\n", audCodecId);
        MMLOGE(TAG, "HiAirPlayClient audio error just ignore audio\n");
        mMediaMask &= (~PROTO_AUDIO_MASK);
    }

    bConnected = HI_TRUE;
    MMLOGI(TAG, "HiAirPlayClient connect out OK\n");

    clientProtoExit = 0;
    return 0;
}

int HiAirPlayClient::disconnect()
{
    int ret  = 0;
    MMLOGI(TAG, "HiAirPlayClient disconnect\n");
    if(bConnected)
    {
        bConnected = HI_FALSE;
        stop();
    }
    //for h264 omxcodec close special case
    clientProtoExit = 1;
    return HI_SUCCESS;
}

int HiAirPlayClient::requestIFrame()
{
    MMLOGI(TAG, "HiAirPlayClient requestIFrame could  not support\n");
    return HI_FAILURE;
}

/*only used for 264*/
int HiAirPlayClient::getSPSPPS(void* sps, int *spsSize, void* pps, int* ppsSize)
{
    /*just for H264 omxcodec need*/
    if(mSource->getSPSPPS(sps, spsSize, pps, ppsSize) < 0)
    {
        MMLOGI(TAG, "mffDemux getSPSPPS failed\n");
        return HI_FAILURE;
    }

    MMLOGI(TAG, "sps len: %d pps len: %d\n", *spsSize, *ppsSize);
    return HI_SUCCESS;
}

int HiAirPlayClient::getVideoWidth()
{
    MMLOGI(TAG, "HiAirPlayClient getVideoWidth\n");
    return mSource->getVideoWidth();
}

int HiAirPlayClient::getVideoHeight()
{
    MMLOGI(TAG, "HiAirPlayClient getVideoHeight\n");
    return mSource->getVideoHeight();
}

int HiAirPlayClient::seekTo(int mSec)
{
    return mSource->seekTo(mSec);
}

int HiAirPlayClient::getDuration(int& mSec)
{
    return mSource->getDuration(mSec);;
}

int HiAirPlayClient::readVideoStream(void* ptr, unsigned int& dataSize, int64_t& pts, int& ptype)
{
    return mSource->readVideoStream(ptr, dataSize, pts, ptype);
}

int HiAirPlayClient::readAudioStream(void* ptr, unsigned int& dataSize, int64_t& pts)
{
    return mSource->readAudioStream(ptr, dataSize, pts);
}

int HiAirPlayClient::getMediaMask(int& mediaMask)
{
    mediaMask = mMediaMask;
    return 0;
}

void HiAirPlayClient::notify(int msg,int ext1,int ext2)
{
    mlistener->notify(msg, ext1, ext2);
}

int HiAirPlayClient::getVideoType(eProtoVidType& type)
{
    int ret = 0;
    if(bConnected)
    {
        enum AVCodecID vidCodecId=AV_CODEC_ID_NONE;
        mSource->getVideoType(vidCodecId);
        switch(vidCodecId)
        {
            case AV_CODEC_ID_H264:
                type = Protocol_Video_H264;
                break;

            case AV_CODEC_ID_HEVC:
                type = Protocol_Video_H265;
                break;

            default:
                MMLOGE(TAG, "video format are not suported");
                ret = HI_FAILURE;
                type = Protocol_Video_BUTT;
                break;
        }
    }
    else
    {
        MMLOGE(TAG, "HiAirPlayClient not be running");
        ret = HI_FAILURE;
    }
    return ret;
}

int HiAirPlayClient::getAudioType(stProtoAudAttr& audAttr)
{
    int ret = 0;

    if(bConnected)
    {
        enum AVCodecID audCodecId=AV_CODEC_ID_NONE;
        mSource->getAudioType(audCodecId, audAttr.chnNum, audAttr.sampleRate);
        audAttr.bitwidth = 2;
        audAttr.u32BitRate = 0;

        switch(audCodecId)
        {
            case AV_CODEC_ID_AAC:
                audAttr.type = Protocol_Audio_AAC;
                break;

            case AV_CODEC_ID_PCM_ALAW:
                audAttr.type = Protocol_Audio_G711A;
                break;

            case AV_CODEC_ID_PCM_MULAW:
                audAttr.type = Protocol_Audio_G711Mu;
                break;

            case AV_CODEC_ID_ADPCM_G726:
                audAttr.type = Protocol_Audio_G726LE;
                break;

            default:
                MMLOGE(TAG, "audio format are not suported\n");
                ret = HI_FAILURE;
                audAttr.type = Protocol_Audio_BUTT;
                break;
        }
    }
    else
    {
        MMLOGE(TAG, "HiAirPlayClient not be running");
        ret = HI_FAILURE;
    }

    return ret;
}

int HiAirPlayClient::getMediaInfo(ProtoMediaInfo& mediaInfo)
{
    return mSource->getMediaInfo(mediaInfo);
}

int HiAirPlayClient::selectStreamIndex(unsigned char index)
{
    return mSource->selectStreamIndex(index);
}
