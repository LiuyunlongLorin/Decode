#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "HiMediaCodecHandlr.h"
#include "HiMLoger.h"
#include "comm_utils.h"
#include "Hies_proto.h"

#include "HiffmpegHandlr.h"

#include <jni.h>
#include <android/log.h>

#include "media/NdkMediaExtractor.h"

#include <android/native_window_jni.h>

#define TAG "HiMediaCodecHandlr"

//namespace android{

static ANativeWindow* window;

HiMediaCodecHandlr::HiMediaCodecHandlr(jobject jsurface,
    JNIEnv* env,
    int apiVer,
    HiProtocol* protocol,
    MediaPlayerPCMWriter* pcmWriter)
    :HiMediaHandlr(jsurface, apiVer, protocol),
    mPCMWriter(pcmWriter)
{
    window = ANativeWindow_fromSurface(env, jsurface);

    mOpened = 0;
    audConfigFlag = 0;
    mMediaMask = 0;

    mVidThread = 0;
    mAudThread = 0;
    mDecoder = NULL;
    mOpened = 0;
    mRunning = 0;
    mPaused = 0;
    mbFristVidFrame = 1;
    mbFirstInputFrame = 0;
    mediaCodc = NULL;

}

HiMediaCodecHandlr::~HiMediaCodecHandlr()
{
    MMLOGI(TAG, "native window released");
    ANativeWindow_release(window);
}


int HiMediaCodecHandlr::open()
{
    unsigned char sps_set[128] = {0};
    unsigned char pps_set[128] = {0};
    int sps_size = 128;
    int pps_size =128;
    int ret = 0;
    unsigned char prefix[4] = {0x00, 0x00, 0x00, 0x01};
    unsigned char sps_new[128];
    unsigned char pps_new[128];
    AMediaFormat *format = NULL;
    media_status_t mediaRet = AMEDIA_OK;
    eProtoVidType type = Protocol_Video_BUTT;

    if(mOpened)
    {
        MMLOGE(TAG, "HiMediaCodecHandlr already opened \n");
        return 0;
    }
    mProto->getMediaMask(mMediaMask);
    if(mMediaMask & PROTO_AUDIO_MASK)
    {
        mDecoder = new HiffmpegDecoder();
        stProtoAudAttr audAttr;
        memset(&audAttr, 0x00, sizeof(stProtoAudAttr));
        mProto->getAudioType(audAttr);
        if(mDecoder->openAudioDecoder(audAttr) < 0)
        {
            MMLOGE(TAG, "HiffmpegDecoder open error \n");
            goto Failed;
        }
    }

    //video
    format = AMediaFormat_new();
    if(!format)
    {
        MMLOGE(TAG, "AMediaFormat_new ret error \n");
        goto closeDecoder;
    }
    ret=mProto->getVideoType(type);
    if(ret < 0)
    {
        MMLOGE(TAG, "getVideoType failed(%d)", ret);
        goto delFormat;
    }

    if(type==Protocol_Video_H264 )
        AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, FORMAT_VIDEO_AVC);
    if(type==Protocol_Video_H265 )
        AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, FORMAT_VIDEO_HEVC);

    AMediaFormat_setInt64(format, AMEDIAFORMAT_KEY_DURATION, 49920000000);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, 1920);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, 1080);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, MAX_VIDEO_FRAME_SIZE);

    ret = mProto->getSPSPPS(sps_set,&sps_size,pps_set,&pps_size);
    if(ret != 0)
    {
        MMLOGE(TAG, "getSPS PPS failed\n");
        goto delFormat;
    }

    memcpy(sps_new, prefix, 4);
    memcpy(sps_new+4, sps_set, sps_size);
    memcpy(pps_new, prefix, 4);
    memcpy(pps_new+4, pps_set, pps_size);
    sps_size+=4;
    pps_size+=4;
    AMediaFormat_setBuffer(format, "csd-0", sps_new, sps_size);
    AMediaFormat_setBuffer(format, "csd-1", pps_new, pps_size);


    MMLOGI(TAG,"csd-0: %d csd-1: %d", sps_size, pps_size);
    MMLOGI(TAG,"video  format: %s", AMediaFormat_toString(format));


    // Omitting most error handling for clarity.
    // Production code should check for errors.
    // MMLOGE(TAG, "AMediaCodec_createDecoderByType video/hevc\n");
     if(type==Protocol_Video_H264 )
        mediaCodc = AMediaCodec_createDecoderByType(FORMAT_VIDEO_AVC);
     if(type==Protocol_Video_H265)
        mediaCodc = AMediaCodec_createDecoderByType(FORMAT_VIDEO_HEVC);




    if(!mediaCodc)
    {
        MMLOGE(TAG, "AMediaCodec_createDecoderByType failed\n");
        goto delFormat;
    }
    mediaRet = AMediaCodec_configure(mediaCodc, format, window, NULL, 0);
    if(mediaRet != AMEDIA_OK)
    {
        MMLOGE(TAG, "AMediaCodec_configure failed\n");
        goto delCodec;
    }
    mediaRet = AMediaCodec_start(mediaCodc);
    if(mediaRet != AMEDIA_OK)
    {
        MMLOGE(TAG, "AMediaCodec_start failed\n");
        goto delCodec;
    }
    mediaRet = AMediaFormat_delete(format);
    if(mediaRet != AMEDIA_OK)
    {
        MMLOGE(TAG, "AMediaFormat_delete failed\n");
        goto stopCodec;
    }
    mOpened = 1;

    return 0;
stopCodec:
    AMediaCodec_stop(mediaCodc);
delCodec:
    AMediaCodec_delete(mediaCodc);
delFormat:
    AMediaFormat_delete(format);
closeDecoder:
    if(mDecoder)
        mDecoder->closeAudioDecoder();
Failed:
    return -1;
}

int HiMediaCodecHandlr::close()
{
    if(mOpened)
    {
        mOpened = 0;
        stop();
        AMediaCodec_stop(mediaCodc);
        AMediaCodec_delete(mediaCodc);
        if(mMediaMask & PROTO_AUDIO_MASK)
        {
            mDecoder->closeAudioDecoder();
            delete mDecoder;
            mDecoder = NULL;
        }
        MMLOGI(TAG, "HiMediaCodecHandlr close \n");
    }

    return 0;
}

void* HiMediaCodecHandlr::startDecVideo(void* ptr)
{
    HiMediaCodecHandlr* pHandlr = static_cast<HiMediaCodecHandlr*>(ptr);
    pHandlr->decodeMovie(ptr);
}

void* HiMediaCodecHandlr::startRenderVideo(void* ptr)
{
    HiMediaCodecHandlr* pHandlr = static_cast<HiMediaCodecHandlr*>(ptr);
    pHandlr->renderMovie(ptr);
}

void* HiMediaCodecHandlr::startPlayAudio(void* ptr)
{
    HiMediaCodecHandlr* pHandlr = static_cast<HiMediaCodecHandlr*>(ptr);
    pHandlr->decodeAudio(ptr);
}

int HiMediaCodecHandlr::start()
{
    if(!mRunning)
    {
        mRunning = 1;
        mProto->getMediaMask(mMediaMask);
        mAudThread = 0;
        mVidThread = 0;
        if(mMediaMask & PROTO_VIDEO_MASK)
        {
            int ret = pthread_mutex_init(&mVidFlushLock, NULL);
            if(ret < 0)
            {
                MMLOGE(TAG, "pthread_mutex_init mVidFlushLock err: %d ", ret);
                return -1;
            }
            pthread_create(&mVidThread, NULL, startDecVideo, this);
            MMLOGI(TAG, "pthread_create startDecVideo id: %08x ", mVidThread);
            pthread_create(&mRendThread, NULL, startRenderVideo, this);
            MMLOGI(TAG, "pthread_create startRendVideo id: %08x ", mRendThread);
        }
        if(mMediaMask & PROTO_AUDIO_MASK)
        {
            pthread_create(&mAudThread, NULL, startPlayAudio, this);
            MMLOGI(TAG, "pthread_create startAudio id: %08x ", mAudThread);
        }
    }
    else
    {
        if(mPaused)
            mPaused = 0;
    }
    return 0;
}

int HiMediaCodecHandlr::stop()
{
    if(mRunning)
    {
        MMLOGI(TAG,"HiMediaCodecHandlr stop begin");
        mRunning = 0;
        if(pthread_join(mAudThread, 0) < 0)
        {
             MMLOGE(TAG,"pthread_join failed");
        }
        mAudThread = 0;
        if(pthread_join(mVidThread, 0) < 0)
        {
             MMLOGE(TAG,"pthread_join failed");
        }
        mVidThread = 0;
        if(pthread_join(mRendThread, 0) < 0)
        {
             MMLOGE(TAG,"pthread_join failed");
        }
        mRendThread = 0;
        pthread_mutex_unlock(&mVidFlushLock);
        if(pthread_mutex_destroy(&mVidFlushLock) < 0)
        {
            MMLOGE(TAG,"pthread_mutex_destroy failed");
        }
        MMLOGI(TAG,"HiMediaCodecHandlr stop end");
    }
    return 0;
}

void* HiMediaCodecHandlr::decodeAudio(void* ptr)
{
    AVPacket pkt;
    int gotAudio = 0;
    int diff = 0;
    int errHappened = 0;
    unsigned int readLen = 0;
    int ret = 0;

    void* pInBuffer = av_malloc(MAX_AUDIO_FRAME_SIZE);
    if(!pInBuffer)
    {
        MMLOGE(TAG, "malloc pInBuffer error \n");
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
        return 0;
    }
    uint8_t* pOutBuffer = (uint8_t*)av_malloc(MAX_AUDIO_FRAME_SIZE);
    if(!pOutBuffer)
    {
        MMLOGE(TAG, "malloc pOutBuffer error \n");
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
        return 0;
    }
    unsigned int outLen = 0;
    HI_U64 Audbegin = getTimeS();
    HI_U64 AudTime = Audbegin;
    while(mRunning)
    {
        if(mAudNeedSeek)
        {
            mDecoder->audioFlush();
            mAudNeedSeek = 0;
        }
        if(mPaused)
        {
            mLastAudioRealTsUs = getSysTimeUs();
            usleep(50000);
            continue;
        }
        if(getTimeS()-AudTime >=180)
        {
            AudTime = getTimeS();
            diff = getTimeS()-Audbegin;
            MMLOGE(TAG, "display audio last  time: %d (s)!!!!", diff);
        }
        av_init_packet(&pkt);
        readLen = MAX_AUDIO_FRAME_SIZE;
        if((ret = mProto->readAudioStream(pInBuffer, readLen, pkt.pts)) < 0)
        {
            MMLOGE(TAG, "readAudioStream failed");
            errHappened = 1;
            break;
        }
        if(ret == ERROR_PROTO_EXIT)
            break;
        pkt.size = readLen;
        pkt.data = (uint8_t*)pInBuffer;
        pkt.dts = pkt.pts;
        //MMLOGI(TAG, "audio read pkt  pts: %lld, len: %d \n",  pkt.pts, pkt.size);
        outLen = MAX_AUDIO_FRAME_SIZE;
        int len = mDecoder->decodeAudio(pOutBuffer, &outLen, &gotAudio, &pkt);
        if(len < 0)
        {
            MMLOGE(TAG, "HI_HIES_decodeAudio failed");
            errHappened = 1;
            break;
        }
        if(!gotAudio)
        {
            continue;
        }
        if(!audConfigFlag)
        {
            ret = mDecoder->getAudioAttr(&mChnCnt, &mSampleRate, &mSampleFmt);
            if(ret < 0)
            {
                MMLOGE(TAG, "there some error happened, could not get the attr \n");
                errHappened = 1;
                break;
            }
            configAudioRender(mChnCnt, mSampleRate, AV_SAMPLE_FMT_S16);
            audConfigFlag = 1;
        }

        avsyncAudioUpdate(pkt.pts);
        if(mPCMWriter)
        {
            if (outLen > 0)
                mPCMWriter->writePCM(pOutBuffer, outLen);
            //MMLOGI(TAG, "write pcm len: %d pts: %lld\n", outLen, pkt.pts);
        }
    }
    if(pOutBuffer)
        av_free(pOutBuffer);
    if(pInBuffer)
        av_free(pInBuffer);
    if(errHappened)
    {
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
        MMLOGE(TAG, "audio playing err");
    }
    notify(DETACHING, -1, -1);
    MMLOGI(TAG,"HiMediaCodecHandlr decodeAudio exit");
    return 0;
}

void HiMediaCodecHandlr::notify(int msg, int ext1, int ext2)
{
    bool send = true;
    bool locked = false;

    if ((mListener != 0) && send) {
       mListener->notify(msg, ext1, ext2);
    }
}

void* HiMediaCodecHandlr::renderMovie(void* ptr)
{
    int64_t pts = 0;
    int got_pic = 0;
    int diff = 0;
    int errHappened = 0;
    int ptype = 0;
    unsigned int readLen = 0;
    int ret = 0;
    media_status_t media_ret = AMEDIA_OK;

    while(mRunning)
    {
        if(mPaused)
        {
            usleep(50000);
            continue;
        }
        if(!mbFirstInputFrame)
        {
            usleep(50000);
            continue;
        }
        pthread_mutex_lock(&mVidFlushLock);
        AMediaCodecBufferInfo info;
        ssize_t status = AMediaCodec_dequeueOutputBuffer(mediaCodc, &info, 10000);
        if (status >= 0)
        {
            if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM)
            {
                MMLOGE(TAG,"output EOS");
                pthread_mutex_unlock(&mVidFlushLock);
                AMediaCodec_releaseOutputBuffer(mediaCodc, status, false);
                break;
            }
            bool bRender = true;
            if(info.size == 0)
                bRender = false;
#ifdef HI_AV_SYNC
            if(mProto->getProtoType() == PROTO_TYPE_FILE_CLIENT
            || mProto->getProtoType() == PROTO_TYPE_REMOTE_CLIENT)
            {
               if(syncWithAudio(info.presentationTimeUs))
                   bRender = false;
            }
#endif
            //MMLOGI(TAG,"output idx: %d isRend: %d pts: %lld off: %d size: %d", status, bRender, info.presentationTimeUs/1000, info.offset, info.size);
            media_ret = AMediaCodec_releaseOutputBuffer(mediaCodc, status, bRender);
            if(media_ret < 0)
            {
                MMLOGE(TAG, "AMediaCodec_releaseOutputBuffer err %d", media_ret);
                errHappened = 1;
                pthread_mutex_unlock(&mVidFlushLock);
                break;
            }
        }
        else if (status == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
        {
            MMLOGI(TAG,"output buffers changed");
        }
        else if (status == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
        {
            AMediaFormat *format = NULL;
            format = AMediaCodec_getOutputFormat(mediaCodc);
            MMLOGI(TAG,"format changed to: %s", AMediaFormat_toString(format));
            AMediaFormat_delete(format);
        }
        else if (status == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
        {
            //MMLOGI(TAG,"no output buffer right now");
        }
        else
        {
            MMLOGI(TAG,"unexpected info code: %zd", status);
            errHappened = 1;
            pthread_mutex_unlock(&mVidFlushLock);
            break;
        }
        pthread_mutex_unlock(&mVidFlushLock);
        //spare time to release the lock ,prevent block in decodeMovie
        if (status < 0)
        {
            usleep(50000);
        }
    }

    if(errHappened)
    {
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
        MMLOGE(TAG, "playing err");
        notify(DETACHING, -1, -1);
    }
    MMLOGI(TAG,"HiMediaCodecHandlr renderMovie exit");
    return 0;
}

int HiMediaCodecHandlr::vidReadFrame(bool* bSyncIFrame, void* ptr, unsigned int& dataSize, int64_t& pts, int& ptype)
{
    int ret = 0;
    bool bNeedSync = false;
    const unsigned int bufferLen = dataSize;

    if(*bSyncIFrame)
        bNeedSync = true;

    do
    {
        dataSize = bufferLen;
        if((ret = mProto->readVideoStream(ptr, dataSize, pts, ptype)) < 0)
        {
            MMLOGE(TAG, "ffmpeg readVideoStream failed");
            break;
        }
        updateCurPostion(pts);
        if(bNeedSync && ptype == H264_I_FRAME_PAYLOAD)
        {
            MMLOGI(TAG, "lost vid frame until I frame");
            bNeedSync = false;
        }
        if(bNeedSync && ptype == H265_I_FRAME_PAYLOAD)
        {
            MMLOGI(TAG, "lost vid frame until I frame");
            bNeedSync = false;
        }
    }while(bNeedSync);

    *bSyncIFrame = false;
    return ret;
}

void* HiMediaCodecHandlr::decodeMovie(void* ptr)
{
    int64_t pts = 0;
    int got_pic = 0;
    int diff = 0;
    int errHappened = 0;
    int ptype = 0;
    unsigned int readLen = 0;
    int ret = 0;
    bool bSyncIFrame = false;
    media_status_t media_ret = AMEDIA_OK;
    int64_t presentationTimeUs = 0;

    while(mRunning)
    {
        if(mVidNeedSeek)
        {
            pthread_mutex_lock(&mVidFlushLock);
            MMLOGI(TAG, "AMediaCodec_flush ");
            media_ret = AMediaCodec_flush(mediaCodc);
            if(media_ret < 0)
            {
                MMLOGE(TAG, "AMediaCodec_flush err %d", media_ret);
                errHappened = 1;
                pthread_mutex_unlock(&mVidFlushLock);
                break;
            }
            mProto->seekTo(mSeekTimeUs/1000);
            mVidNeedSeek = 0;
            mbFristVidFrame = 1;
            bSyncIFrame = true;
            mLastAudioTsUs = -1;
            pthread_mutex_unlock(&mVidFlushLock);
        }
        if(mPaused)
        {
            usleep(50000);
            continue;
        }
        ssize_t bufidx = -1;
        bufidx = AMediaCodec_dequeueInputBuffer(mediaCodc, 10000);
        //MMLOGI(TAG,"input buffer %d", bufidx);
        if (bufidx >= 0)
        {
            size_t bufsize = 0;
            uint8_t *buf = AMediaCodec_getInputBuffer(mediaCodc, bufidx, &bufsize);
            if(buf == NULL || bufsize == 0)
            {
                MMLOGE(TAG, "AMediaCodec_getInputBuffer failed");
                errHappened = 1;
                break;
            }
            readLen = bufsize;
            if((ret = vidReadFrame(&bSyncIFrame, buf, readLen, pts, ptype)) < 0)
            {
                MMLOGE(TAG, "vidReadFrame failed");
                errHappened = 1;
                AMediaCodec_queueInputBuffer(mediaCodc, bufidx, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                break;
            }
           // MMLOGI(TAG,"output pts: %lld readLen: %d buf: %d ptype: % d ret : %d", pts, readLen, *buf, ptype,ret);
            if(ret == ERROR_PROTO_EXIT)
            {
                break;
            }

            ssize_t sampleSize = readLen;
            presentationTimeUs = pts*1000;
            media_ret = AMediaCodec_queueInputBuffer(mediaCodc, bufidx, 0, sampleSize, presentationTimeUs, 0);
            if(media_ret < 0)
            {
                MMLOGE(TAG, "AMediaCodec_queueInputBuffer err %d", media_ret);
                errHappened = 1;
                break;
            }
            if(!mbFirstInputFrame)
                mbFirstInputFrame = 1;
        }
    }

    if(errHappened)
    {
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
        MMLOGE(TAG, "playing err");
    }
    notify(DETACHING, -1, -1);
    MMLOGI(TAG,"HiMediaCodecHandlr decodeMovie exit");
    return 0;
}

void HiMediaCodecHandlr::configAudioRender(int chnlCnt, int sampleRate, int sample_fmt)
{
    int AudioLatency= 0;
    MMLOGI(TAG, "audio  channels:%d, samplerate: %d, sample_fmt: %d ",chnlCnt ,
        sampleRate, sample_fmt);
        /*config the paras of AudioTrack*/
    AudioLatency = mPCMWriter->configAudioTrack(STREAM_MUSIC,
                        sampleRate,
                        ((chnlCnt == 2) ? CHANNEL_CONFIGURATION_STEREO: CHANNEL_CONFIGURATION_MONO),
                        ((sample_fmt == AV_SAMPLE_FMT_S16)?(ENCODING_PCM_16BIT):(ENCODING_PCM_8BIT)),
                        MODE_STREAM);
    mAudioLatency = AudioLatency;
}

int HiMediaCodecHandlr::getWidth(int * width)
{
    *width = mProto->getVideoWidth();
    return 0;
}

int HiMediaCodecHandlr::getHeight(int * height)
{
    *height = mProto->getVideoHeight();
    return 0;
}

#ifdef HI_AV_SYNC
bool HiMediaCodecHandlr::syncWithAudio(int64_t timeUs)
{
    //sync
    int64_t nowTs = -1;
    //MMLOGE(TAG, "sync with audio ");
    if(!(mMediaMask & PROTO_AUDIO_MASK))
    {
        if(mbFristVidFrame)
        {
            mTimeSourceDelta = getSysTimeUs() - timeUs;
            mbFristVidFrame = 0;
        }
        nowTs = getSysTimeUs() - mTimeSourceDelta;
    }
    else
    {
        if(mLastAudioTsUs != -1)
            nowTs = mLastAudioTsUs + (getSysTimeUs() - mLastAudioRealTsUs); //compensate for audio frame is too long
        else
            nowTs = timeUs;
        //MMLOGE(TAG, "now %lld timeUs: %lld audioTs: %lld ", nowTs, timeUs, mLastAudioTsUs);
    }
    int64_t latenessTs = nowTs - timeUs;
    if(latenessTs > 120000ll)
    {
            MMLOGE(TAG, "lost cur vidframe for late %lldms ", latenessTs/1000);
            return true;
    }
    if(latenessTs < -10000)
    {
        MMLOGE(TAG, "cur Frame is too early : %lldms", latenessTs/1000);
        if(latenessTs < -60000)
            usleep(60000);
        else
            usleep(REVERSE(latenessTs));
    }
    //sync end
    return false;
}
#endif

//}
