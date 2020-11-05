#include "HiMLoger.h"
#include "comm_utils.h"
#include "HiffmpegDemux.h"

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

#define TAG "HiffmpegDemux"

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

int HI_ffmpeg_AddNALPrefix(uint8_t* pInData, int dataLen, uint8_t* pOutBuf, int* outLen)
{
    uint8_t* pOutPtr = pOutBuf;
    HI_U8 prefix[4] = {0x00, 0x00, 0x00, 0x01};

    memcpy(pOutBuf, prefix, 4);
    pOutBuf += 4;

    if(*outLen -4 < dataLen)
    {
        MMLOGE(TAG, "addprefix outbuf len too short: %d dataLen: %d\n", *outLen-4, dataLen);
        return -1;
    }
    memcpy(pOutBuf, pInData, dataLen);

    *outLen = dataLen+4;
    return 0;
}

int HI_ffmpeg_decode_H264_extraData(uint8_t* extra_data, int data_size, uint8_t** pSps, uint8_t** pPps)
{
    uint8_t* pData = extra_data;
    int leftSize = data_size;
    int bFind = 0;

    *pSps = pData;
    pData += 4;
    while(pData < extra_data +  data_size)
    {
        if(pData[0] == 0 && pData[1] == 0 &&
            pData[2] == 0 && pData[3] == 1)
        {
            bFind = 1;
            break;
        }
        else
            pData++;
    }
    if(!bFind)
    {
        MMLOGE(TAG, "could not find H264 pps\n");
        return -1;
    }
    *pPps = pData;
    return 0;
}

int HI_ffmpeg_decode_Hevc_extraData(uint8_t* extra_data, int data_size, HiHevcParaSetInfo* pSetInfo)
{
    uint8_t* pData = extra_data;
    int leftSize = data_size;
    if(leftSize < 3)
    {
        MMLOGE(TAG, "input extra data len is too short: %d\n", leftSize);
        return -1;
    }
    if(pData[0] || pData[1] || pData[2] > 1)
    {
        //skip 21
        pData+=21;
        int nalLenSize = (pData[0]&3)+1;
        int nalNum = pData[1];
        pData+=2;
        leftSize -= 23;
        MMLOGI(TAG, "nal num: %d\n", nalNum);
        if(leftSize <= 0)
        {
            MMLOGE(TAG, "there no parameter set info\n");
            return 0;
        }
        for(int i=0; i<nalNum;i++)
        {
            int type =pData[0];
            pData+=1;
            int cnt =AV_RB16(pData);
            pData+=2;

            for(int j = 0; j < cnt; j++)
            {

                int nalSize =AV_RB16(pData);
                pData+=2;

                uint8_t type = (pData[0] >> 1) & 0x3f;
                MMLOGI(TAG, "nal size: %d type: %d\n", nalSize, type);
                switch (type)
                {
                    case HEVC_NAL_VPS:
                        pSetInfo->vpsLen = MAX_PARASET_LENGTH;
                        HI_ffmpeg_AddNALPrefix(pData, nalSize, pSetInfo->vps, &pSetInfo->vpsLen);
                        break;
                    case HEVC_NAL_SPS:
                        pSetInfo->spsLen = MAX_PARASET_LENGTH;
                        HI_ffmpeg_AddNALPrefix(pData, nalSize, pSetInfo->sps, &pSetInfo->spsLen);
                        break;
                    case HEVC_NAL_PPS:
                        pSetInfo->ppsLen = MAX_PARASET_LENGTH;
                        HI_ffmpeg_AddNALPrefix(pData, nalSize, pSetInfo->pps, &pSetInfo->ppsLen);
                        break;
                    case HEVC_NAL_SEI_PREFIX:
                        pSetInfo->seiLen = MAX_PARASET_LENGTH;
                        HI_ffmpeg_AddNALPrefix(pData, nalSize, pSetInfo->sei, &pSetInfo->seiLen);
                        break;

                    default:
                        MMLOGE(TAG, "extra data nal type err\n");
                        return -1;
                }
                pData+=nalSize;
            }
        }

    }
    else
    {
        MMLOGE(TAG, "extra data isnot hevc format\n");
        return -1;
    }

    if(!pSetInfo->spsLen || !pSetInfo->ppsLen || !pSetInfo->vpsLen)
    {
        MMLOGE(TAG, "do not get enought para set info\n");
        return -1;
    }
    return 0;
}

static int HI_ffmpeg_Hevc_mp4toannexb_filter(uint8_t *poutbuf, int *poutbuf_size,
                                   const uint8_t *pInbuf, int buf_size, HiHevcParaSetInfo* pParaInfo)
{
    int i;
    uint8_t unit_type;
    int32_t nal_size;
    const uint8_t *buf_end = pInbuf + buf_size;
    const uint8_t *buf = pInbuf;
    int ret = 0;
    const int length_size = 4;
    int idr_vps_sps_pps_seen = 0;
    uint8_t* pOutPtr = poutbuf;
    int WrSize = 0;
    uint8_t start_code[4] = {0x00, 0x00, 0x00, 0x01};
    int cumul_size = 0;

    do {
        if (buf + length_size > buf_end)
            goto fail;

        for (nal_size = 0, i = 0; i<length_size; i++)
            nal_size = (nal_size << 8) | buf[i];

        buf += length_size;
        unit_type = ((*buf)>>1) & 0x3f;

        if (buf + nal_size > buf_end || nal_size < 0)
            goto fail;

        if (unit_type == HEVC_NAL_VPS|| unit_type == HEVC_NAL_SPS || unit_type == HEVC_NAL_PPS)
            idr_vps_sps_pps_seen = 1;

        /* prepend only to the first type 19 NAL unit of an IDR picture, if no sps/pps are already present */
        if ((unit_type == HEVC_NAL_IDR || unit_type == HEVC_NAL_SEI_PREFIX) && !idr_vps_sps_pps_seen)
        {
            memcpy(pOutPtr, pParaInfo->vps, pParaInfo->vpsLen);
            pOutPtr += pParaInfo->vpsLen;
            WrSize += pParaInfo->vpsLen;
            memcpy(pOutPtr, pParaInfo->sps, pParaInfo->spsLen);
            pOutPtr += pParaInfo->spsLen;
            WrSize += pParaInfo->spsLen;
            memcpy(pOutPtr, pParaInfo->pps, pParaInfo->ppsLen);
            pOutPtr += pParaInfo->ppsLen;
            WrSize += pParaInfo->ppsLen;
            memcpy(pOutPtr, pParaInfo->sei, pParaInfo->seiLen);
            pOutPtr += pParaInfo->seiLen;
            WrSize += pParaInfo->seiLen;
            idr_vps_sps_pps_seen = 1;
            //MMLOGD(TAG, "add vps sps pps sei");
        }

        {
            memcpy(pOutPtr, start_code, 4);
            pOutPtr +=4;
            WrSize +=4;
        }
        memcpy(pOutPtr, buf, nal_size);
        pOutPtr += nal_size;
        WrSize += nal_size;

        //MMLOGD(TAG, "nalType: %d len: %d", unit_type, nal_size);
        buf += nal_size;
        cumul_size += nal_size + length_size;
    } while (cumul_size < buf_size);

    *poutbuf_size = WrSize;
    return 0;

fail:
    *poutbuf_size = 0;
    MMLOGE(TAG, "hevc mp4 to annexb err happen");
    return -1;
}

static void HI_ffmpeg_print_errToStr(int errNum)
{
    char errstr[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errNum, errstr, AV_ERROR_MAX_STRING_SIZE);
    MMLOGE(TAG,"errno: %d %s\n", errNum, errstr);
}

static HI_S32 HI_ffmpeg_AVBSF_Init(AVBSFContext** ppstBsfCtx,
    const AVStream* pstStream, const HI_CHAR* pBsfName)
{
    HI_S32 s32Ret = HI_FAILURE;
    AVBSFContext* pstBsfCtx = NULL;

    const AVBitStreamFilter* pstABSFilter = av_bsf_get_by_name(pBsfName);
    if (!pstABSFilter)
    {
        MMLOGE(TAG, "Unknown bitstream filter %s\n", pBsfName);
        return HI_FAILURE;
    }

    s32Ret = av_bsf_alloc(pstABSFilter, &pstBsfCtx);
    if (s32Ret < 0)
    {
        MMLOGE(TAG, "av_bsf_alloc failed ret %d\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = avcodec_parameters_copy(pstBsfCtx->par_in, pstStream->codecpar);
    if (s32Ret < 0)
    {
        MMLOGE(TAG, "avcodec_parameters_from_context failed ret %d\n", s32Ret);
        av_bsf_free(&pstBsfCtx);
        return HI_FAILURE;
    }

    pstBsfCtx->time_base_in = pstStream->time_base;

    s32Ret = av_bsf_init(pstBsfCtx);
    if (s32Ret < 0)
    {
        MMLOGE(TAG, "Error initializing bitstream filter: %s\n", pstBsfCtx->filter->name);
        av_bsf_free(&pstBsfCtx);
        return HI_FAILURE;
    }

    *ppstBsfCtx = pstBsfCtx;
    return  HI_SUCCESS;
}

static HI_S32 HI_ffmpeg_AVBSF_Filter(AVBSFContext* pstBsfCtx,
    AVPacket* pSrcPkt, AVPacket* pDstPkt)
{
    HI_S32 s32Ret = HI_FAILURE;

    s32Ret = av_bsf_send_packet(pstBsfCtx, pSrcPkt);
    if (s32Ret < 0)
    {
        MMLOGE(TAG, "av_bsf_send_packet failed ret %d\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = av_bsf_receive_packet(pstBsfCtx, pDstPkt);
    if (s32Ret == AVERROR(EAGAIN) || s32Ret == AVERROR_EOF)
    {
        MMLOGE(TAG, "need more data input or no data output  ret: %d\n", s32Ret);
        return HI_RET_FILE_EOF;
    }
    else if (s32Ret < 0)
    {
        MMLOGE(TAG, "av_bsf_receive_packet failed ret %d\n", s32Ret);
        return HI_RET_FILE_EOF;
    }

    AVPacket stAvpkt = { 0 };
    av_init_packet(&stAvpkt);
    /* drain all the remaining packets we cannot return */
    while (s32Ret >= 0)
    {
        s32Ret = av_bsf_receive_packet(pstBsfCtx, &stAvpkt);
        if(s32Ret >= 0)
        {
            MMLOGE(TAG, "av_bsf_receive_packet extra packet len: %d are not handled\n", stAvpkt.size);
        }
        av_packet_unref(&stAvpkt);
    }

    //todo: here we only consider about h264,h265, so extradata update do not
    //handle here
    return HI_SUCCESS;
}

static HI_VOID HI_ffmpeg_AVBSF_Deinit(AVBSFContext** ppstBsfCtx)
{
    av_bsf_free(ppstBsfCtx);
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

HiffmpegDemux::HiffmpegDemux()
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
    memset(&mHevcParaInfo, 0x00, sizeof(HiHevcParaSetInfo));

    memset(&mMetaTrkInfo, 0x00, sizeof(mMetaTrkInfo));

    mLastReadPts=0;
    mbHasMetaTrack = 0;
}

HiffmpegDemux::~HiffmpegDemux()
{

}

/* set resolution directly if already known, instead get by decode*/
int HiffmpegDemux::getMetaResolution(enum AVCodecID codec_id, AVPacket* pPkt, int* pWidth, int* pHeight)
{
    AVCodecContext * pAvCtx = NULL;
    int ret = 0;
    AVFrame* pframe = NULL;
    AVPacket pkt_ref = { 0 }; // data and size must be 0;

    AVCodec* pCodec =  avcodec_find_decoder(codec_id);
    pAvCtx = avcodec_alloc_context3(pCodec);
    if (!pAvCtx)
    {
        av_log(NULL, AV_LOG_ERROR, "avcodec_alloc_context3 failed\n");
        ret = -1;
        goto Failed;
    }

   ret =  avcodec_open2(pAvCtx, pCodec, NULL);
   if (ret < 0)
   {
       av_log(NULL, AV_LOG_ERROR, "avcodec_open2 failed\n");
       goto FreeCtx;
   }

   av_init_packet(&pkt_ref);
   pkt_ref.data = pPkt->data;
   pkt_ref.size = pPkt->size;
   pkt_ref.pts = pPkt->pts;
   pkt_ref.dts = pPkt->dts;

   ret = avcodec_send_packet(pAvCtx, &pkt_ref);
   if (ret < 0)
   {
       av_log(NULL, AV_LOG_ERROR, "avcodec_open2 failed\n");
       goto UnrefPkt;
   }

   pframe = av_frame_alloc();
    if (!pframe)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate video frame\n");
        ret = -1;
        goto UnrefPkt;
    }

    ret = avcodec_receive_frame(pAvCtx, pframe);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "avcodec_receive_frame failed\n");
        goto FreeCtx;
    }

    *pWidth = pframe->width;
    *pHeight = pframe->height;
    printf("after decode width: %d  height: %d extradatasize: %d\n",  pframe->width, pframe->height, pAvCtx->extradata_size);

    av_frame_unref(pframe);

    av_frame_free(&pframe);
UnrefPkt:
    av_packet_unref(&pkt_ref);
FreeCtx:
    avcodec_free_context(&pAvCtx);
Failed:
    return ret;
}




int HiffmpegDemux::parseMetaSPS(enum AVCodecID codecId, uint8_t* pStrmPtr, int dataLen)
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

int HiffmpegDemux::getMetaCodecpar(AVFormatContext *pInCtx, enum AVCodecID* pRawCodecId, int* pwidth , int* pheight)
{
    int ret = 0;
    AVPacket pkt = { 0 }; // data and size must be 0;
    av_init_packet(&pkt);
    int64_t timestamp = 0;
    int stream_index = 0;
    AVInputFormat* pInputFmt = NULL;
    AVIOContext* pIOCtx = NULL;

    while(1)
    {
        if ((ret = av_read_frame(pInCtx, &pkt)) < 0)
        {
            printf("av_read_frame out\n");
            break;
        }

        if(pkt.stream_index == DEFAULT_META_STREAM_IDX)
        {
            av_log(NULL, AV_LOG_ERROR, "get of meta stream_index %d pts: %lld len: %d\n",
                    pkt.stream_index, pkt.pts, pkt.size);
            break;
        }
        else
            av_packet_unref(&pkt);
    }

    /*attention about avio_alloc_context  usage of buffer ,
    may be freed in av_probe_input_buffer*/
    void* pReadBuffer = av_malloc(pkt.size);
    if(!pReadBuffer)
    {
        av_log(NULL, AV_LOG_ERROR, "av_malloc no mem\n");
        ret = -1;
        goto FreePkt;
    }

    memcpy(pReadBuffer, pkt.data, pkt.size);

    pIOCtx = avio_alloc_context((unsigned char*)pReadBuffer, pkt.size,
                                  0, NULL, NULL, NULL, NULL);
    if (!pIOCtx)
    {
        av_log(NULL, AV_LOG_ERROR, "avio_alloc_context no mem\n");
        ret = -1;
        goto FreeBuf;
    }

    ret = av_probe_input_buffer(pIOCtx, &pInputFmt, NULL,  NULL, 0, pkt.size);
    if(ret < 0 || !pInputFmt)
    {
        av_log(NULL, AV_LOG_ERROR, "av_probe_input_buffer failed\n");
        ret = -1;
        goto FreeBuf;
    }

    if(pInputFmt->raw_codec_id != AV_CODEC_ID_H264
        && pInputFmt->raw_codec_id != AV_CODEC_ID_HEVC)
    {
        av_log(NULL, AV_LOG_ERROR, "err detect raw codec id : %d\n", pInputFmt->raw_codec_id);
        ret = -1;
        goto FreeBuf;
    }

    *pRawCodecId = (enum AVCodecID)pInputFmt->raw_codec_id;

    ret = parseMetaSPS((enum AVCodecID)pInputFmt->raw_codec_id, pkt.data, pkt.size);
    if(ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "parseMetaSPS failed\n");
        goto FreeBuf;
    }

    ret = getMetaResolution((enum AVCodecID)pInputFmt->raw_codec_id, &pkt, pwidth, pheight);
    if(ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "getMetaResolution failed\n");
        goto FreeBuf;
    }

    timestamp = pkt.pts;
    stream_index = pkt.stream_index;

    av_packet_unref(&pkt);

    av_seek_frame(pInCtx, stream_index, timestamp, AVSEEK_FLAG_BACKWARD);

    if(pIOCtx->buffer && pReadBuffer!= pIOCtx->buffer)
        printf("avio buffer ptr changed\n");

    if(pIOCtx->buffer)
        av_free(pIOCtx->buffer);

    avio_context_free(&pIOCtx);

    return 0;

FreeBuf:
    if(pIOCtx->buffer)
        av_free(pIOCtx->buffer);
FreeIO:
    avio_context_free(&pIOCtx);
FreePkt:
    av_packet_unref(&pkt);

    return -1;
}

void HiffmpegDemux::probeMetaTrack(AVFormatContext* pFmtCtx)
{
    if(pFmtCtx->nb_streams < MAX_STREAM_NUM
        || pFmtCtx->streams[DEFAULT_META_STREAM_IDX]->codecpar->codec_type != AVMEDIA_TYPE_DATA)
    {
        MMLOGE(TAG, "mp4 file stream num: %d, may not have meta  track\n");
        return;
    }

    int ret = getMetaCodecpar(pFmtCtx, &mMetaTrkInfo.codecId, &mMetaTrkInfo.width, &mMetaTrkInfo.height);
    if(ret != 0)
    {
        return;
    }
    mbHasMetaTrack = 1;
}

void HiffmpegDemux::discardStream()
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

int HiffmpegDemux::open(char* pUrl)
{
    int ret = 0, got_frame;

    /* register all formats and codecs */
    av_register_all();
    if(strlen(pUrl) == 0)
    {
        MMLOGE(TAG, "ffmpeg demux  url len is 0 \n");
        goto failed;
    }
    if(avformat_network_init() < 0)
    {
        MMLOGE(TAG, "avformat_network_init failed\n");
        goto failed;
    }

    /* open input file, and allocate format context */
    if (avformat_open_input(&mpFmtCtx, pUrl, NULL, NULL) < 0)
    {
        MMLOGE(TAG, "Could not open source file %s\n", pUrl);
        goto net_deinit;
    }
    MMLOGI(TAG, "input format: %s\n", mpFmtCtx->iformat->name);

    /* retrieve stream information */
    if (avformat_find_stream_info(mpFmtCtx, NULL) < 0)
    {
        MMLOGE(TAG, "Could not find stream information\n");
        avformat_network_deinit();
        avformat_close_input(&mpFmtCtx);
        goto input_close;
    }

    //av_log_set_level(AV_LOG_TRACE);

    av_log_set_callback(ffmpegNotify);

    probeMetaTrack(mpFmtCtx);

    if(mbHasMetaTrack)
    {
        mVidCodecId = mMetaTrkInfo.codecId;
        mVideoStreamIdx = DEFAULT_META_STREAM_IDX;
        mVideoStream = mpFmtCtx->streams[DEFAULT_META_STREAM_IDX];
        mbHaveVideo = 1;
        mpFmtCtx->streams[0]->discard = AVDISCARD_ALL;
    }
    else
    {
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

            /*parse sps,pps,vps(only for hevc)*/
            MMLOGI(TAG, "extra datasize: %d", mVideoStream->codecpar->extradata_size);
            //for(int i = 0;i<mVideoStream->codecpar->extradata_size;i++)
            //{
                //MMLOGE(TAG, "0x%02x", mVideoStream->codecpar->extradata[i]);
            //}

            ret = createStreamFilter();
            if(ret != HI_SUCCESS)
            {
                MMLOGE(TAG,"createStreamFilter err \n");
                goto input_close;
            }
            mbHaveVideo = 1;
            MMLOGI(TAG,"video timebase: %d %d \n", mVideoStream->time_base.den, mVideoStream->time_base.num);
        }
    }


    ret = av_find_best_stream(mpFmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (ret < 0)
    {
        MMLOGE(TAG, "Could not find audio stream in input file '%s'\n",pUrl);
        mbHaveAudio = 0;
    }
    else
    {
        mAudioStream = mpFmtCtx->streams[ret];
        mAudioStreamIdx = ret;
        mAudCodecId = mAudioStream->codecpar->codec_id;
        MMLOGI(TAG, "audio codec type  %s\n", avcodec_get_name(mAudCodecId));
        mbHaveAudio = 1;
        MMLOGI(TAG,"audio timebase: %d %d \n", mAudioStream->time_base.den, mAudioStream->time_base.num);
    }

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


int HiffmpegDemux::close()
{
    if(mOpened)
    {
        avformat_close_input(&mpFmtCtx);
        if(mH264StreamFilter)
        {
            HI_ffmpeg_AVBSF_Deinit(&mH264StreamFilter);
            mH264StreamFilter = NULL;
        }
        avformat_network_deinit();
        mOpened = 0;
    }
    return 0;
}

int HiffmpegDemux::getSPSPPS(void* sps, int *spsSize, void* pps, int* ppsSize)
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

        if(!mbHasMetaTrack)
        {
            if(HI_ffmpeg_decode_H264_extraData(mH264StreamFilter->par_out->extradata,
            mH264StreamFilter->par_out->extradata_size,
            &pSPS, &pPPS) < 0)
            {
                MMLOGE(TAG,"HI_ffmpeg_decode_H264_extraData err\n");
                return -1;
            }

            spslen = pPPS - pSPS;
            ppslen = mVideoStream->codecpar->extradata_size - spslen;
        }
        else
        {
            pSPS = mMetaTrkInfo.stAvcParaInfo.sps;
            pPPS = mMetaTrkInfo.stAvcParaInfo.pps;
            spslen = mMetaTrkInfo.stAvcParaInfo.spsLen;
            ppslen = mMetaTrkInfo.stAvcParaInfo.ppsLen;
        }

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
        if(!mbHasMetaTrack)
        {
            if(HI_ffmpeg_decode_Hevc_extraData(mVideoStream->codecpar->extradata,
            mVideoStream->codecpar->extradata_size,
            &mHevcParaInfo) < 0)
            {
                MMLOGE(TAG,"HI_ffmpeg_decode_Hevc_extraData err\n");
                return -1;
            }
            if(mHevcParaInfo.spsLen > *spsSize || mHevcParaInfo.ppsLen > *ppsSize)
            {
                MMLOGE(TAG, "sps or pps buffer size is too small\n");
                return -1;
            }
            memcpy(sps, mHevcParaInfo.sps+4, mHevcParaInfo.spsLen-4);
            *spsSize = mHevcParaInfo.spsLen-4;
            memcpy(pps, mHevcParaInfo.pps+4, mHevcParaInfo.ppsLen-4);
            *ppsSize = mHevcParaInfo.ppsLen-4;
        }
        else
        {
            *spsSize = 0;
            *ppsSize = 0;
        }
    }

     return 0;
}

int HiffmpegDemux::getMediaMask(int& bHaveAudio, int& bHaveVideo)
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

int HiffmpegDemux::getVideoWidth(int& width)
{
    if(!mOpened || !mbHaveVideo)
    {
        MMLOGE(TAG,"ffmpeg demux have not been open  or have no video stream\n");
        return -1;
    }
    width = (mbHasMetaTrack) ? mMetaTrkInfo.width : mVideoStream->codecpar->width;
    return 0;
}

int HiffmpegDemux::getVideoHeight(int& height)
{
    if(!mOpened || !mbHaveVideo)
    {
        MMLOGE(TAG,"ffmpeg demux have not been open  or have no video stream\n");
        return -1;
    }
    height = (mbHasMetaTrack) ? mMetaTrkInfo.height : mVideoStream->codecpar->height;
    return 0;
}

int HiffmpegDemux::getVideoType(enum AVCodecID& vidCodecId)
{
    if(!mOpened || !mbHaveVideo)
    {
        MMLOGE(TAG,"ffmpeg demux have not been open  or have no video stream\n");
        return -1;
    }
    vidCodecId = mVidCodecId;
    return 0;
}

int HiffmpegDemux::getDuration(int& mSec)
{
    if(mpFmtCtx->duration < 0)
    {
        MMLOGE(TAG,"stream duration is negative: %lld\n", mpFmtCtx->duration);
        return -1;
    }
    mSec = (mpFmtCtx->duration + 500) / 1000;
    return 0;
}

void HiffmpegDemux::seekTo(int mSec)
{
    int64_t timeUs = ((int64_t)mSec*1000);
    if (mpFmtCtx->start_time != AV_NOPTS_VALUE)
        timeUs += mpFmtCtx->start_time;

    mLastReadPts = timeUs;
}

int HiffmpegDemux::seekStream(int mSec)
{
    int ret = 0;
    int64_t timeUs = ((int64_t)mSec*1000);

    if(timeUs > mpFmtCtx->duration || timeUs < 0)
    {
        MMLOGE(TAG,"seek input time err: %lld\n", timeUs);
        return -1;
    }

    int ns, hh, mm, ss;
    int tns, thh, tmm, tss;
    tns  = mpFmtCtx->duration / 1000000LL;
    thh  = tns / 3600;
    tmm  = (tns % 3600) / 60;
    tss  = (tns % 60);

    ns   = timeUs/ 1000000LL;
    hh   = ns / 3600;
    mm   = (ns % 3600) / 60;
    ss   = (ns % 60);

    MMLOGI(TAG,
           "Seek to %lld (%2d:%02d:%02d) of total duration %lld (%2d:%02d:%02d)    starttime: %lld   \n",
            timeUs, hh, mm, ss,
            mpFmtCtx->duration, thh, tmm, tss, mpFmtCtx->start_time);

    if (mpFmtCtx->start_time != AV_NOPTS_VALUE)
        timeUs += mpFmtCtx->start_time;

    int64_t seek_target = timeUs;

    ret = avformat_seek_file(mpFmtCtx, -1,  INT64_MIN, seek_target, seek_target, 0);
    if (ret < 0)
    {
        ret = avformat_seek_file(mpFmtCtx, -1,  seek_target, seek_target, INT64_MAX, 0);
        if (ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR,
                   "%s: error while seeking\n", mpFmtCtx->filename);
            return -1;
        }
    }

    //pkt.pts is time base on stream->timebase, need expressed in AV_TIME_BASE units
    mLastReadPts = seek_target;
    return 0;
}

int HiffmpegDemux::getAudioType(enum AVCodecID& audCodecId, int& chnNum, int& sampleRate)
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

int HiffmpegDemux::getCurPostion(int& mPos)
{
    //return ms
    if(mLastReadPts > mpFmtCtx->duration)
    {
        MMLOGE(TAG,"cur position value err\n");
        return -1;
    }
    mPos = mLastReadPts/1000;
    return 0;
}

int HiffmpegDemux::readDataFFmpeg(AVPacket& readPkt, int& bIsVideo, int& payload)
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

        if(!mbHasMetaTrack)
        {
            if(mVidCodecId == AV_CODEC_ID_H264)
            {
                AVPacket dstPkt;
                av_init_packet(&dstPkt);
                dstPkt.data = NULL;
                dstPkt.size = 0;
                int ret = 0;

                (pkt.flags & AV_PKT_FLAG_KEY)? (payload = AVC_NAL_IDR) : (payload = AVC_NAL_NORMAL_P);

                ret = HI_ffmpeg_AVBSF_Filter(mH264StreamFilter, &pkt, &dstPkt);
                if(ret == HI_RET_FILE_EOF)
                {
                    av_packet_unref(&pkt);
                    return ret;
                }
                else if(ret < 0)
                {
                    MMLOGE(TAG,"H264 av_bitstream_filter_filter err ret: %d\n", ret);
                    HI_ffmpeg_print_errToStr(ret);
                    av_packet_unref(&pkt);
                    return -1;
                }
                memcpy(pWr, dstPkt.data, dstPkt.size);
                WrSize = dstPkt.size;
                av_packet_move_ref(&pkt, &dstPkt);
            }
            else if(mVidCodecId == AV_CODEC_ID_HEVC)
            {
                payload = 1;
                int outLen = readPkt.size;
                HI_ffmpeg_Hevc_mp4toannexb_filter(pWr, &outLen, pkt.data, pkt.size, &mHevcParaInfo);
                //MMLOGD(TAG,"HI_ffmpeg_Hevc_mp4toannexb_filter inputlen: %d outLen: %d\n", pkt.size, outLen);
                if(pkt.size != outLen)
                    MMLOGD(TAG,"convert hevc mp4 to annexb diff inputlen: %d outLen: %d\n", pkt.size, outLen);
                WrSize += outLen;
                if(pkt.flags & AV_PKT_FLAG_KEY)
                {
                    payload = 19;
                }
            }
        }
        else
        {
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



int HiffmpegDemux::readFrame(AVPacket& readPkt, int& bIsVideo, int& payload)
{
    int ret = 0;
    while((ret = readDataFFmpeg(readPkt, bIsVideo, payload)) == HI_RET_OTHER_STREAM)
    {
        continue;
    }
    return ret;
}

int HiffmpegDemux::getMediaInfo(ProtoMediaInfo& mediaInfo)
{
    if(!mOpened)
    {
        MMLOGE(TAG, "getMediaInfo demux not opened");
        return HI_FAILURE;
    }

    mediaInfo.u8TrackNum = mpFmtCtx->nb_streams;
    int i = 0;

    for(; i<mediaInfo.u8TrackNum; i++)
    {
        if(mpFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            mediaInfo.stTrackInfo[i].enTrackType = TRACK_TYPE_VIDEO_E;

            if(mpFmtCtx->streams[i]->codecpar->codec_id == AV_CODEC_ID_H264)
            {
                mediaInfo.stTrackInfo[i].attr.stVideoAttr.type = Protocol_Video_H264;
            }
            else if(mpFmtCtx->streams[i]->codecpar->codec_id == AV_CODEC_ID_HEVC)
            {
                mediaInfo.stTrackInfo[i].attr.stVideoAttr.type = Protocol_Video_H265;
            }
            else
            {
                mediaInfo.stTrackInfo[i].attr.stVideoAttr.type = Protocol_Video_BUTT;
            }
            mediaInfo.stTrackInfo[i].attr.stVideoAttr.width = mpFmtCtx->streams[i]->codecpar->width;
            mediaInfo.stTrackInfo[i].attr.stVideoAttr.height = mpFmtCtx->streams[i]->codecpar->height;
        }
        else if(mpFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            mediaInfo.stTrackInfo[i].enTrackType = TRACK_TYPE_AUDIO_E;
            if(mpFmtCtx->streams[i]->codecpar->codec_id == AV_CODEC_ID_AAC)
            {
                mediaInfo.stTrackInfo[i].attr.stAudioAttr.type = Protocol_Audio_AAC;
            }
            else
            {
                mediaInfo.stTrackInfo[i].attr.stAudioAttr.type = Protocol_Audio_BUTT;
            }
            mediaInfo.stTrackInfo[i].attr.stAudioAttr.sampleRate = mpFmtCtx->streams[i]->codecpar->sample_rate;
            mediaInfo.stTrackInfo[i].attr.stAudioAttr.bitwidth = mpFmtCtx->streams[i]->codecpar->bits_per_coded_sample;
            mediaInfo.stTrackInfo[i].attr.stAudioAttr.chnNum = mpFmtCtx->streams[i]->codecpar->channels;
            mediaInfo.stTrackInfo[i].attr.stAudioAttr.u32BitRate = mpFmtCtx->streams[i]->codecpar->bit_rate;
        }
        else if(mpFmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_DATA)
        {
            mediaInfo.stTrackInfo[i].enTrackType = TRACK_TYPE_DATA_E;
        }
        else
        {
            mediaInfo.stTrackInfo[i].enTrackType = TRACK_TYPE_UNKOWN_E;
        }
    }

    return HI_SUCCESS;
}

void HiffmpegDemux::destroyStreamFilter()
{
    if(mH264StreamFilter)
    {
        HI_ffmpeg_AVBSF_Deinit(&mH264StreamFilter);
        mH264StreamFilter = NULL;
    }
    memset(&mHevcParaInfo, 0x00, sizeof(HiHevcParaSetInfo));
}

int HiffmpegDemux::createStreamFilter()
{
    int ret = HI_SUCCESS;

    if(mVidCodecId == AV_CODEC_ID_H264)
    {
        ret = HI_ffmpeg_AVBSF_Init(&mH264StreamFilter, mVideoStream, "h264_mp4toannexb");
        if(ret != HI_SUCCESS)
        {
            MMLOGE(TAG,"HI_ffmpeg_decode_Hevc_extraData err \n");
            return HI_FAILURE;
        }
        MMLOGI(TAG, "HI_ffmpeg_AVBSF_Init success");
    }
    else if(mVidCodecId == AV_CODEC_ID_HEVC)
    {
        memset(&mHevcParaInfo, 0x00, sizeof(HiHevcParaSetInfo));
        ret = HI_ffmpeg_decode_Hevc_extraData(mVideoStream->codecpar->extradata,
            mVideoStream->codecpar->extradata_size,
            &mHevcParaInfo);
        if(ret < 0)
        {
            MMLOGE(TAG,"HI_ffmpeg_decode_Hevc_extraData err \n");
            return HI_FAILURE;
        }
    }
    return HI_SUCCESS;
}

int HiffmpegDemux::selectStreamIndex(unsigned char index)
{
    int i = 0;

    MMLOGI(TAG, "selectStreamIndex index:%d ", index);
    if(!mOpened)
    {
        MMLOGE(TAG, "selectStreamIndex demux not opened");
        return HI_FAILURE;
    }

    if(index >= mpFmtCtx->nb_streams)
    {
        MMLOGE(TAG, "selectStreamIndex input index:%d over max:%d", index,  mpFmtCtx->nb_streams);
        return HI_FAILURE;
    }

    if(mVideoStreamIdx == index || mAudioStreamIdx == index)
    {
        MMLOGI(TAG, "index:%d same with current set", index);
        return HI_SUCCESS;
    }

    for(i=0;i<mpFmtCtx->nb_streams;i++)
    {
        mpFmtCtx->streams[i]->discard = AVDISCARD_DEFAULT;
    }

    if(mpFmtCtx->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        mVideoStream = mpFmtCtx->streams[index];
        mVideoStreamIdx = index;
        mVidCodecId = mVideoStream->codecpar->codec_id;

        destroyStreamFilter();
        if(createStreamFilter() != HI_SUCCESS)
        {
            MMLOGE(TAG,"createStreamFilter failed \n");
            return HI_FAILURE;
        }
        MMLOGI(TAG, "video codec type  %s width:%d height:%d\n", avcodec_get_name(mVidCodecId),
            mVideoStream->codecpar->width, mVideoStream->codecpar->height);
        MMLOGI(TAG,"video timebase: %d %d \n", mVideoStream->time_base.den, mVideoStream->time_base.num);
    }
    else if(mpFmtCtx->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
        mAudioStream = mpFmtCtx->streams[index];
        mAudioStreamIdx = index;
        mAudCodecId = mAudioStream->codecpar->codec_id;
        MMLOGI(TAG, "audio codec type  %s\n", avcodec_get_name(mAudCodecId));
        MMLOGI(TAG,"audio timebase: %d %d \n", mAudioStream->time_base.den, mAudioStream->time_base.num);
    }
    else
    {
        MMLOGE(TAG, "selectStreamIndex input index:%d not video or audio", index);
        return HI_FAILURE;
    }
    avformat_flush(mpFmtCtx);

    av_seek_frame(mpFmtCtx, -1, 0, 0);

    discardStream();
    return HI_SUCCESS;
}
