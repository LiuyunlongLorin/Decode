#include "HiMLoger.h"
#include "comm_utils.h"
#include "HiffmpegCache.h"

extern "C"
{
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/intreadwrite.h>
}

//#define __STDC_LIMIT_MACROS
//#include "stdint.h"

#define INT64_MIN        ((-9223372036854775807LL) -1)
#define INT64_MAX        ((9223372036854775807LL))

#define TAG "HiffmpegCache"

#define HEVC_NAL_IDR (19)
#define HEVC_NAL_NORMAL_P (1)
#define HEVC_NAL_VPS (32)
#define HEVC_NAL_SPS (33)
#define HEVC_NAL_PPS (34)
#define HEVC_NAL_SEI_PREFIX (39)

#define AVC_NAL_SPS (7)
#define AVC_NAL_PPS (8)
#define AVC_NAL_IDR (5)
#define AVC_NAL_NORMAL_P (1)

#define MAX_STREAM_NUM (3)
#define DEFAULT_META_STREAM_IDX (2)

static void ffmpegNotify(void* ptr, int level, const char* fmt, va_list vl)
{
    char tmpBuffer[1024];
    vsnprintf(tmpBuffer,1024,fmt,vl);

    switch(level) {
        case AV_LOG_PANIC:
            MMLOGE(TAG, "AV_LOG_PANIC: %s", tmpBuffer);
            break;

        case AV_LOG_FATAL:
            MMLOGE(TAG, "AV_LOG_FATAL: %s", tmpBuffer);
            break;

        case AV_LOG_ERROR:
            MMLOGE(TAG, "AV_LOG_ERROR: %s", tmpBuffer);
            break;

        case AV_LOG_WARNING:
            MMLOGI(TAG, "AV_LOG_WARNING: %s", tmpBuffer);
            break;

        case AV_LOG_INFO:
            MMLOGI(TAG, "%s", tmpBuffer);
            break;

        case AV_LOG_DEBUG:
            MMLOGI(TAG, "%s", tmpBuffer);

        case AV_LOG_TRACE:
            //MMLOGI(TAG, "%s", tmpBuffer);
            break;
    }
}

static void HI_ffmpeg_print_errToStr(int errNum)
{
    char errstr[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errNum, errstr, AV_ERROR_MAX_STRING_SIZE);
    MMLOGE(TAG,"errno: %d %s\n", errNum, errstr);
}

static uint8_t* findStartCode(uint8_t *pBufPtr, uint32_t u32Len, uint8_t* pHeadLen)
{
    uint8_t *pEndPtr = pBufPtr + u32Len;
    while(pBufPtr != pEndPtr)
    {
        if (0x00 == pBufPtr[0] && 0x00 == pBufPtr[1]
                && 0x00 == pBufPtr[2] && 0x01 == pBufPtr[3])
        {
            *pHeadLen = 4;
            return pBufPtr;
        }
        else if(0x00 == pBufPtr[0] && 0x00 == pBufPtr[1]
                && 0x01 == pBufPtr[2])
        {
            *pHeadLen = 3;
            return pBufPtr;
        }
        else
        {
            pBufPtr++;
        }
    }
    return NULL;
}

HiffmpegCache::HiffmpegCache()
{
    mpFmtCtx = NULL;
    mVideoStream = NULL;
    mAudioStream = NULL;
    mbHaveVideo = 0;
    mbHaveAudio = 0;
    mOpened = 0;
    mAudioStreamIdx = -1;
    mVideoStreamIdx = -1;
    mH264StreamFilter = NULL;
    mVidCodecId=AV_CODEC_ID_NONE;
    mAudCodecId=AV_CODEC_ID_NONE;
    // memset(&mHevcParaInfo, 0x00, sizeof(HiHevcParaSetInfo));

    memset(&mMetaTrkInfo, 0x00, sizeof(mMetaTrkInfo));
    mLastReadPts=0;
}

HiffmpegCache::~HiffmpegCache()
{   

}

/* set resolution directly if already known, instead get by decode*/
int HiffmpegCache::getMetaResolution(enum AVCodecID codec_id, AVPacket* pPkt, int* pWidth, int* pHeight)
{
   int ret = 0;
    return ret;
}




int HiffmpegCache::parseMetaSPS(enum AVCodecID codecId, uint8_t* pStrmPtr, int dataLen)
{
    int bHasSps = 0;
    int bHasPps = 0;
    uint8_t* pBeginPtr = pStrmPtr;
    uint8_t* pEndPtr = pStrmPtr+dataLen;
    if(codecId == AV_CODEC_ID_H264)
    {
        while(pBeginPtr < pEndPtr)
        {
            uint8_t u8NalHeadLen = 0;
            uint8_t *pNalPtr = findStartCode(pBeginPtr, (pEndPtr-pBeginPtr), &u8NalHeadLen);
            if(!pNalPtr)
            {
                MMLOGE(TAG, "get sps pps out\n");
                break;
            }
            uint8_t u8NalType = (*(pNalPtr+u8NalHeadLen)) & 0x1F;
            uint8_t * pNextPtr = findStartCode(pNalPtr+u8NalHeadLen, (pEndPtr-pNalPtr), &u8NalHeadLen);
            if(!pNextPtr) pNextPtr = pEndPtr;

            if(u8NalType == AVC_NAL_SPS)
            {
                mMetaTrkInfo.stAvcParaInfo.spsLen = pNextPtr-pNalPtr;
                if(mMetaTrkInfo.stAvcParaInfo.spsLen > MAX_PARASET_LENGTH
                    || !mMetaTrkInfo.stAvcParaInfo.spsLen)
                {
                    MMLOGE(TAG, "key frame sps len:%d overflow buf\n", mMetaTrkInfo.stAvcParaInfo.spsLen);
                    return -1;
                }
                memcpy(mMetaTrkInfo.stAvcParaInfo.sps, pNalPtr, mMetaTrkInfo.stAvcParaInfo.spsLen);
                MMLOGE(TAG, "meta frame sps len:%d\n", mMetaTrkInfo.stAvcParaInfo.spsLen);
                bHasSps = 1;
            }
            else  if(u8NalType == AVC_NAL_PPS)
            {
                mMetaTrkInfo.stAvcParaInfo.ppsLen = pNextPtr-pNalPtr;
                if(mMetaTrkInfo.stAvcParaInfo.ppsLen > MAX_PARASET_LENGTH
                    || !mMetaTrkInfo.stAvcParaInfo.ppsLen)
                {
                    MMLOGE(TAG, "key frame pps len:%d overflow buf\n", mMetaTrkInfo.stAvcParaInfo.ppsLen);
                    return -1;
                }
                memcpy(mMetaTrkInfo.stAvcParaInfo.pps, pNalPtr, mMetaTrkInfo.stAvcParaInfo.ppsLen);
                MMLOGE(TAG, "meta frame pps len:%d\n", mMetaTrkInfo.stAvcParaInfo.ppsLen);
                bHasPps = 1;
            }
            pBeginPtr += u8NalHeadLen;
            if(bHasPps&&bHasSps)
            {
                return 0;
            }
        }
    }
    else if(codecId == AV_CODEC_ID_HEVC)
    {
        /*hevc sps info no need for current hevc decoder of hisilicon*/
        return 0;
    }
    return -1;
}

int HiffmpegCache::getMetaCodecpar(AVFormatContext *pInCtx, enum AVCodecID* pRawCodecId, int* pwidth , int* pheight)
{
    

    return -1;
}

/*获取分辨率  sps和pps*/
void HiffmpegCache::probeMetaTrack(AVStream*  pVideoStream)
{
    
    mMetaTrkInfo.height = pVideoStream->codecpar->height;
    mMetaTrkInfo.width = pVideoStream->codecpar->width;
    mMetaTrkInfo.codecId = pVideoStream->codecpar->codec_id;
    parseMetaSPS(pVideoStream->codecpar->codec_id, pVideoStream->codecpar->extradata, pVideoStream->codecpar->extradata_size);
}

void HiffmpegCache::discardStream()
{
    int i = 0;

    for(i=0;i<mpFmtCtx->nb_streams;i++)
    {
        if(i != mAudioStreamIdx && i !=  mVideoStreamIdx)
        {
            mpFmtCtx->streams[i]->discard = AVDISCARD_ALL;
            MMLOGI(TAG,"discard index %d audidx:%d vididx:%d\n", i, mAudioStreamIdx, mVideoStreamIdx);
        }
        else
        {
            mpFmtCtx->streams[i]->discard = AVDISCARD_DEFAULT;
        }
    }

}

/*打开远端的udp流，解析出sps pps*/
int HiffmpegCache::open(char* pUrl)
{
    int ret = 0, got_frame, i = 0;
    /*设置码流属性*/
	AVDictionary* options = NULL;
    /* ffmpeg 注册*/
    av_register_all();
    if(strlen(pUrl) == 0)
    {
        MMLOGE(TAG, "ffmpeg demux  url len is 0 \n");
        goto failed;
    }
    /*ffmpeg网络协议注册*/
    if(avformat_network_init() < 0)
    {
        MMLOGE(TAG, "avformat_network_init failed\n");
        goto failed;
    }
    MMLOGE(TAG, "open url before \n");
    mpFmtCtx = avformat_alloc_context();
	av_dict_set(&options, "buffer_size", "40960000", 0);
	av_dict_set(&options, "preset", "superfast", 0);
	av_dict_set(&options, "tune", "zerolatency", 0);
	av_dict_set(&options, "stimeout", "5000000", 0);  

    /*打开远端url */
    if (avformat_open_input(&mpFmtCtx, pUrl, NULL, &options) < 0)
    {
        MMLOGE(TAG, "Could not open source file %s\n", pUrl);
        goto net_deinit;
    }
    MMLOGI(TAG, "input format: %s\n", mpFmtCtx->iformat->name);

    /* 读取码流信息 */
    if (avformat_find_stream_info(mpFmtCtx, NULL) < 0)
    {
        MMLOGE(TAG, "Could not find stream information\n");
        avformat_network_deinit();
        avformat_close_input(&mpFmtCtx);
        goto input_close;
    }

    av_log_set_callback(ffmpegNotify);
    // probeMetaTrack(mpFmtCtx);

        /*find video stream*/
        ret = av_find_best_stream(mpFmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        if (ret < 0)
        {
            MMLOGE(TAG, "Could not find video stream in input file '%s'\n",pUrl);
            mbHaveVideo = 0;
        }
        else
        {
            mVideoStream = mpFmtCtx->streams[ret];
            mVideoStreamIdx = ret;
            mVidCodecId = mVideoStream->codecpar->codec_id;
            MMLOGI(TAG, "index:%d video codec type  %s width:%d height:%d\n", mVideoStreamIdx, avcodec_get_name(mVidCodecId),
                mVideoStream->codecpar->width, mVideoStream->codecpar->height);

            /*里面存有sps pps*/
            MMLOGI(TAG, "extra datasize: %d", mVideoStream->codecpar->extradata_size);
             MMLOGI(TAG, "[0] = %x  [1]=%x, [2]=%x,  [3]=%x, [4]=%x\n ", mVideoStream->codecpar->extradata[0],
              mVideoStream->codecpar->extradata[1], mVideoStream->codecpar->extradata[2], mVideoStream->codecpar->extradata[3],
              mVideoStream->codecpar->extradata[4]);
            for(i = 0;i<mVideoStream->codecpar->extradata_size;i++)
            {
                // MMLOGE(TAG, "0x%02x", mVideoStream->codecpar->extradata[i]);
                MMLOGI(TAG, "%x  ", mVideoStream->codecpar->extradata[i] );
            }
            // printf("\n");
            probeMetaTrack(mVideoStream);
            /*需要解析一下 sps pps */
            // ret = createStreamFilter();
            // if(ret != HI_SUCCESS)
            // {
            //     MMLOGE(TAG,"createStreamFilter err \n");
            //     goto input_close;
            // }
            mbHaveVideo = 1;
            MMLOGI(TAG,"video timebase: %d %d \n", mVideoStream->time_base.den, mVideoStream->time_base.num);
        }



    /*udp 无audio*/
    mbHaveAudio = 0;

    /* dump input information to stderr */
    av_dump_format(mpFmtCtx, 0, pUrl, 0);

    discardStream();

    MMLOGI(TAG,"Demuxing open.\n");
    mOpened = 1;
    return 0;

input_close:
    avformat_close_input(&mpFmtCtx);
net_deinit:
    avformat_network_deinit();
failed:
    return -1;
}


int HiffmpegCache::close()
{
    if(mOpened)
    {
        avformat_close_input(&mpFmtCtx);
        avformat_network_deinit();
        mOpened = 0;
    }
    return 0;
}

int HiffmpegCache::getSPSPPS(void* sps, int *spsSize, void* pps, int* ppsSize)
{
     if(mVidCodecId != AV_CODEC_ID_H264 && mVidCodecId != AV_CODEC_ID_HEVC)
    {
        MMLOGE(TAG,"ffmpeg demux getSPSPPS can not support\n");
        return -1;
    }
    if(mVidCodecId == AV_CODEC_ID_H264)
    {
        MMLOGE(TAG,"mVidCodecId == AV_CODEC_ID_H264\n");
        uint8_t* pSPS = NULL;
        uint8_t* pPPS = NULL;
        int spslen = 0;
        int ppslen = 0;
        pSPS = mMetaTrkInfo.stAvcParaInfo.sps;
        pPPS = mMetaTrkInfo.stAvcParaInfo.pps;
        spslen = mMetaTrkInfo.stAvcParaInfo.spsLen;
        ppslen = mMetaTrkInfo.stAvcParaInfo.ppsLen;
        if(spslen > *spsSize || ppslen > *ppsSize)
        {
            MMLOGE(TAG, "sps or pps buffer size is too small\n");
            return -1;
        }
        memcpy(sps, pSPS+4, spslen-4);
        *spsSize = spslen-4;
        memcpy(pps, pPPS+4, ppslen-4);
        *ppsSize = ppslen-4;
    }
    else if(mVidCodecId == AV_CODEC_ID_HEVC)
    {
        MMLOGE(TAG,"mVidCodecId == AV_CODEC_ID_HEVC\n");
        MMLOGE(TAG, "Dont  Support  Now\n");
    }
    else
    {
        MMLOGE(TAG,"Dont Know  Video Type\n");
    }
    


     return 0;
}

int HiffmpegCache::getMediaMask(int& bHaveAudio, int& bHaveVideo)
{
    if(!mOpened)
    {
        MMLOGE(TAG,"ffmpeg demux have not been open\n");
        return -1;
    }
    bHaveAudio = mbHaveAudio;
    bHaveVideo = mbHaveVideo;
    return 0;
}

int HiffmpegCache::getVideoWidth(int& width)
{
    if(!mOpened || !mbHaveVideo)
    {
        MMLOGE(TAG,"ffmpeg demux have not been open  or have no video stream\n");
        return -1;
    }
    width = mMetaTrkInfo.width;
    return 0;
}

int HiffmpegCache::getVideoHeight(int& height)
{
    if(!mOpened || !mbHaveVideo)
    {
        MMLOGE(TAG,"ffmpeg demux have not been open  or have no video stream\n");
        return -1;
    }
    height = mMetaTrkInfo.height;
    return 0;
}

int HiffmpegCache::getVideoType(enum AVCodecID& vidCodecId)
{
    if(!mOpened || !mbHaveVideo)
    {
        MMLOGE(TAG,"ffmpeg demux have not been open  or have no video stream\n");
        return -1;
    }
    vidCodecId = mVidCodecId;
    return 0;
}


int HiffmpegCache::getAudioType(enum AVCodecID& audCodecId, int& chnNum, int& sampleRate)
{
    if(!mOpened || !mbHaveAudio)
    {
        MMLOGE(TAG,"ffmpeg demux have not been open  or have no audio stream\n");
        return -1;
    }
    audCodecId = mAudCodecId;
    chnNum = mAudioStream->codecpar->channels;
    sampleRate = mAudioStream->codecpar->sample_rate;
    return 0;
}



int HiffmpegCache::readDataFFmpeg(AVPacket& readPkt, int& bIsVideo, int& payload)
{
    /* initialize packet, set data to NULL, let the demuxer fill it */
    int ret = 0;
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;

    /* read frames from the file */
    ret = av_read_frame(mpFmtCtx, &pkt);
    if (ret == AVERROR_EOF)
    {
        MMLOGE(TAG,"read endof ret: %d\n", ret);
        HI_ffmpeg_print_errToStr(ret);
        return HI_RET_FILE_EOF;
    }
    if(ret < 0)
    {
        MMLOGE(TAG,"av_read_frame failed\n");
        return -1;
    }
    if(pkt.size <= 0)
    {
        MMLOGE(TAG,"HiffmpegDemux err: read stream len 0\n");
        return -1;
    }

    //MMLOGI(TAG,"org read frame len: %d orgpts: %lld  duration: %lld index: %d \n", pkt.size, pkt.pts, pkt.duration, pkt.stream_index);

    uint8_t* pWr = readPkt.data;
    int WrSize = 0;

    if(pkt.stream_index == mAudioStreamIdx)
    {
        if(!mbHaveAudio)
        {
            MMLOGE(TAG,"err: there no audio in file\n");
            av_packet_unref(&pkt);
            return -1;
        }
        bIsVideo = 0;
        memcpy(pWr, pkt.data, pkt.size);
        WrSize = pkt.size;
        //pkt.pts is time base on stream->timebase, need expressed in AV_TIME_BASE units
        mLastReadPts = av_rescale_q(pkt.pts-mAudioStream->start_time, mAudioStream->time_base, AV_TIME_BASE_Q);
    }
    else if(pkt.stream_index == mVideoStreamIdx)
    {
        if(!mbHaveVideo)
        {
            MMLOGE(TAG,"err: there no video in file\n");
            av_packet_unref(&pkt);
            return -1;
        }

        if(readPkt.size < pkt.size+(3*MAX_PARASET_LENGTH))
        {
            MMLOGE(TAG,"readFrame input buffer size too short: %d dataLen %d\n", readPkt.size, pkt.size);
            return -1;
        }
        bIsVideo = 1;
        //pkt.pts is time base on stream->timebase, need expressed in AV_TIME_BASE units
        mLastReadPts = av_rescale_q(pkt.pts-mVideoStream->start_time, mVideoStream->time_base, AV_TIME_BASE_Q);
    
        if(mVidCodecId == AV_CODEC_ID_H264)
        {
            uint8_t u8NalHeadLen = 0;
            uint8_t *pNalPtr = findStartCode(pkt.data, pkt.size, &u8NalHeadLen);
            uint8_t u8NalType = (*(pNalPtr+u8NalHeadLen)) & 0x1F;
            payload = ((u8NalType == AVC_NAL_IDR) || (u8NalType == AVC_NAL_SPS))?  AVC_NAL_IDR : AVC_NAL_NORMAL_P;
        }
        else if(mVidCodecId == AV_CODEC_ID_HEVC)
        {
            payload = (pkt.flags & AV_PKT_FLAG_KEY)?  HEVC_NAL_IDR : HEVC_NAL_NORMAL_P;
        }
        memcpy(pWr, pkt.data, pkt.size);
        WrSize = pkt.size;
        
    }
    else
    {
        //MMLOGI(TAG,"read stream index: %d in input file,  ignore\n", pkt.stream_index);
        av_packet_unref(&pkt);
        return HI_RET_OTHER_STREAM;
    }

    readPkt.size = WrSize;
    //readPkt.pts = pkt.pts;
    readPkt.pts = mLastReadPts/1000;
    readPkt.flags = pkt.flags;
    //MMLOGI(TAG,"read frame len: %d orgpts: %lld realpts: %lld duration: %lld index: %d \n", pkt.size, pkt.pts, readPkt.pts, pkt.duration, pkt.stream_index);
    av_packet_unref(&pkt);
    return 0;
}



int HiffmpegCache::readFrame(AVPacket& readPkt, int& bIsVideo, int& payload)
{
    int ret = 0;
    while((ret = readDataFFmpeg(readPkt, bIsVideo, payload)) == HI_RET_OTHER_STREAM)
    {
        continue;
    }
    return ret;
}

int HiffmpegCache::getMediaInfo(ProtoMediaInfo& mediaInfo)
{
    
    return HI_SUCCESS;
}
