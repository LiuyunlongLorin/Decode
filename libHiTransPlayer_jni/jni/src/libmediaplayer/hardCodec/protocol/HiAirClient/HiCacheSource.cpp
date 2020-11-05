#include <unistd.h>
#include "HiCacheSource.h"
#include "HiMLoger.h"
#include "comm_utils.h"
#include "Hies_proto.h"
#include "HiMbufManager.h"


#define TAG "HiCacheSource"
#define MAX_PLAY_WIDTH (1920)
#define MAX_PLAY_HEIGHT (1080)

extern volatile int clientProtoExit;

HiCacheSource::HiCacheSource(const char* url)
{
    strncpy(mUrl, url, HI_MAX_URL_LENGTH-1);

    mLastVidPts = -1;
    mLastAudPts = -1;
    mLastVidReadTime = 0;
    mLastAudReadTime = 0;
    mReadThrd = 0;
    mbStarted = HI_FALSE;
    mEosFlag = HI_FALSE;
    mbFetching = HI_FALSE;
    mbPaused = HI_FALSE;
    mbOpened = HI_FALSE;
    mffDemux=NULL;
    mbNetworkReq=0;
    mlistener=NULL;
}

int HiCacheSource::open()
{
    int ret  = 0;

    if(mbOpened)
    {
        MMLOGI(TAG, "HiCacheSource already opened\n");
        return 0;
    }
    mffDemux = new HiffmpegDemux();
    pthread_mutex_init(&mReadLock, NULL);
    MMLOGI(TAG, "mffDemux open :%s\n", mUrl);
    if(mffDemux->open(mUrl) < 0)
    {
        MMLOGE(TAG, "ffmpeg demux open failed\n");
        return -1;
    }
    mbFetching = HI_TRUE;
    int width = 0, height = 0;
    mffDemux->getVideoWidth(width);
    mffDemux->getVideoHeight(height);
    HI_BOOL bNeedAlloc = HI_FALSE;

    if(MAX_PLAY_WIDTH == width && MAX_PLAY_HEIGHT == height)
    {
        bNeedAlloc = HI_TRUE;
    }
    MMLOGI(TAG, "HI_CacheBuf_Init enter\n");
    ret = HI_CacheBuf_Init(bNeedAlloc);
    if(ret < 0)
    {
        MMLOGE(TAG, "HI_CacheBuf_Init failed\n");
        mffDemux->close();
        return -1;
    }
    mbOpened = HI_TRUE;
    return 0;
}

int HiCacheSource::close()
{
    if(mbOpened)
    {
        stop();
        HI_CacheBuf_DeInit();
        mffDemux->close();
        delete mffDemux;
        pthread_mutex_unlock(&mReadLock);
        pthread_mutex_destroy(&mReadLock);
        mbOpened = HI_FALSE;
    }
    return 0;
}

int HiCacheSource::start()
{
    MMLOGI(TAG, "HiCacheSource start enter\n");

    if(mbStarted)
    {
        MMLOGE(TAG, "HiCacheSource already started\n");
        return HI_FAILURE;
    }
    mbStarted = HI_TRUE;
    pthread_create(&mReadThrd, NULL, startFetchThread, this);
    MMLOGI(TAG, "pthread_create FetchThread id: %08x ", mReadThrd);

    clientProtoExit = 0;
    return 0;
}

int HiCacheSource::isReachEOF(void)
{
    return mEosFlag;
}

HI_U32 HiCacheSource::getCacheSize(void)
{
    return HI_CacheBuf_GetVidCacheSize();
}

void HiCacheSource::ensureCacheFetching(void)
{
    mbFetching = HI_TRUE;
}

int64_t HiCacheSource::getLastCachedTimeUs(void)
{
    return HI_CacheBuf_GetLastCachedVidPts();
}

void* HiCacheSource::startFetchThread(void* ptr)
{
    HiCacheSource* pHandlr = static_cast<HiCacheSource*>(ptr);
    pHandlr->fetchThread(ptr);

    return NULL;
}

void* HiCacheSource::fetchThread(void* ptr)
{
    int ret = 0;
    int iPtype = 0;
    int bErrFlag = 0;

    void* pBuffer = av_malloc(MAX_VIDEO_FRAME_SIZE);
    if(!pBuffer)
    {
        MMLOGE(TAG, "malloc error \n");
        return NULL;
    }

    while(mbStarted)
    {
        if(mEosFlag)
        {
            usleep(50000);
            continue;
        }

        if(HI_CacheBuf_IsCacheFull())
        {
            mbFetching = HI_FALSE;
        }
        else
            mbFetching = HI_TRUE;
        if(mbFetching)
        {
            AVPacket readPkt;
            av_init_packet(&readPkt);
            readPkt.data = (uint8_t*)pBuffer;
            readPkt.size = MAX_VIDEO_FRAME_SIZE;
            int bIsVideo = 0;
            pthread_mutex_lock(&mReadLock);
            ret = mffDemux->readFrame(readPkt, bIsVideo, iPtype);
            if(ret < 0)
            {
                MMLOGE(TAG, "mffDemux readFrame failed");
                bErrFlag = 1;
                pthread_mutex_unlock(&mReadLock);
                break;
            }
            if(HI_RET_FILE_EOF == ret)
            {
                mEosFlag = HI_TRUE;
                pthread_mutex_unlock(&mReadLock);
                continue;
            }
            if(!bIsVideo)
            {
                //read out audio frame
                HI_CacheBuf_Put_Audio(readPkt.data, readPkt.size, readPkt.pts);
            }
            else
            {
                HI_CacheBuf_Put_Video(readPkt.data, readPkt.size, readPkt.pts, iPtype);
            }
            pthread_mutex_unlock(&mReadLock);
        }
        else
        {
            usleep(10000);
        }
    }

    if(pBuffer)
        free(pBuffer);
    if(bErrFlag)
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
    notify(DETACHING, -1, -1);
    MMLOGI(TAG, "fetchThread exited\n");
    return NULL;
}

void HiCacheSource::notify(int msg,int ext1,int ext2)
{
    mlistener->notify(msg, ext1, ext2);
}

void HiCacheSource::setListener(MediaPlayerListener* listener)
{
    mlistener = listener;
}

int HiCacheSource::stop()
{
    int ret  = 0;
    if(mbStarted)
    {
        MMLOGI(TAG, "HiCacheSource stop\n");
        mbStarted = HI_FALSE;
        if(pthread_join(mReadThrd, 0) < 0)
        {
             MMLOGI(TAG,"pthread_join mReadThrd failed");
        }
    }
    //for h264 omxcodec close special case
    clientProtoExit = 1;
    return HI_SUCCESS;
}

/*only used for 264*/
int HiCacheSource::getSPSPPS(void* sps, int *spsSize, void* pps, int* ppsSize)
{
    /*just for H264 omxcodec need*/
    MMLOGI(TAG, "HiCacheSource getSPSPPS\n");

    if(mffDemux->getSPSPPS(sps, spsSize, pps, ppsSize) < 0)
    {
        MMLOGI(TAG, "mffDemux getSPSPPS failed\n");
        return -1;
    }
    return 0;
}

int HiCacheSource::getVideoWidth()
{
    MMLOGI(TAG, "HiCacheSource getVideoWidth\n");
    int width = 0;
    mffDemux->getVideoWidth(width);
    return width;
}

int HiCacheSource::getVideoHeight()
{
    MMLOGI(TAG, "HiCacheSource getVideoHeight\n");
    int height = 0;
    mffDemux->getVideoHeight(height);
    return height;
}

int HiCacheSource::seekTo(int mSec)
{
    pthread_mutex_lock(&mReadLock);
    if(HI_CacheBuf_SeekTo(mSec) < 0)
    {
        HI_CacheBuf_Flush();
        mffDemux->seekStream(mSec);
    }
    mEosFlag = HI_FALSE;
    pthread_mutex_unlock(&mReadLock);
    return 0;
}

int HiCacheSource::getDuration(int& mSec)
{
    mffDemux->getDuration(mSec);
    return 0;
}

int HiCacheSource::readVideoStream(void* ptr, unsigned int& dataSize, int64_t& pts, int& ptype)
{
    int ret = 0;

    while(mbStarted)
    {
        ret = HI_CacheBuf_Get_Video(ptr, dataSize, pts, ptype);
        if(ret < 0)
        {
            //MMLOGE(TAG, "HI_CacheBuf_Get_Video failed");
            usleep(50000);
        }
        else
            break;
    }
    if(!mbStarted)
        return ERROR_PROTO_EXIT;
    return 0;
}

int HiCacheSource::readAudioStream(void* ptr, unsigned int& dataSize, int64_t& pts)
{
    int ret = 0;

    while(mbStarted)
    {
        ret = HI_CacheBuf_Get_Audio(ptr, dataSize, pts);
        if(ret < 0)
        {
            //MMLOGE(TAG, "HI_CacheBuf_Get_Audio failed");
            usleep(50000);
        }
        else
            break;
    }
    if(!mbStarted)
        return ERROR_PROTO_EXIT;
    mLastAudPts = pts;
    return 0;
}

int HiCacheSource::getMediaMask(int& bHaveAudio, int& bHaveVideo)
{
    if(mffDemux->getMediaMask(bHaveAudio, bHaveVideo) < 0)
    {
        MMLOGE(TAG, "ffmpeg demux getMediaMask failed\n");
        return -1;
    }

    return 0;
}

int HiCacheSource::getVideoType(enum AVCodecID& vidCodecId)
{
    return mffDemux->getVideoType(vidCodecId);
}

int HiCacheSource::getAudioType(enum AVCodecID& audCodecId, int& chnNum, int& sampleRate)
{
    return mffDemux->getAudioType(audCodecId, chnNum, sampleRate);
}

int HiCacheSource::getMediaInfo(ProtoMediaInfo& mediaInfo)
{
    return mffDemux->getMediaInfo(mediaInfo);
}

int HiCacheSource::selectStreamIndex(unsigned char index)
{
    return mffDemux->selectStreamIndex(index);
}
