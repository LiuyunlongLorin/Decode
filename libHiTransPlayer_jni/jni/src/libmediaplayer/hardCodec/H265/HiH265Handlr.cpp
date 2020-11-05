extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/log.h"
#include "yuv2rgb_neon.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"

}
#include <unistd.h>
#include "HiH265Handlr.h"
#include "nativeWindow.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "HiMLoger.h"
#include "Hies_proto.h"
//#include "liveClientMbuf.h"
#include "comm_utils.h"
#include "HiffmpegDecoder.h"


#define TAG "HiH265Handlr"

//namespace android
//{

/*图像surface 分辨率改变时，到java层的回调函数*/
/*when the surface resoultion change, this function will be callback to java*/
static int ANWindow_setBuffersGeometry(void* pHandle, int width, int height, int format)
{
    HiH265Handlr* pHandlr = (HiH265Handlr*)pHandle;
    pHandlr->notify(MEDIA_ASR_CHANGE, -1, -1);
    return 0;
}

HiH265Handlr::HiH265Handlr(jobject jsurface,
                           JNIEnv* env,
                           MediaPlayerPCMWriter* pcmWriter,
                           int apiVer,
                           HiProtocol* protocol)
    : HiMediaHandlr(jsurface, apiVer, protocol),
      mjenv(env),
      mPCMWriter(pcmWriter)
{
    mVidThread = -1;
    mAudThread = -1;
    pANWindow = NULL;
    mDecoder = NULL;
    mOpened = 0;
    mRunning = 0;
    mWidth = 0;
    mHeight = 0;
    vidConfigFlag = 0;
    audConfigFlag = 0;
    mPixels = NULL;
    mSetVidResFlag = 0;
    mMaxWidth = 0;
    mMaxHeight = 0;
    mVidNeedSeek = 0;
    mAudNeedSeek = 0;
    mPaused = 0;
    mMediaMask = 0;
    mVidLostFlag = 0;
    mPixFormat = 0;
    mChnCnt = 0;
    mSampleRate = 0;
    mSampleFmt = 0;
    yuvFrame = (YUVFrame*)malloc(sizeof(YUVFrame));
    memset(yuvFrame, 0, sizeof(YUVFrame));
    pthread_mutex_init(&m_snaplock, NULL);
    mSwsCtx = NULL;
}

HiH265Handlr::~HiH265Handlr()
{
    pthread_mutex_destroy(&m_snaplock);
    free(yuvFrame);
}

void HiH265Handlr::notify(int msg, int ext1, int ext2)
{
    MMLOGI(TAG, "message received msg=%d, ext1=%d, ext2=%d", msg, ext1, ext2);

    if (mListener != 0)
    {
        MMLOGI(TAG, "callback application");
        mListener->notify(msg, ext1, ext2);
        MMLOGI(TAG, "back from callback");
    }
}

int HiH265Handlr::configNativeWindowRender(int width, int height)
{
    void*        pixels = NULL;

    /*获取NativeWindow的显示buffer*/
    /*get the display buffer of Native Window*/
    if (AndroidNativeWindow_getPixels(pANWindow,
                                      width, height, &pixels, WINDOW_FORMAT_RGB_565) != 0)
    {
        MMLOGE(TAG, "AndroidNativeWindow_getPixels failed");
        return -1;
    }

    mFrame = av_frame_alloc();

    if (!mFrame)
    {
        MMLOGE(TAG, "avcodec_alloc_frame failed");
        return -1;
    }


    /*关联display buffer到mFrame*/
    /*create connection between mFrame and display buffer*/
    av_image_fill_arrays(mFrame->data, mFrame->linesize,
                                    (uint8_t*) pixels,
                                    AV_PIX_FMT_RGB565,
                                    width,
                                    height,
                                    1);

    mPixels = pixels;

    //notify(MEDIA_ASR_CHANGE, -1, -1);
    return 0;
}

void HiH265Handlr::configAudioRender(int chnlCnt, int sampleRate, int sample_fmt)
{
    int AudioLatency = 0;
    MMLOGI(TAG, "audio  channels:%d, samplerate: %d, sample_fmt: %d ", chnlCnt ,
           sampleRate, sample_fmt);
    /*配置AudioTrack 参数*/
    /*config the paras of AudioTrack*/
    AudioLatency = mPCMWriter->configAudioTrack(STREAM_MUSIC,
                   sampleRate,
                   ((chnlCnt == 2) ? CHANNEL_CONFIGURATION_STEREO : CHANNEL_CONFIGURATION_MONO),
                   ((sample_fmt == AV_SAMPLE_FMT_S16) ? (ENCODING_PCM_16BIT) : (ENCODING_PCM_8BIT)),
                   MODE_STREAM);
    mAudioLatency = AudioLatency;
    MMLOGE(TAG, "AudioTrack latency : %d \n", mAudioLatency);
}

int HiH265Handlr::open()
{
    if (!mOpened)
    {
        pANWindow = AndroidNativeWindow_register(mjenv, mjsurface, mApiVersion, &ANWindow_setBuffersGeometry, this);

        if (!pANWindow)
        {
            MMLOGE(TAG, "AndroidNativeWindow_register error \n");
            return -1;
        }

        mDecoder = new HiH265Decoder();
        mWidth = mProto->getVideoWidth();
        mHeight = mProto->getVideoHeight();

        if (mSetVidResFlag)
        { mDecoder->setMaxResolution(mMaxWidth, mMaxHeight); }

        /*初始化音视频编码器
            init the audio and video coder with H265*/
        int ret = mDecoder->open();

        if (ret != 0)
        {
            MMLOGE(TAG, "mDecoder  open failed\n");
            delete mDecoder;
            mDecoder = NULL;
            AndroidNativeWindow_unregister(pANWindow);
            pANWindow = NULL;
            return -1;
        }

        mOpened = 1;
        MMLOGI(TAG, "HiH265Handlr open success!!");
    }
    else
    {
        MMLOGI(TAG, "HiH265Handlr already opened!!");
    }

    return 0;
}

int HiH265Handlr::close()
{
    if (mRunning)
    {
        stop();
        mRunning = 0;
    }

    if (mOpened)
    {
        if (pANWindow)
        {
            AndroidNativeWindow_unregister(pANWindow);
            pANWindow = NULL;
        }

        MMLOGI(TAG, "before mPixels free!!");

        if (mPixels)
        {
            av_free(mPixels);
            mPixels = NULL;
        }

        if(mSwsCtx)
        {
            sws_freeContext((SwsContext*)mSwsCtx);
            mSwsCtx = NULL;
        }

        MMLOGI(TAG, "before decoder close!!");

        if (mDecoder)
        {
            mDecoder->close();
            delete mDecoder;
            mDecoder = NULL;
        }

    }

    mOpened = 0;
    mRunning = 0;
    return 0;
}


/*循环获取音频数据，并送入decoder解码，获得PCM数据，回调java AudioTrack 播放*/
/*get the audio data , the input to decoder and get pcm output, then input pcm to audio track*/
void HiH265Handlr::decodeAudio(void* ptr)
{
    AVPacket pkt;
    int diff = 0;
    int ret = 0;
    int errHappened = 0;
    int audConfigFlag = 0;
    int mChnCnt, mSampleRate, mSampleFmt = 0;
    unsigned int readLen = 0;

    HiffmpegDecoder* iDecoder = new HiffmpegDecoder();
    stProtoAudAttr audAttr;
    memset(&audAttr, 0x00, sizeof(stProtoAudAttr));
    mProto->getAudioType(audAttr);
    ret = iDecoder->openAudioDecoder(audAttr);

    if (ret < 0)
    {
        MMLOGE(TAG, "HiffmpegDecoder open error \n");
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
        notify(DETACHING, -1, -1);
        delete iDecoder;
        return;
    }

    int gotAudio = 0;
    void* pInBuffer = av_malloc(MAX_AUDIO_FRAME_SIZE);

    if (!pInBuffer)
    {
        MMLOGE(TAG, "malloc error \n");
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
        notify(DETACHING, -1, -1);
        delete iDecoder;
        return;
    }

    uint8_t* pOutBuffer = (uint8_t*)av_malloc(MAX_AUDIO_FRAME_SIZE);

    if (!pOutBuffer)
    {
        MMLOGE(TAG, "malloc pOutBuffer error \n");
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
        notify(DETACHING, -1, -1);
        delete iDecoder;
        av_free(pInBuffer);
        return;
    }

    unsigned int outLen = 0;
    HI_U64 Audbegin = getTimeS();
    HI_U64 AudTime = Audbegin;

    //HiRtspClient* iprotocol = static_cast<HiRtspClient*>(mProto);
    while (mRunning)
    {
        if (mAudNeedSeek)
        {
            iDecoder->audioFlush();
            mAudNeedSeek = 0;
        }

        if (mPaused)
        {
            mLastAudioRealTsUs = getSysTimeUs();
            usleep(50000);
            continue;
        }

        if (getTimeS() - AudTime >= 180)
        {
            AudTime = getTimeS();
            diff = getTimeS() - Audbegin;
            MMLOGE(TAG, "display audio last  time: %d (s)!!!!", diff);
        }

        av_init_packet(&pkt);
        readLen = MAX_AUDIO_FRAME_SIZE;

        if ((ret = mProto->readAudioStream(pInBuffer, readLen, pkt.pts)) < 0)
        {
            MMLOGE(TAG, "readAudioStream failed");
            errHappened = 1;
            break;
        }

        if (ret == ERROR_PROTO_EXIT)
        { break; }

        avsyncAudioUpdate(pkt.pts);
        pkt.size = readLen;
        pkt.data = (uint8_t*)pInBuffer;
        pkt.dts = pkt.pts;
        //__android_log_print(ANDROID_LOG_INFO, TAG, "audio read pkt  pts: %lld, len: %d \n",  pkt.pts, pkt.size);
        outLen = MAX_AUDIO_FRAME_SIZE;

        if ((iDecoder->decodeAudio(pOutBuffer, &outLen, &gotAudio, &pkt)) < 0)
        {
            MMLOGE(TAG, "omx decodeAudio failed");
            errHappened = 1;
            break;
        }

        if (!gotAudio)
        { continue; }

        if (!audConfigFlag)
        {
            int ret = 0;
            ret = iDecoder->getAudioAttr(&mChnCnt, &mSampleRate, &mSampleFmt);

            if (ret < 0)
            {
                MMLOGE(TAG, "there some error happened, could not get the attr \n");
                errHappened = 1;
                break;
            }

            MMLOGE(TAG, "Audio Attr chnl: %d sampleRate: %d sampleFmt: %d pcmLen: %d\n", mChnCnt, mSampleRate, mSampleFmt, outLen);
            configAudioRender(mChnCnt, mSampleRate, AV_SAMPLE_FMT_S16);
            audConfigFlag = 1;
        }

        if (mPCMWriter)
        {
            /* if a frame has been decoded, output it */
            if (outLen > 0)
            { mPCMWriter->writePCM(pOutBuffer, outLen); }
        }
    }

    if (pOutBuffer)
    { av_free(pOutBuffer); }

    if (pInBuffer)
    { av_free(pInBuffer); }

    if (errHappened)
    {
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
        MMLOGE(TAG, "playing err");
    }

    MMLOGI(TAG, "start Notify Detaching decodeAudio");
    iDecoder->closeAudioDecoder();
    delete iDecoder;
    iDecoder = NULL;
    notify(DETACHING, -1, -1);
}



void* HiH265Handlr::startPlayVideo(void* ptr)
{
    HiH265Handlr* pHandlr = static_cast<HiH265Handlr*>(ptr);
    pHandlr->decodeMovie(ptr);
    return 0;
}

void* HiH265Handlr::startPlayAudio(void* ptr)
{
    HiH265Handlr* pHandlr = static_cast<HiH265Handlr*>(ptr);
    pHandlr->decodeAudio(ptr);
    return 0;
}

/*循环获取视频数据，并送入decoder解码，获得YUV数据，转换为
    RGB, 送入NativeWindow 更新显示*/
/*get the video frame ,  input to decoder and get YUV output, convert YUV to RGB ,then input RGB to NativeWindow for display update*/
void* HiH265Handlr::decodeMovie(void* ptr)
{
    int got_pic = 0;
    int diff = 0;
    int errHappened = 0;
    int ptype = 0;
    unsigned int readLen = 0;
    int64_t pts = 0;
    stOutVidFrameInfo frameInfo;
    stInVidDataInfo dataInfo;
    int ret = 0;
#ifdef PRINT_DECODETIME
    HI_U64 putTime = 0;
    HI_U32 decodeDelay = 0;
    HI_Test_Statistic_Init(&decodeDelay);
#endif
    HI_U64 Vidbegin = getTimeS();
    HI_U64 VidTime = Vidbegin;
    void* pBuffer = av_malloc(MAX_VIDEO_FRAME_SIZE);

    if (!pBuffer)
    {
        MMLOGE(TAG, "malloc error \n");
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
        notify(DETACHING, -1, -1);
        return 0;
    }

    memset(&frameInfo, 0x00, sizeof(stOutVidFrameInfo));
    memset(&dataInfo, 0x00, sizeof(stInVidDataInfo));
    MMLOGE(TAG, "display video begin time: %lld !!!!", Vidbegin);

    while (mRunning)
    {
        if (mPaused && !mVidNeedSeek)
        {
            usleep(50000);
            continue;
        }

        if (mVidNeedSeek)
        {
            //current every I frame is IDR(refresh), the old cached frame will be flushed after the IDR input
            //,after seek frist frame is I frame, must ensure this frame is IDR frame;then do nothing for decoder
            mProto->seekTo(mSeekTimeUs / 1000);
            mbFristVidFrame = 1;
            mLastAudioTsUs = -1;
            mVidNeedSeek = 0;
        }

        if (getTimeS() - VidTime >= 60)
        {
            VidTime = getTimeS();
            diff = getTimeS() - Vidbegin;
            MMLOGE(TAG, "display video last  time: %d (s)!!!!", diff);
        }

        readLen = MAX_VIDEO_FRAME_SIZE;

        if ((ret = mProto->readVideoStream(pBuffer, readLen, pts, ptype)) < 0)
        {
            MMLOGE(TAG, "H265 readVideoStream failed");
            errHappened = 1;
            break;
        }

        if (ret == ERROR_PROTO_EXIT)
        { break; }

        updateCurPostion(pts);
#ifdef HI_AV_SYNC

        //sync
        if (mProto->getProtoType() == PROTO_TYPE_FILE_CLIENT
            || mProto->getProtoType() == PROTO_TYPE_REMOTE_CLIENT)
        {
            int64_t timeUs = pts * 1000;
            int64_t nowTs = -1;

            if (!(mMediaMask & PROTO_AUDIO_MASK))
            {
                if (mbFristVidFrame)
                {
                    mTimeSourceDelta = getSysTimeUs() - timeUs;
                    mbFristVidFrame = 0;
                    MMLOGE(TAG, "update time delta: %lld \n", mTimeSourceDelta);
                }

                nowTs = getSysTimeUs() - mTimeSourceDelta;
            }
            else
            {
                if (mLastAudioTsUs != -1)
                { nowTs = mLastAudioTsUs + (getSysTimeUs() - mLastAudioRealTsUs); } //compensate for audio frame is too long
                else
                { nowTs = timeUs; }
            }

            //MMLOGE(TAG, "now %lld timeUs: %lld audioTs: %lld ", nowTs, timeUs, mLastAudioTsUs);
            int64_t latenessTs = nowTs - timeUs;

            if (ptype == 19 || ptype == 32)
            { mVidLostFlag = 0; }

            if (mVidLostFlag)
            {
                MMLOGE(TAG, "lost cur vidframe util I frame ");
                continue;
            }

            if (latenessTs > 120000ll)
            {
                mVidLostFlag = 1;
                MMLOGE(TAG, "lost cur vidframe for late %lldms ", latenessTs / 1000);
                continue;
            }

            if (latenessTs < -10000)
            {
                MMLOGE(TAG, "cur Frame is too early : %lldms", latenessTs / 1000);

                if (latenessTs < -60000)
                { usleep(60000); }
                else
                { usleep(REVERSE(latenessTs)); }
            }
        }

        //sync end
#endif
        dataInfo.dataLen = readLen;
        dataInfo.pData = (UINT8*)pBuffer;
        dataInfo.timestamp = pts;
        //MMLOGI(TAG, "video read pkt  pts: %lld, len: %d data ptr: 0x%08x  got_pic ptr:  0x%08x\n",  dataInfo.timestamp, dataInfo.dataLen, dataInfo.pData, &got_pic);
        HI_U64 beginDec = getTimeUs();

        if ((mDecoder->decodeVideo(&frameInfo, &got_pic, &dataInfo)) < 0)
        {
            MMLOGE(TAG, "H265 decodeVideo failed");
#ifdef PRINT_DECODETIME
            HI_LiveClient_Get_Loc(pts, &putTime, HI_TRUE);
#endif
            continue;
        }

        if (!got_pic)
        {
            continue;
        }

#ifdef PRINT_DECODETIME
        HI_U64 endtime = getTimeUs();

        if (HI_LiveClient_Get_Loc(pts, &putTime, HI_TRUE) < 0)
        {
            putTime = endtime;
        }

        HI_U64 decT = (endtime - putTime) / 1000;

        //MMLOGI(TAG, "h265decoder decode time:%lld \n",decT);
        if (decT > 0)
        {
            HI_Test_Statistic_Update(decodeDelay, decT);
        }

#endif

        if (!vidConfigFlag)
        {
            ret = mDecoder->getVideoAttr(&mWidth, &mHeight, &mPixFormat);

            if (ret < 0)
            {
                MMLOGE(TAG, "there some error happened, could not get the attr \n");
                errHappened = 1;
                break;
            }

            MMLOGE(TAG, "265 width: %d height: %d \n", mWidth, mHeight);
            ret = configNativeWindowRender(mWidth, mHeight);

            if (ret < 0)
            {
                MMLOGE(TAG, "configNativeWindowRender error\n");
                errHappened = 1;
                break;
            }

            MMLOGE(TAG, "configNativeWindowRender  OK\n");
            vidConfigFlag = 1;
        }

        getYUVFrame(&frameInfo);

#ifndef YUV2RGB_NEON
        if(!mSwsCtx)
        {
            mSwsCtx = (void*)sws_getContext(mWidth, mHeight, (enum AVPixelFormat)mPixFormat,
                                     mWidth, mHeight, AV_PIX_FMT_RGB565,
                                     SWS_BILINEAR, NULL, NULL, NULL);
        }
        uint8_t *curdata[4] = {NULL, NULL, NULL, NULL};
        int curlinesize[4] = {0, 0, 0, 0};
        curdata[0] = frameInfo.pY;
        curdata[1] = frameInfo.pU;
        curdata[2] = frameInfo.pV;
        curlinesize[0] = frameInfo.YStride;
        curlinesize[1] = frameInfo.UVStride;
        curlinesize[2] = frameInfo.UVStride;
        sws_scale((SwsContext*)mSwsCtx, (const uint8_t * const*)curdata,
                  curlinesize, 0, mHeight, mFrame->data, mFrame->linesize);
#else
        yuv420_2_rgb565_neon((uint8_t*)mPixels,
                             frameInfo.pY,
                             frameInfo.pU,
                             frameInfo.pV,
                             mWidth, mHeight,
                             frameInfo.YStride,
                             frameInfo.UVStride,
                             mWidth << 1);
#endif
        /*更新NativeWindow 当前显示图像*/
        /*update the display buffer to refresh disp image*/
        AndroidNativeWindow_update(pANWindow, mPixels, mWidth,
                                   mHeight, WINDOW_FORMAT_RGB_565);

    }

#ifdef PRINT_DECODETIME
    HI_Test_Statistic_PrintInfo(decodeDelay);
    HI_Test_Statistic_DeInit(decodeDelay);
#endif

    if (pBuffer)
    { av_free(pBuffer); }

    MMLOGE(TAG, "start Notify Detaching decodeMovie");

    if (errHappened)
    {
        /*回调error给java层，告知java层player 发生错误*/
        /*tell the player error to java*/
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
        MMLOGE(TAG, "playing err");
    }

    notify(DETACHING, -1, -1);
    return 0;
}

int HiH265Handlr::start()
{
    if (!mOpened)
    {
        MMLOGE(TAG, "have not been opened \n");
        return -1;
    }

    if (!mRunning)
    {
        mRunning = 1;
        mProto->getMediaMask(mMediaMask);
        MMLOGE(TAG, "H265 Mediamask: %d \n", mMediaMask);
        mAudThread = 0;
        mVidThread = 0;

        if (mMediaMask & PROTO_VIDEO_MASK)
        {
            pthread_create(&mVidThread, NULL, startPlayVideo, this);
            MMLOGE(TAG, "pthread_create startVideo id: %d ", mVidThread);
        }

        if (mMediaMask & PROTO_AUDIO_MASK)
        {
            pthread_create(&mAudThread, NULL, startPlayAudio, this);
            MMLOGE(TAG, "pthread_create startAudio id: %d ", mAudThread);
        }
    }
    else
    {
        if (mPaused)
        { mPaused = 0; }
    }

    return 0;
}

int HiH265Handlr::stop()
{
    if (mRunning)
    {
        mRunning = 0;

        if (pthread_join(mAudThread, 0) < 0)
        {
            MMLOGI(TAG, "pthread_join failed");
        }

        mAudThread = 0;

        if (pthread_join(mVidThread, 0) < 0)
        {
            MMLOGI(TAG, "pthread_join failed");
        }

        mVidThread = 0;
    }

    return 0;
}

int HiH265Handlr::getWidth(int* width)
{
    *width = mWidth;
    return 0;
}

int HiH265Handlr::getHeight(int* height)
{
    *height = mHeight;
    return 0;
}

void HiH265Handlr::setMaxDecodeResolution(int width, int height)
{
    mSetVidResFlag = 1;
    mMaxWidth = width;
    mMaxHeight = height;
}
void HiH265Handlr::getYUVFrame(stOutVidFrameInfo* frame)
{
    pthread_mutex_lock(&m_snaplock);

    memcpy(yuvFrame->ydata, frame->pY, frame->YStride * mHeight);
    memcpy(yuvFrame->udata, frame->pU, frame->UVStride * mHeight / 2);
    memcpy(yuvFrame->vdata, frame->pV, frame->UVStride * mHeight / 2);

    yuvFrame->mYpitch = frame->YStride;
    yuvFrame->mUpitch = frame->UVStride;
    yuvFrame->mVpitch = frame->UVStride;
    yuvFrame->mYuvPts = frame->pts;

    pthread_mutex_unlock(&m_snaplock);

}
int HiH265Handlr::getSnapData(void* ptr, int* ypitch, int* upitch, int* vpitch, int* uoffset, int* voffset, int64_t* pts )
{

    int iOffset = 0;
    pthread_mutex_lock(&m_snaplock);

    for (int i = 0; i < mHeight; i++)
    {
        memcpy(ptr + iOffset, yuvFrame->ydata + i * yuvFrame->mYpitch, mWidth);
        iOffset += mWidth;
    }

    *uoffset = iOffset;

    for (int i = 0; i < mHeight / 2; i++)
    {
        memcpy(ptr + iOffset, yuvFrame->udata + i * yuvFrame->mUpitch, mWidth / 2);
        iOffset += mWidth / 2;
    }

    *voffset = iOffset;

    for (int i = 0; i < mHeight / 2; i++)
    {
        memcpy(ptr + iOffset, yuvFrame->vdata + i * yuvFrame->mVpitch, mWidth / 2);
        iOffset += mWidth / 2;
    }

    *pts = yuvFrame->mYuvPts;
    *ypitch = mWidth;
    *upitch = mWidth / 2;
    *vpitch = mWidth / 2;


    pthread_mutex_unlock(&m_snaplock);

    MMLOGI(TAG, "get snap data pts:%d ypitch:%d upitch:%d vpitch:%d ", yuvFrame->mYuvPts, yuvFrame->mYpitch, yuvFrame->mUpitch, yuvFrame->mVpitch);
    return 0;



}


//}
