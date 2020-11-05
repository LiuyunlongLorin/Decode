#include <unistd.h>
#include "HiFileClient.h"
#include "HiMLoger.h"
#include "comm_utils.h"
#include "HiFileCacheBuf.h"
#include "Hies_proto.h"


#define TAG "HiFileClient"


extern volatile int clientProtoExit;
#define MAX_READ_TIMOUT (20)

int readVideoFromCache(void* ptr, unsigned int& dataSize, int64_t& pts, int& payload)
{
    int ret = 0;
    HI_U32 iSIze = dataSize;
    int64_t iPts = 0;
    int ptype = 0;

    ret = HI_FileClient_read_video_stream(ptr, iSIze, iPts, ptype);
    if(ret != HI_SUCCESS)
    {
        return ret;
    }
    dataSize = iSIze;
    pts = iPts;
    payload = ptype;
    return 0;
}

int readAudioFromCache(void* ptr, unsigned int& dataSize, int64_t& pts)
{
    int ret = 0;
    HI_U32 iSIze = dataSize;
    int64_t iPts = 0;

    ret = HI_FileClient_read_audio_stream(ptr, iSIze, iPts);
    if(ret != HI_SUCCESS)
    {
        return ret;
    }
    dataSize = iSIze;
    pts = iPts;
    return 0;
}

HiFileClient::HiFileClient(const char* url)
:HiProtocol(url)
{
    bConnected = HI_FALSE;
    bStarted = HI_FALSE;
    mMediaMask = 0;
    mLastVidPts = -1;
    mLastAudPts = -1;
    mLastVidReadTime = 0;
    mLastAudReadTime = 0;
    mReadThrd = INVALID_THREAD_ID;
    mEofFlag = 0;
    mffDemux=NULL;
    HI_FileClient_Init_Proto();
    mProtoType = PROTO_TYPE_FILE_CLIENT;
    mClientHandle=NULL;
}

HiFileClient::~HiFileClient()
{
    HI_FileClient_DeInit_Proto();
}


int HiFileClient::connect()
{
    char* pUrl = mUrl;
    MMLOGE(TAG, "HiFileClient connect enter\n");
    mffDemux = new HiffmpegDemux();
    pthread_mutex_init(&mReadLock, NULL);
    if(mffDemux->open(pUrl) < 0)
    {
        MMLOGE(TAG, "ffmpeg demux open failed\n");
        return HI_FAILURE;
    }
    int bHaveAudio = 0;
    int bHaveVideo = 0;
    if(mffDemux->getMediaMask(bHaveAudio, bHaveVideo) < 0)
    {
        MMLOGE(TAG, "ffmpeg demux getMediaMask\n");
        mffDemux->close();
        return HI_FAILURE;
    }
    enum AVCodecID  audCodecId=AV_CODEC_ID_NONE;
    enum AVCodecID  vidCodecId=AV_CODEC_ID_NONE;
    if(bHaveVideo)
    {
        mMediaMask |= PROTO_VIDEO_MASK;
        MMLOGE(TAG, "fileclient have video\n");
        mffDemux->getVideoType(vidCodecId);
    }
    if(bHaveAudio)
    {
        mMediaMask |= PROTO_AUDIO_MASK;
        MMLOGE(TAG, "fileclient have audio\n");
        int chnNum, sampleRate;
        mffDemux->getAudioType(audCodecId, chnNum, sampleRate);
    }

    if(mMediaMask & PROTO_VIDEO_MASK
        && vidCodecId != AV_CODEC_ID_H264
        && vidCodecId != AV_CODEC_ID_HEVC)
    {
        MMLOGE(TAG, "video formate are not supported vid: %d\n", vidCodecId);
        mffDemux->close();
        return HI_FAILURE;
    }
    if(mMediaMask & PROTO_AUDIO_MASK
        && audCodecId != AV_CODEC_ID_AAC
        && audCodecId != AV_CODEC_ID_PCM_ALAW
        && audCodecId != AV_CODEC_ID_PCM_MULAW
        && audCodecId != AV_CODEC_ID_ADPCM_G726)
    {
        MMLOGE(TAG, "audio formate are not supported aud: %d\n", audCodecId);
        MMLOGE(TAG, "HiFileClient audio error just ignore audio\n");
        mMediaMask &= (~PROTO_AUDIO_MASK);
    }
    bConnected = HI_TRUE;
    MMLOGE(TAG, "HiFileClient connect out OK\n");

    clientProtoExit = 0;
    return 0;
}

int HiFileClient::play()
{
    if(mReadThrd == INVALID_THREAD_ID)
    {
        bStarted = HI_TRUE;
        pthread_create(&mReadThrd, NULL, startRead, this);
        MMLOGE(TAG, "pthread_create readThread id: %d ", mReadThrd);
    }

    return HI_SUCCESS;
}

int HiFileClient::stop()
{
    bStarted = HI_FALSE;
    if(mReadThrd != INVALID_THREAD_ID)
    {
        if(pthread_join(mReadThrd, 0) < 0)
        {
             MMLOGI(TAG,"pthread_join mReadThrd failed");
        }
        mReadThrd = INVALID_THREAD_ID;
    }
    return HI_SUCCESS;
}

void* HiFileClient::startRead(void* ptr)
{
    HiFileClient* pHandlr = static_cast<HiFileClient*>(ptr);
    pHandlr->readThread(ptr);

    return NULL;
}

void* HiFileClient::readThread(void* ptr)
{
    int iPtype = 0;
    int ret = 0;
    int errFlag = 0;
    HI_S32 s32Ret = HI_SUCCESS;
    HI_BOOL bNeedRead = HI_TRUE;
    int bIsVideo = 0;
    AVPacket readPkt;

    void* pBuffer = av_malloc(MAX_VIDEO_FRAME_SIZE);
    if(!pBuffer)
    {
        MMLOGE(TAG, "malloc error \n");
        //notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
        return NULL;
    }
    /*read until get video frame*/
    while(bStarted)
    {
        if(mEofFlag)
        {
            usleep(50000);
            continue;
        }
        if(bNeedRead)
        {
            av_init_packet(&readPkt);
            readPkt.data = (uint8_t*)pBuffer;
            readPkt.size = MAX_VIDEO_FRAME_SIZE;
            pthread_mutex_lock(&mReadLock);
            ret = mffDemux->readFrame(readPkt, bIsVideo, iPtype);
            if(ret < 0)
            {
                MMLOGE(TAG, "mffDemux readFrame failed");
                errFlag = 1;
                pthread_mutex_unlock(&mReadLock);
                break;
            }
            if(HI_RET_FILE_EOF == ret)
            {
                mEofFlag = 1;
                MMLOGE(TAG, "fileclient read eof");
                pthread_mutex_unlock(&mReadLock);
                if(HI_FileClient_getVideoCacheCnt() <= 1)
                    notify(MEDIA_REACH_EOF, -1, -1);
                continue;
            }
            bNeedRead = HI_FALSE;
        }

        if(!bIsVideo)
        {
            //read out audio frame
            s32Ret = HI_FileClient_put_audio(readPkt.data, readPkt.size, readPkt.pts);
        }
        else
        {
            s32Ret = HI_FileClient_put_video(readPkt.data, readPkt.size, readPkt.pts, iPtype);
        }

        if(s32Ret == HI_ERR_BUF_FULL)
        {
            usleep(30000);
            pthread_mutex_unlock(&mReadLock);
            continue;
        }

        if(s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HI_FileClient_put stream failed Ret: %x", s32Ret);
            pthread_mutex_unlock(&mReadLock);
            break;
        }
        bNeedRead = HI_TRUE;
        pthread_mutex_unlock(&mReadLock);
    }
    if(pBuffer)
        av_free(pBuffer);
    if(errFlag)
    {
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
    }
    notify(DETACHING, -1, -1);
    MMLOGE(TAG, "HifileCLient readThread exit");
    return NULL;
}

int HiFileClient::disconnect()
{
    int ret  = 0;
    MMLOGI(TAG, "HiFileClient disconnect\n");
    if(bConnected)
    {
        //for h264 omxcodec close special case
        clientProtoExit = 1;
        bConnected = HI_FALSE;
        stop();
        mffDemux->close();
        delete mffDemux;
        HI_FileClient_Flush_VideoCache();
        HI_FileClient_Flush_AudioCache();
        pthread_mutex_unlock(&mReadLock);
        pthread_mutex_destroy(&mReadLock);
    }
    return HI_SUCCESS;
}

int HiFileClient::requestIFrame()
{
    MMLOGI(TAG, "HiAirPlayClient requestIFrame could  not support\n");
    return HI_FAILURE;
}

/*only used for 264*/
int HiFileClient::getSPSPPS(void* sps, int *spsSize, void* pps, int* ppsSize)
{
    /*just for H264 omxcodec need*/
    int ret = 0;
    MMLOGI(TAG, "HiFileClient getSPSPPS\n");

    if(mffDemux->getSPSPPS(sps, spsSize, pps, ppsSize) < 0)
    {
        MMLOGI(TAG, "mffDemux getSPSPPS failed\n");
        return -1;
    }

    MMLOGE(TAG, "sps len: %d pps len: %d\n", *spsSize, *ppsSize);
    MMLOGE(TAG, "sps:\n");
    int i = 0;
    for(;i<*spsSize; i++)
        MMLOGI(TAG, "0x%02x\n", ((HI_U8*)sps)[i]);
    MMLOGI(TAG, "pps:\n");
    for(i=0;i<*ppsSize; i++)
        MMLOGI(TAG, "0x%02x\n", ((HI_U8*)pps)[i]);
    return 0;
}

int HiFileClient::getVideoWidth()
{
    MMLOGI(TAG, "HiFileClient getVideoWidth\n");
    int width = 0;
    mffDemux->getVideoWidth(width);
    return width;
}

int HiFileClient::getVideoHeight()
{
    MMLOGI(TAG, "HiFileClient getVideoHeight\n");
    int height = 0;
    mffDemux->getVideoHeight(height);
    return height;
}

int HiFileClient::seekTo(int mSec)
{
    //async
    pthread_mutex_lock(&mReadLock);
    HI_FileClient_Flush_VideoCache();
    HI_FileClient_Flush_AudioCache();
    mffDemux->seekStream(mSec);
    pthread_mutex_unlock(&mReadLock);
    mEofFlag = 0;
    return 0;
}

int HiFileClient::getDuration(int& mSec)
{
    mffDemux->getDuration(mSec);
    return 0;
}

int HiFileClient::readVideoStream(void* ptr, unsigned int& dataSize, int64_t& pts, int& ptype)
{
    int iPtype = 0;
    int ret = 0;

    if(!bConnected)
    {
        MMLOGE(TAG, "fileclient proto have been closed");
        return ERROR_PROTO_EXIT;
    }
    if(HI_FileClient_getVideoCacheCnt() <= 1 && mEofFlag)
    {
        notify(MEDIA_REACH_EOF, -1, -1);
    }

    /*read until get video frame*/
    while(bConnected)
    {
        ret = readVideoFromCache(ptr, dataSize, pts, ptype);
        if(ret != 0)
        {
            //MMLOGE(TAG, "readVideoFromCache failed");
            usleep(50000);
        }
        else
            break;
    }

    if(!bConnected) return ERROR_PROTO_EXIT;

    return 0;
}

int HiFileClient::readAudioStream(void* ptr, unsigned int& dataSize, int64_t& pts)
{
    int iPtype = 0;
    int bIsVideo = 0;
    int ret = 0;

    if(!bConnected)
    {
        MMLOGE(TAG, "fileclient proto have been closed");
        return ERROR_PROTO_EXIT;
    }
    /*read until get audio frame*/
    while(bConnected)
    {
        ret = readAudioFromCache(ptr, dataSize, pts);
        if(ret != 0)
        {
            //MMLOGE(TAG, "readAudioFromCache failed");
            usleep(50000);
        }
        else
            break;
    }
    if(!bConnected) return ERROR_PROTO_EXIT;
    return 0;
}

int HiFileClient::getMediaMask(int& mediaMask)
{
    mediaMask = mMediaMask;
    return 0;
}

void HiFileClient::notify(int msg,int ext1,int ext2)
{
    mlistener->notify(msg, ext1, ext2);
}

int HiFileClient::getVideoType(eProtoVidType& type)
{
    int ret = 0;
    if(bConnected)
    {
        enum AVCodecID vidCodecId=AV_CODEC_ID_NONE;
        mffDemux->getVideoType(vidCodecId);
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
        MMLOGE(TAG, "HiFileClient not be running");
        ret = HI_FAILURE;
    }
    return ret;
}

int HiFileClient::getAudioType(stProtoAudAttr& audAttr)
{
    int ret = 0;

    if(bConnected)
    {
        enum AVCodecID audCodecId=AV_CODEC_ID_NONE;
        mffDemux->getAudioType(audCodecId, audAttr.chnNum, audAttr.sampleRate);
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
        MMLOGE(TAG, "HiFileClient not be running");
        ret = HI_FAILURE;
    }

    return ret;
}

int HiFileClient::getMediaInfo(ProtoMediaInfo& mediaInfo)
{
    return mffDemux->getMediaInfo(mediaInfo);
}

int HiFileClient::selectStreamIndex(unsigned char index)
{
    return mffDemux->selectStreamIndex(index);
}
