#include <unistd.h>
#include "pthread.h"
#include "HiUdpClient.h"
#include "HiMLoger.h"
#include "Hies_proto.h"
#include "HiffmpegCache.h"
#include "HiLiveRecord.h"

#define TAG "HiUdpClient"

#define MAX_RTSP_CONN_TIMEOUTMS (2000)
#define MAX_AUD_BUF_SIZE (512*1024)
#define MAX_RTSP_MBUF_READ_TIMEOUT_S (8)
/*mbuffer storage upper level, if storage percent reach, reset mbuffer
to improve realtime*/
#define MAX_WATER_LEVEL (10)

#define RTP_FUA_HEADER_LEN_H264 (2)
#define RTP_FUA_HEADER_LEN_H265 (3)

#define HEVC_NAL_HEADER_LEN (2)
//0x00 00 00 01
#define NAL_STARTCODE_LEN (4)

#define H264_RTP_CLOCK_FREQ (90)

#define NOTIFY_EXT_NULL (-1)

extern volatile HI_S32 clientProtoExit;


HiUdpClient::HiUdpClient(const char* url)
:HiProtocol(url)
{
    bConnected = HI_FALSE;
    bStarted = HI_FALSE;
    mMediaMask = 0;
    bConnectReady = HI_FALSE;
    bRecordStrm = HI_FALSE;
    hAudMbufHdl = 0;
    hVidMbufHdl = 0;
    bLostUnitlIFrameFlag = HI_TRUE;
    mReadThrd = INVALID_THREAD_ID;
    mEofFlag = 0;
    mffmpegCache=NULL;

    pu8ConstructBuf = NULL;
    u32ConstuctdataLen = 0;
    mWaterLevel = MAX_WATER_LEVEL;
    mRecordHandle = 0;
    // memset(&stStreamInfo, 0x00, sizeof(stStreamInfo));
    pthread_mutex_init(&m_conlock, NULL);
    
    mProtoType = PROTO_TYPE_UDP_CLIENT;
}

HiUdpClient::~HiUdpClient()
{
    HI_S32 s32Ret  = HI_SUCCESS;

    freeMediaBuffer();
    pthread_mutex_destroy(&m_conlock);

    if(mRecordHandle)
    {
        s32Ret = HI_LiveRecord_Destroy(mRecordHandle);
        if (s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HI_LiveRecord_Destroy  failed");
        }
        mRecordHandle = 0;
    }

}

/*在连接这一步就应该获取到分辨率、sps、pps*/
HI_S32 HiUdpClient::connect()
{
    HI_S32 s32Ret  = HI_SUCCESS;

    MMLOGI(TAG, "HiUdpClient connect enter\n");
    /*初始化连接锁*/
    pthread_mutex_lock(&m_conlock);
    char* pUrl = mUrl;
    MMLOGE(TAG, "HiUdpClient connect enter\n");
    /*New 一个ffmpeg 连接的客户端*/
    mffmpegCache = new HiffmpegCache();
    /*初始化读码流的锁*/
    pthread_mutex_init(&mReadLock, NULL);
    /*打开客户端*/
    if(mffmpegCache->open(pUrl) < 0)
    {
        MMLOGE(TAG, "ffmpeg demux open failed\n");
        return HI_FAILURE;
    }
    int bHaveAudio = 0;
    int bHaveVideo = 0;
    /*获取码流中的是否含有音视频个数*/
    if(mffmpegCache->getMediaMask(bHaveAudio, bHaveVideo) < 0)
    {
        MMLOGE(TAG, "ffmpeg demux getMediaMask\n");
        mffmpegCache->close();
        pthread_mutex_destroy(&mReadLock);
        return HI_FAILURE;
    }
    enum AVCodecID  audCodecId=AV_CODEC_ID_NONE;
    enum AVCodecID  vidCodecId=AV_CODEC_ID_NONE;
    if(bHaveVideo)
    {
        mMediaMask |= PROTO_VIDEO_MASK;
        MMLOGE(TAG, "HiUdpClient have video\n");
        mffmpegCache->getVideoType(vidCodecId);
    }
    if(bHaveAudio)
    {
        mMediaMask |= PROTO_AUDIO_MASK;
        MMLOGE(TAG, "HiUdpClient have audio\n");
        int chnNum, sampleRate;
        mffmpegCache->getAudioType(audCodecId, chnNum, sampleRate);
    }

    if(mMediaMask & PROTO_VIDEO_MASK
        && vidCodecId != AV_CODEC_ID_H264
        && vidCodecId != AV_CODEC_ID_HEVC)
    {
        MMLOGE(TAG, "video formate are not supported vid: %d\n", vidCodecId);
        mffmpegCache->close();
        pthread_mutex_destroy(&mReadLock);
        return HI_FAILURE;
    }
    if(mMediaMask & PROTO_AUDIO_MASK
        && audCodecId != AV_CODEC_ID_AAC
        && audCodecId != AV_CODEC_ID_PCM_ALAW
        && audCodecId != AV_CODEC_ID_PCM_MULAW
        && audCodecId != AV_CODEC_ID_ADPCM_G726)
    {
        MMLOGE(TAG, "audio formate are not supported aud: %d\n", audCodecId);
        MMLOGE(TAG, "HiUdpClient audio error just ignore audio\n");
        mMediaMask &= (~PROTO_AUDIO_MASK);
    }
    MMLOGE(TAG, "HiUdpClient connect out OK\n");

    /*创建Mbuf*/
    s32Ret = allocMediaBuffer();
    if (s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "allocMediaBuffer error\n");
        mffmpegCache->close();
        
        pthread_mutex_unlock(&m_conlock);
        return HI_FAILURE;
    }
    #if 0
    pRecFile = fopen("/sdcard/save.h265", "wb");

    if(!pRecFile)
    {
        MMLOGE(TAG, "fopen save.h265 failed\n");
        return HI_FAILURE;
    }
    #endif
    bConnected = HI_TRUE;
    pthread_mutex_unlock(&m_conlock);
    MMLOGI(TAG, "HiUdpClient connect out OK\n");
    clientProtoExit = 0;
    return s32Ret;
}

HI_VOID HiUdpClient::resetMediaBuffer()
{
    HI_S32 s32Ret = HI_SUCCESS;
    if(hAudMbufHdl)
    {
        s32Ret = HI_MBUF_Reset(hAudMbufHdl);
        if(s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HiUdpClient reset audio media buffer failed\n");
        }
    }

    if(hVidMbufHdl)
    {
        s32Ret = HI_MBUF_Reset(hVidMbufHdl);
        if(s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HiUdpClient reset video media buffer failed\n");
        }
    }

    if(mRecordHandle)
    {
        s32Ret = HI_LiveRecord_FlushStream(mRecordHandle);
        if (s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HI_LiveRecord_FlushStream  failed");
        }
    }

}

HI_S32 HiUdpClient::freeMediaBuffer()
{
    HI_S32 s32Ret = HI_SUCCESS;

    if(hAudMbufHdl)
    {
        s32Ret = HI_MBuf_Destroy(hAudMbufHdl);
        if(s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HiUdpClient destory audio media buffer failed\n");
        }
    }

    if(hVidMbufHdl)
    {
        s32Ret = HI_MBuf_Destroy(hVidMbufHdl);
        if(s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HiUdpClient destory video media buffer failed\n");
        }
    }

    if(pu8ConstructBuf)
    {
        free(pu8ConstructBuf);
        pu8ConstructBuf = NULL;
    }
    return HI_SUCCESS;
}

HI_S32 HiUdpClient::allocMediaBuffer()
{
    HI_S32 s32Ret = HI_SUCCESS;

    //free prev first
    freeMediaBuffer();

    pu8ConstructBuf = (HI_U8*)malloc(MAX_VIDEO_FRAME_SIZE);
    if(!pu8ConstructBuf)
    {
        MMLOGE(TAG, "HiUdpClient malloc  pu8ContructBuf failed\n");
        return HI_FAILURE;
    }
    u32ConstuctdataLen = 0;
    memset(pu8ConstructBuf, 0x00, MAX_VIDEO_FRAME_SIZE);
    if(mMediaMask & PROTO_AUDIO_MASK)
    {
        s32Ret = HI_MBuf_Create(&hAudMbufHdl, NULL, MAX_AUD_BUF_SIZE);
        if(s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HiUdpClient create audio media buffer failed\n");
            free(pu8ConstructBuf);
            return HI_FAILURE;
        }
    }

    if(mMediaMask & PROTO_VIDEO_MASK)
    {
        HI_U32 u32MbufSize = 1920 *1080;
        s32Ret = HI_MBuf_Create(&hVidMbufHdl, NULL, u32MbufSize);
        if(s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HiUdpClient create video media buffer failed\n");
            if(mMediaMask & PROTO_AUDIO_MASK)
            {
                HI_MBuf_Destroy(hAudMbufHdl);
            }
            free(pu8ConstructBuf);
            return HI_FAILURE;
        }
    }
    return HI_SUCCESS;
}


HI_S32 HiUdpClient::disconnect()
{
    HI_S32 s32Ret  = HI_SUCCESS;
    MMLOGI(TAG, "HiUdpClient disconnect\n");

    if (bConnected)
    {
        clientProtoExit = 1;
        /*去初始化操作*/
        bConnected = HI_FALSE;
        stop();
        mffmpegCache->close();
        delete mffmpegCache;
        // HI_FileClient_Flush_VideoCache();
        // HI_FileClient_Flush_AudioCache();
        pthread_mutex_unlock(&mReadLock);
        pthread_mutex_destroy(&mReadLock);
    }
    /*复位mbuf*/
    resetMediaBuffer();

    #if 0
        if(pRecFile)
        fclose(pRecFile);
        pRecFile = NULL;
    #endif
    return HI_SUCCESS;
}

HI_S32 HiUdpClient::requestIFrame()
{
    MMLOGI(TAG, "HiUdpClient requestIFrame could  not support\n");
    return HI_FAILURE;
}


HI_S32 HiUdpClient::checkMbufWaterLevel()
{
    HI_U32 u32VidBufSize =  1920*1080;

    HI_U32 u32RemainSize = HI_MBUF_GetFreeBufferSize(hVidMbufHdl);

    if(u32RemainSize > u32VidBufSize)
    {
        MMLOGE(TAG, "error happened remain bufsize large than total size\n");
        return HI_FAILURE;
    }
    HI_U32 u32UsePercent = ((u32VidBufSize-u32RemainSize)*100)/u32VidBufSize;
    HI_U32 u32CachedFrame = HI_MBUF_GetCachedFrameNum(hVidMbufHdl);
    //MMLOGE(TAG, "mbuffer  cache frame: %d\n", u32UsePercent, u32CachedFrame);
    if(u32CachedFrame > mWaterLevel)
    {
        MMLOGI(TAG, "flush mbuffer for cached max framenum: %d\n", u32CachedFrame);
        resetMediaBuffer();
        bLostUnitlIFrameFlag = HI_TRUE;
    }
}


HI_S32 HiUdpClient::getSPSPPS(HI_VOID* sps, HI_S32* spsSize, HI_VOID* pps, HI_S32* ppsSize)
{
    MMLOGI(TAG, "HiUdpClient getSPSPPS\n");
    if(mffmpegCache->getSPSPPS(sps, spsSize, pps, ppsSize) < 0)
    {
        MMLOGI(TAG, "mffmpegCache getSPSPPS failed\n");
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

HI_S32 HiUdpClient::getVideoWidth()
{
    MMLOGI(TAG, "HiUdpClient getVideoWidth\n");
     int width = 0;
     mffmpegCache->getVideoWidth(width);
    return width;
}

HI_S32 HiUdpClient::getVideoHeight()
{
    MMLOGI(TAG, "HiUdpClient getVideoHeight\n");
    int height = 0;
   mffmpegCache->getVideoHeight(height);
    return height;
}

HI_S32 HiUdpClient::readMbuffer(HI_HANDLE hMbufHdl, HiFrameInfo* pstFrame)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32CurTime = getTimeS();

    do
    {
        if(!bConnected)
        {
            MMLOGE(TAG, "rtsp client not connected, read failed");
            return HI_FAILURE;
        }
        if (getTimeS() - u32CurTime > MAX_RTSP_MBUF_READ_TIMEOUT_S)
        {
            MMLOGE(TAG, "read  stream timeout after %d s", MAX_RTSP_MBUF_READ_TIMEOUT_S);
            return HI_FAILURE;
        }

        s32Ret = HI_Mbuf_ReadStream(hMbufHdl, pstFrame);
        if (s32Ret == HI_SUCCESS)
        {
            break;
        }

        usleep(10000);
    }
    while (s32Ret != HI_SUCCESS);

    return HI_SUCCESS;
}


int HiUdpClient::play()
{
    if(mReadThrd == INVALID_THREAD_ID)
    {
        bStarted = HI_TRUE;
        pthread_create(&mReadThrd, NULL, startRead, this);
        MMLOGE(TAG, "pthread_create readThread id: %x ", mReadThrd);
    }

    return HI_SUCCESS;
}

int HiUdpClient::stop()
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

void* HiUdpClient::startRead(void* ptr)
{
    HiUdpClient* pHandlr = static_cast<HiUdpClient*>(ptr);
    pHandlr->readThread(ptr);

    return NULL;
}

void* HiUdpClient::readThread(void* ptr)
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
            ret = mffmpegCache->readFrame(readPkt, bIsVideo, iPtype);
            if(ret < 0)
            {
                MMLOGE(TAG, "mffmpegCache readFrame failed");
                errFlag = 1;
                pthread_mutex_unlock(&mReadLock);
                break;
            }
            if(HI_RET_FILE_EOF == ret) //读取缓存区数据过快
            {
                mEofFlag = 1;
                MMLOGE(TAG, "fileclient read eof");
                pthread_mutex_unlock(&mReadLock);
                if(HI_MBUF_GetCachedFrameNum(hVidMbufHdl) <= 1)
                    notify(MEDIA_REACH_EOF, -1, -1);
                continue;
            }
            bNeedRead = HI_FALSE;
        }

        if(!bIsVideo)
        {
            //read out audio frame
            s32Ret = writeAudioStream(readPkt.data, readPkt.size, readPkt.pts);
        }
        else
        {
            s32Ret = writeVideoStream(readPkt.data, readPkt.size, readPkt.pts, iPtype);
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
    MMLOGE(TAG, "HiUdpClient readThread exit");
    return NULL;
}



HI_S32 HiUdpClient::readVideoStream(void* ptr, HI_U32& dataSize, int64_t& pts, HI_S32& ptype)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HiFrameInfo stFrameInfo;

    stFrameInfo.pData = (HI_U8*)ptr;
    stFrameInfo.LenData= dataSize;

    s32Ret = readMbuffer(hVidMbufHdl, &stFrameInfo);
    if (s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "rtspclient read video  failed");
        return HI_FAILURE;
    }
    //MMLOGE(TAG, "read video frame pts: %d len: %d payload: %d\n", stFrameInfo.pts, stFrameInfo.LenData, stFrameInfo.payload);
    dataSize = stFrameInfo.LenData;
    pts = stFrameInfo.pts;
    ptype = stFrameInfo.payload;

    return 0;
}

HI_S32 HiUdpClient::readAudioStream(void* ptr, HI_U32& dataSize, int64_t& pts)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HiFrameInfo stFrameInfo;

    stFrameInfo.pData = (HI_U8*)ptr;
    stFrameInfo.LenData= dataSize;

    s32Ret = readMbuffer(hAudMbufHdl, &stFrameInfo);
    if (s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "rtspclient read audio  failed");
        return HI_FAILURE;
    }

    dataSize = stFrameInfo.LenData;
    pts = stFrameInfo.pts;
    return HI_SUCCESS;
}

HI_S32 HiUdpClient::writeVideoStream(const void* pbuf, HI_U32 dataLen , HI_U32 pts, int payload)
{
    HI_S32 s32Ret = HI_SUCCESS;

    HiFrameInfo stCurFrame;
    memset(&stCurFrame, 0x00, sizeof(stCurFrame));

    stCurFrame.pData = (HI_U8*)pbuf;
    stCurFrame.LenData = dataLen;
    stCurFrame.pts =  pts;
    stCurFrame.payload = payload;
    //MMLOGE(TAG, "write video frame pts: %d len: %d pt: %d\n", stCurFrame.pts, dataLen, payload);
    s32Ret = HI_MBUF_WriteStream(hVidMbufHdl, &stCurFrame);
    if (s32Ret == HI_RET_MBUF_FULL)
    {
        //MMLOGI(TAG, "video buf write full");
        return HI_ERR_BUF_FULL;
    }
    else if (s32Ret != HI_SUCCESS)
    {
        MMLOGI(TAG, "video HI_MBUF_WriteStream faied Ret: %d!!!\n", s32Ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 HiUdpClient::writeAudioStream(const void* pbuf, HI_U32 dataLen , HI_U32 pts)
{
    HI_S32 s32Ret = HI_SUCCESS;

    HiFrameInfo stCurFrame;
    memset(&stCurFrame, 0x00, sizeof(stCurFrame));

    stCurFrame.pData = (HI_U8*)pbuf;
    stCurFrame.LenData = dataLen;
    stCurFrame.pts =  pts;
    //MMLOGE(TAG, "write audio frame pts: %d len: %d \n", stCurFrame.pts, dataLen);
    s32Ret = HI_MBUF_WriteStream(hAudMbufHdl, &stCurFrame);
    if (s32Ret == HI_RET_MBUF_FULL)
    {
        //MMLOGI(TAG, "audio buf write full");
        return HI_ERR_BUF_FULL;
    }
    else if (s32Ret != HI_SUCCESS)
    {
        MMLOGI(TAG, "audio HI_MBUF_WriteStream faied Ret: %d!!!\n", s32Ret);
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}



HI_S32 HiUdpClient::getMediaMask(HI_S32& mediaMask)
{
    mediaMask = mMediaMask;
    return HI_SUCCESS;
}

void HiUdpClient::notify(HI_S32 msg, HI_S32 ext1, HI_S32 ext2)
{
    mlistener->notify(msg, ext1, ext2);
}

HI_S32 HiUdpClient::getVideoType(eProtoVidType& type)
{
    HI_S32 s32Ret = 0;
    pthread_mutex_lock(&m_conlock);
    if(bConnected)
    {
        enum AVCodecID vidCodecId=AV_CODEC_ID_NONE;
        mffmpegCache->getVideoType(vidCodecId);
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
                s32Ret = HI_FAILURE;
                type = Protocol_Video_BUTT;
                break;
        }
    }
    else
    {
        MMLOGE(TAG, "HiUdpClient not be running");
        s32Ret = HI_FAILURE;
    }
    pthread_mutex_unlock(&m_conlock);
    return s32Ret;
}

HI_S32 HiUdpClient::getAudioType(stProtoAudAttr& audAttr)
{
    HI_S32 s32Ret = 0;
    pthread_mutex_lock(&m_conlock);
    if(bConnected)
    {
        enum AVCodecID audCodecId=AV_CODEC_ID_NONE;
        mffmpegCache->getAudioType(audCodecId, audAttr.chnNum, audAttr.sampleRate);
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
                s32Ret = HI_FAILURE;
                audAttr.type = Protocol_Audio_BUTT;
                break;
        }
    }
    else
    {
        MMLOGE(TAG, "HiUdpClient not be running");
        s32Ret = HI_FAILURE;
    }
   
    pthread_mutex_unlock(&m_conlock);

    return s32Ret;
}

void HiUdpClient::setVideoMbufLimit(HI_U32 dropLimit, HI_U32 clearLimit)
{
    MMLOGI(TAG, "HiUdpClient setVideoMbufLimit could not support\n");
    mWaterLevel = dropLimit;
}

void  HiUdpClient::setVideoSaveFlag(HI_S32 flag) const
{
    MMLOGI(TAG, "HiUdpClient setVideoSaveFlag flag: %d\n", flag);
}

void  HiUdpClient::setRecordFlag(HI_S32 flag)
{
    HI_S32 s32Ret = HI_SUCCESS;

    MMLOGI(TAG, "HiUdpClient setRecordFlag bRec: %d\n", flag);
    HI_BOOL bRecordFlag = (flag)? HI_TRUE : HI_FALSE;
    if(bRecordStrm != bRecordFlag)
    {
        if(!bRecordFlag && mRecordHandle)
        {
            s32Ret = HI_LiveRecord_FlushStream(mRecordHandle);
            if (s32Ret != HI_SUCCESS)
            {
                MMLOGE(TAG, "HI_LiveRecord_FlushStream  failed");
            }
        }
    }
    bRecordStrm = bRecordFlag;
}

HI_S32 HiUdpClient::getRecordVideo(void* ptr, HI_U32& dataSize, int64_t& pts, HI_S32& ptype)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HiFrameInfo stFrameInfo;

    if(!mRecordHandle)
    {
        return HI_FAILURE;
    }
    stFrameInfo.pData = (HI_U8*)ptr;
    stFrameInfo.LenData= dataSize;

    s32Ret = HI_LiveRecord_ReadStream(mRecordHandle, &stFrameInfo, HI_RECORD_TYPE_VIDEO);
    if (s32Ret != HI_SUCCESS)
    {
        //MMLOGE(TAG, "HI_LiveRecord_ReadStream video failed");
        return HI_FAILURE;
    }
    //MMLOGE(TAG, "record read video frame pts: %d len: %d payload: %d\n", stFrameInfo.pts, stFrameInfo.LenData, stFrameInfo.payload);
    dataSize = stFrameInfo.LenData;
    pts = stFrameInfo.pts;
    ptype = stFrameInfo.payload;
    return HI_SUCCESS;
}

HI_S32 HiUdpClient::getRecordAudio(void* ptr, HI_U32& dataSize, int64_t& pts,HI_S32& ptype)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HiFrameInfo stFrameInfo;

    if(!mRecordHandle)
    {
        return HI_FAILURE;
    }

    stFrameInfo.pData = (HI_U8*)ptr;
    stFrameInfo.LenData= dataSize;

    s32Ret = HI_LiveRecord_ReadStream(mRecordHandle, &stFrameInfo, HI_RECORD_TYPE_AUDIO);
    if (s32Ret != HI_SUCCESS)
    {
        //MMLOGE(TAG, "HI_LiveRecord_ReadStream audio failed");
        return HI_FAILURE;
    }
    //MMLOGE(TAG, "record read audio frame pts: %d len: %d payload: %d\n", stFrameInfo.pts, stFrameInfo.LenData, stFrameInfo.payload);
    dataSize = stFrameInfo.LenData;
    pts = stFrameInfo.pts;
    ptype = stFrameInfo.payload;
    return HI_SUCCESS;
}
