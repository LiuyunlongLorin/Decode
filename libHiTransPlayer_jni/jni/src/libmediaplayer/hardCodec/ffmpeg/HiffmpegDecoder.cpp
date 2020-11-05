#include <stdlib.h>
#include <stdarg.h>
#include "HiffmpegDecoder.h"
#include "HiMLoger.h"

#define TAG "HiffmpegDecoder"


//namespace android{
HiffmpegDecoder::HiffmpegDecoder()
{
    /* register all the codecs */
    avcodec_register_all();
    mVdoCodecCtx = NULL;
    mAudCodecCtx = NULL;
    mFirstAudioFlag = 0;
    mFirstVideoFlag = 0;
    mAudDecodeFrame = NULL;
    pAudResampleHandle = NULL;
    pAudCvtOutPtr = NULL;
    mAudDecodeFrame = NULL;
    mAudCodecID=AV_CODEC_ID_NONE;
}

HiffmpegDecoder::~HiffmpegDecoder()
{

}

int HiffmpegDecoder::openVideoDecoder(enum AVCodecID codecID, int bMultiThread)
{
    struct AVCodec* i_pcodec;
    int ret = 0;

    i_pcodec = avcodec_find_decoder(codecID);
    if (!i_pcodec) {
        MMLOGE(TAG, "avcodec_find_decoder error \n");
        goto err_ret;
    }

    if(mVdoCodecCtx)
    {
        MMLOGE(TAG,  "HiffmpegVideoDecoder: %d already opened \n", codecID);
        goto err_ret;
    }

    mVdoCodecCtx = avcodec_alloc_context3(i_pcodec);
    if(!mVdoCodecCtx)
    {
        MMLOGE(TAG, "malloc error \n");
        goto err_ret;
    }
    /*设置ffmpeg callback函数
        set the callback of ffmpeg to display the log of ff*/
    av_log_set_callback(ffmpegNotify);
    MMLOGI(TAG, "ffmpeg Decoder support multiThread: %d \n", bMultiThread);

    if(!bMultiThread && i_pcodec->capabilities&CODEC_CAP_TRUNCATED)
        mVdoCodecCtx->flags|= CODEC_FLAG_TRUNCATED; /* we do not send complete frames */
    if(bMultiThread)
    {
        mVdoCodecCtx->thread_count = 4;
        mVdoCodecCtx->active_thread_type &= FF_THREAD_FRAME;

        mVdoCodecCtx->flags &= (~CODEC_FLAG_TRUNCATED);
        mVdoCodecCtx->flags &= (~CODEC_FLAG_LOW_DELAY);
        mVdoCodecCtx->flags2 &= (~CODEC_FLAG2_CHUNKS);
    }
    else
        mVdoCodecCtx->flags &= CODEC_FLAG_LOW_DELAY;
    /* open it */
    if (avcodec_open2(mVdoCodecCtx, i_pcodec, NULL) < 0) {
        MMLOGE(TAG, "avcodec_open error \n");
        goto open_fail;
    }

    MMLOGI(TAG, "h264 codec init pthread num : %d \n",mVdoCodecCtx->thread_count);
    return 0;

open_fail:
    av_free(mVdoCodecCtx);
    mVdoCodecCtx = NULL;
err_ret:
    return -1;
}

int HiffmpegDecoder::closeVideoDecoder()
{
    if(mVdoCodecCtx)
    {
        avcodec_close(mVdoCodecCtx);
        MMLOGE(TAG, "avcodec_close  video  ok \n");
        av_free(mVdoCodecCtx);
        mVdoCodecCtx = NULL;
    }
    MMLOGI(TAG, "HiffmpegDecoder close video ctx ok \n");
    mFirstVideoFlag = 0;
    return 0;
}

int HiffmpegDecoder::openAudioDecoder(const stProtoAudAttr& audAttr)
{
    struct AVCodec* i_pcodec = NULL;
    int ret = 0;
    enum AVCodecID codecID;

    ret = ConvertProtoCodecTypeToAVCodecID(audAttr.type, &codecID);
    if(ret < 0)
    {
        MMLOGE(TAG, "audio type %d could not supported\n", audAttr.type);
        goto err_ret;
    }
    MMLOGE(TAG, "audio code type: %s channel: %d bandwidth: %d sampleRate: %d bitRate: %d\n",
        avcodec_get_name(codecID), audAttr.chnNum, audAttr.bitwidth, audAttr.sampleRate, audAttr.u32BitRate);

    i_pcodec = avcodec_find_decoder(codecID);
    if(!i_pcodec)
    {
        MMLOGE(TAG,  "could not find respond codec :%d \n", codecID);
        goto err_ret;
    }

    if(mAudCodecCtx)
    {
        MMLOGE(TAG,  "HiffmpegAudioDecoder: %d already opened \n", codecID);
        goto err_ret;
    }

    /*音频解码器Context*/
    /*audio decoder context*/
    mAudCodecCtx = avcodec_alloc_context3(i_pcodec);
    if(!mAudCodecCtx)
    {
        MMLOGE(TAG, "malloc error \n");
        goto err_ret;
    }
    mAudCodecCtx->channels = audAttr.chnNum;
    mAudCodecCtx->sample_rate = audAttr.sampleRate;
    /*G726 format bit width per sample is 4 currently
    ,pls change it according bvt cam server parameter*/
    if(codecID == AV_CODEC_ID_ADPCM_G726LE)
    {
        mAudCodecCtx->bits_per_coded_sample = 4;
        //mAudCodecCtx->sample_rate = 8000;
    }
    else if(codecID == AV_CODEC_ID_PCM_ALAW)
    {
        mAudCodecCtx->bits_per_coded_sample = 8;
        mAudCodecCtx->sample_rate = 8000;
    }
    if (avcodec_open2(mAudCodecCtx, i_pcodec, NULL) < 0)
    {
        MMLOGE(TAG, "avcodec_open error \n");
        goto open_fail;
    }
    mAudDecodeFrame = av_frame_alloc();
    if (!mAudDecodeFrame)
    {
        MMLOGI(TAG, "av_frame_alloc failed");
        goto alloc_fail;
    }
    mAudCodecID = codecID;
    return 0;

alloc_fail:
    av_free(mAudDecodeFrame);
    mAudDecodeFrame = NULL;
open_fail:
    av_free(mAudCodecCtx);
    mAudCodecCtx = NULL;
err_ret:
    return -1;
}

int HiffmpegDecoder::closeAudioDecoder()
{
    if(mAudCodecCtx)
    {
        avcodec_close(mAudCodecCtx);
        MMLOGE(TAG, "avcodec_close  audio  ok \n");
        av_free(mAudCodecCtx);
        mAudCodecCtx = NULL;
        if(pAudResampleHandle)
            audio_resample_deinit(pAudResampleHandle);
        pAudResampleHandle = NULL;
    }
    mFirstAudioFlag = 0;
    return 0;
}

/*循环获取音频数据，并送入decoder解码，获得PCM数据，回调java AudioTrack 播放*/
/*get the audio data , the input to decoder and get pcm output, then input pcm to audio track*/
int HiffmpegDecoder::decodeAudio(uint8_t *outdata, unsigned int *outdata_size, int* pGotData, AVPacket *avpkt)
{
    int ret = 0;
    int bGotFrame = 0;
    if(!mAudCodecCtx)
    {
        MMLOGI(TAG, "audio codec have not been opened\n");
        return -1;
    }
    ret = avcodec_decode_audio4(mAudCodecCtx, mAudDecodeFrame, &bGotFrame, avpkt);
    if(ret < 0)
    {
        MMLOGE(TAG, "decodeAudio failed  \n");
        return -1;
    }

    if(ret != avpkt->size)
        MMLOGI(TAG, "error: do not consume once dataLen: %d consume:%d \n",  avpkt->size, ret);

    if(!bGotFrame)
    {
        *pGotData = 0;
        return 0;
    }

    if(!mFirstAudioFlag)
    {
        if(mAudCodecID == AV_CODEC_ID_AAC)
        {
            if(mAudDecodeFrame->format != AV_SAMPLE_FMT_FLTP)
            {
                MMLOGE(TAG, " formate %d are not supported", mAudDecodeFrame->format);
                return -1;
            }
            pAudResampleHandle = audio_resample_init(mAudDecodeFrame);
            if(!pAudResampleHandle)
            {
                MMLOGE(TAG, "audio_resample_init failed return NULL  \n");
                return -1;
            }
            MMLOGE(TAG, " audio_resample_init OK");
            pAudCvtOutPtr = (uint8_t*)av_malloc(MAX_AUDIO_FRAME_SIZE);
            if(!pAudCvtOutPtr)
            {
                MMLOGE(TAG, "audio malloc outbuffer failed");
                return -1;
            }
        }
        else
        {
            if(mAudDecodeFrame->format != AV_SAMPLE_FMT_S16)
            {
                MMLOGE(TAG, " formate %d are not supported", mAudDecodeFrame->format);
                return -1;
            }
        }
        mFirstAudioFlag = true;
    }

    uint8_t* pPcmBufPtr = NULL;
    int pcmSampleNb = 0;
    if(mAudCodecID == AV_CODEC_ID_AAC)
    {
        /*convert from AV_SAMPLE_FMT_FLTP to AV_SAMPLE_FMT_S16*/
        pcmSampleNb = audio_resample_convert(pAudResampleHandle, mAudDecodeFrame, pAudCvtOutPtr);
        pPcmBufPtr = pAudCvtOutPtr;
    }
    else
    {
        pcmSampleNb = mAudDecodeFrame->nb_samples;
        pPcmBufPtr = mAudDecodeFrame->data[0];
    }
    int data_size = av_samples_get_buffer_size(NULL, mAudDecodeFrame->channels,
                                               pcmSampleNb,
                                               AV_SAMPLE_FMT_S16, 1);
    if(data_size > *outdata_size)
    {
        MMLOGE(TAG, "decode aud input buffer size is too small :%d need: %d  \n", *outdata_size, data_size);
        return -1;
    }
    //MMLOGE(TAG, " sampleNb %d dataSize: %d ", pcmSampleNb, data_size);
    memcpy(outdata, pPcmBufPtr, data_size);
    *outdata_size = data_size;
    *pGotData = 1;

    return 0;
}

void HiffmpegDecoder::videoFlush()
{
    MMLOGE(TAG, "HiffmpegDecoder videoFlush  \n");
    if(mVdoCodecCtx)
        avcodec_flush_buffers(mVdoCodecCtx);
}

void HiffmpegDecoder::audioFlush()
{
    MMLOGE(TAG, "HiffmpegDecoder audioFlush  \n");
    if(mAudCodecCtx)
        avcodec_flush_buffers(mAudCodecCtx);
}

/*循环获取视频数据，并送入decoder解码，获得YUV数据，转换为
    RGB, 送入NativeWindow 更新显示*/
/*get the video frame ,  input to decoder and get YUV output, convert YUV to RGB ,then input RGB to NativeWindow for display update*/
int HiffmpegDecoder::decodeVideo(AVFrame *outdata, int *outdata_size, AVPacket *avpkt)
{
    int ret = 0;
    if(!mVdoCodecCtx)
    {
        MMLOGI(TAG, "video codec have not been opened\n");
        return -1;
    }
    ret = avcodec_decode_video2(mVdoCodecCtx, outdata, outdata_size, avpkt);
    if(ret < 0)
    {
        MMLOGE(TAG, "decode failed  \n");
        return -1;
    }
    if(!mFirstVideoFlag)
        mFirstVideoFlag = true;
    return ret;
}

int HiffmpegDecoder::getVideoAttr(int* width, int* height, int* pixFormat)
{
    if(!mVdoCodecCtx)
    {
        MMLOGI(TAG, "video codec have not been opened\n");
        return -1;
    }
    if(mFirstVideoFlag)
    {
        *width = mVdoCodecCtx->width;
        *height = mVdoCodecCtx->height;
        *pixFormat = mVdoCodecCtx->pix_fmt;
        return 0;
    }
    else
    {
        MMLOGI(TAG, "there no video frame are decoded\n");
        return -1;
    }
}

int HiffmpegDecoder::getAudioAttr(int* chnlCnt, int* sampleRate, int* sample_fmt)
{
    if(!mAudCodecCtx)
    {
        MMLOGI(TAG, "audio codec have not been opened\n");
        return -1;
    }
    if(mFirstAudioFlag)
    {
        *chnlCnt = mAudCodecCtx->channels;
        *sampleRate = mAudCodecCtx->sample_rate;
        *sample_fmt = mAudCodecCtx->sample_fmt;
        return 0;
    }
    else
    {
        MMLOGI(TAG, "there no audio frame are decoded\n");
        return -1;
    }
}

void HiffmpegDecoder::ffmpegNotify(void* ptr, int level, const char* fmt, va_list vl)
{
    char tmpBuffer[1024];
//    __android_log_print(ANDROID_LOG_ERROR, TAG, "AV_LOG_ERROR: %s", tmpBuffer);
    vsnprintf(tmpBuffer,1024,fmt,vl);

    switch(level) {
            /**
             * Something went really wrong and we will crash now.
             */
        case AV_LOG_PANIC:
            MMLOGE(TAG, "AV_LOG_PANIC: %s", tmpBuffer);
            //sPlayer->mCurrentState = MEDIA_PLAYER_STATE_ERROR;
            break;

        case AV_LOG_FATAL:
            MMLOGE(TAG, "AV_LOG_FATAL: %s", tmpBuffer);
            //sPlayer->mCurrentState = MEDIA_PLAYER_STATE_ERROR;
            break;

        case AV_LOG_ERROR:
            MMLOGE(TAG, "AV_LOG_ERROR: %s", tmpBuffer);
            //sPlayer->mCurrentState = MEDIA_PLAYER_STATE_ERROR;
            break;

        case AV_LOG_WARNING:
            MMLOGI(TAG, "AV_LOG_WARNING: %s", tmpBuffer);
            break;

        case AV_LOG_INFO:
            //MMLOGI(TAG, "%s", tmpBuffer);
            break;

        case AV_LOG_DEBUG:
            //MMLOGI(TAG, "%s", tmpBuffer);
            break;

    }
}
//}
