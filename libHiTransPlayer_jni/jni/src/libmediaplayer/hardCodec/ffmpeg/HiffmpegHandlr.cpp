extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/log.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"

#include "Hies_proto.h"
#include "yuv2rgb_neon.h"

}
#include <unistd.h>
#include "HiffmpegHandlr.h"
#include "nativeWindow.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "HiMLoger.h"
#include "HiRtspClient.h"
#include "comm_utils.h"



#define VIDEOPIXELFORMAT AV_PIX_FMT_RGB565

#define TAG "HiffmpegHandlr"

//namespace android
//{

/*图像surface 分辨率改变时，到java层的回调函数*/
/*when the surface resoultion change, this function will be callback to java*/
static int ANWindow_setBuffersGeometry(void* pHandle, int width, int height, int format)
{
    HiffmpegHandlr* pHandlr = (HiffmpegHandlr*)pHandle;
    //pHandlr->notify(MEDIA_ASR_CHANGE, -1, -1);
    return 0;
}

HiffmpegHandlr::HiffmpegHandlr(jobject jsurface,
                               JNIEnv* env,
                               MediaPlayerPCMWriter* pcmWriter,
                               int apiVer,
                               HiProtocol* protocol)
    : HiMediaHandlr(jsurface, apiVer, protocol),
      mjenv(env),
      mPCMWriter(pcmWriter)
{
    mFrame = NULL;
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
    mVidNeedSeek = 0;
    mAudNeedSeek = 0;
    mPaused = 0;
    mVidLostFlag = 0;
    mPixFormat = 0;
    mChnCnt = 0;
    mSampleRate = 0;
    mSampleFmt = 0;
    yuvFrame=(YUVFrame*)malloc(sizeof(YUVFrame));
    memset(yuvFrame, 0, sizeof(YUVFrame));
    pthread_mutex_init(&m_snaplock, NULL);
    mSwsCtx = NULL;
}

HiffmpegHandlr::~HiffmpegHandlr()
{
    pthread_mutex_destroy(&m_snaplock);
    free(yuvFrame);
}

void HiffmpegHandlr::notify(int msg, int ext1, int ext2)
{
    MMLOGI(TAG, "message received msg=%d, ext1=%d, ext2=%d", msg, ext1, ext2);
    bool send = true;
    bool locked = false;

    if ((mListener != 0) && send)
    {
        MMLOGI(TAG, "callback application");
        mListener->notify(msg, ext1, ext2);
        MMLOGI(TAG, "back from callback");
    }
}

int HiffmpegHandlr::configNativeWindowRender(int width, int height)
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

    return 0;
}

void HiffmpegHandlr::configAudioRender(int chnlCnt, int sampleRate, int sample_fmt)
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

int HiffmpegHandlr::open()
{
    int AudioLatency;
    stProtoAudAttr audAttr;
    enum AVCodecID codeId;
    int ret = 0;

    if (!mOpened)
    {
        pANWindow = AndroidNativeWindow_register(mjenv, mjsurface, mApiVersion, &ANWindow_setBuffersGeometry, this);

        if (!pANWindow)
        {
            MMLOGE(TAG, "AndroidNativeWindow_register error \n");
            return -1;
        }

        mProto->getMediaMask(mMediaMask);
        mWidth = mProto->getVideoWidth();
        mHeight = mProto->getVideoHeight();
        mDecoder = new HiffmpegDecoder();



        /*初始化音视频编码器
            init the audio and video coder with ffmpeg*/
        int bMultiThread = 0;

        if (mProto->getProtoType() == PROTO_TYPE_FILE_CLIENT
            || mProto->getProtoType() == PROTO_TYPE_REMOTE_CLIENT
            || (mProto->getVideoWidth() >= 1280 && mProto->getVideoHeight() >= 720))
        { bMultiThread = 1; }

        mDecoder->openVideoDecoder(AV_CODEC_ID_H264, bMultiThread);

        if (mMediaMask & PROTO_AUDIO_MASK)
        {
            memset(&audAttr, 0x00, sizeof(stProtoAudAttr));
            mProto->getAudioType(audAttr);
            ret = mDecoder->openAudioDecoder(audAttr);

            if (ret < 0)
            {
                MMLOGE(TAG, "openAudioDecoder error \n");
                delete mDecoder;
                return -1;
            }
        }

        mOpened = 1;
    }
    else
    {
        MMLOGI(TAG, "HiffmpegHandlr already opened!!");
    }

    return 0;
}

int HiffmpegHandlr::close()
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

        MMLOGI(TAG, "before mFrame free!!");

        if (mFrame)
        {
            if (mFrame->data[0])
            {
                av_free(mFrame->data[0]);
                mFrame->data[0] = NULL;
            }

            av_frame_free(&mFrame);
            mFrame = NULL;
        }

        if(mSwsCtx)
        {
            sws_freeContext((SwsContext*)mSwsCtx);
            mSwsCtx = NULL;
        }

        mDecoder->closeVideoDecoder();
        mDecoder->closeAudioDecoder();
        delete mDecoder;
        mDecoder = NULL;
    }

    mOpened = 0;
    mRunning = 0;
    return 0;
}


/*循环获取音频数据，并送入decoder解码，获得PCM数据，回调java AudioTrack 播放*/
/*get the audio data , the input to decoder and get pcm output, then input pcm to audio track*/
void* HiffmpegHandlr::decodeAudio(void* ptr)
{
    AVPacket pkt;
    int gotAudio = 0;
    int diff = 0;
    int errHappened = 0;
    unsigned int readLen = 0;
    int ret = 0;

    void* pInBuffer = av_malloc(MAX_AUDIO_FRAME_SIZE);

    if (!pInBuffer)
    {
        MMLOGE(TAG, "malloc pInBuffer error \n");
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
        notify(DETACHING, -1, -1);
        return 0;
    }

    uint8_t* pOutBuffer = (uint8_t*)av_malloc(MAX_AUDIO_FRAME_SIZE);

    if (!pOutBuffer)
    {
        MMLOGE(TAG, "malloc pOutBuffer error \n");
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
        notify(DETACHING, -1, -1);
        return 0;
    }
    unsigned int outLen = 0;
    HI_U64 Audbegin = getTimeS();
    HI_U64 AudTime = Audbegin;
    while (mRunning)
    {
        if (mAudNeedSeek)
        {
            mDecoder->audioFlush();
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
        //MMLOGI(TAG, "audio read pkt  pts: %lld, len: %d \n",  pkt.pts, pkt.size);
        outLen = MAX_AUDIO_FRAME_SIZE;
        int len = mDecoder->decodeAudio(pOutBuffer, &outLen, &gotAudio, &pkt);

        if (len < 0)
        {
            MMLOGE(TAG, "HI_HIES_decodeAudio failed");
            errHappened = 1;
            break;
        }

        if (!gotAudio)
        { continue; }

        if (!audConfigFlag)
        {
            ret = mDecoder->getAudioAttr(&mChnCnt, &mSampleRate, &mSampleFmt);

            if (ret < 0)
            {
                MMLOGE(TAG, "there some error happened, could not get the attr \n");
                errHappened = 1;
                break;
            }

            configAudioRender(mChnCnt, mSampleRate, AV_SAMPLE_FMT_S16);
            audConfigFlag = 1;
        }

        if (mPCMWriter)
        {
            if (outLen > 0)
            { mPCMWriter->writePCM(pOutBuffer, outLen); }

            //MMLOGE(TAG, "write pcm len: %d", outLen);
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
    notify(DETACHING, -1, -1);
    return 0;
}

void* HiffmpegHandlr::startPlayVideo(void* ptr)
{
    HiffmpegHandlr* pHandlr = static_cast<HiffmpegHandlr*>(ptr);
    pHandlr->decodeMovie(ptr);
    return 0;
}

void* HiffmpegHandlr::startPlayAudio(void* ptr)
{
    HiffmpegHandlr* pHandlr = static_cast<HiffmpegHandlr*>(ptr);
    pHandlr->decodeAudio(ptr);
    return 0;
}

/*循环获取视频数据，并送入decoder解码，获得YUV数据，转换为
    RGB, 送入NativeWindow 更新显示*/
/*get the video frame ,  input to decoder and get YUV output, convert YUV to RGB ,then input RGB to NativeWindow for display update*/
void* HiffmpegHandlr::decodeMovie(void* ptr)
{
    AVPacket pkt;
    int got_pic = 0;
    int diff = 0;
    int errHappened = 0;
    int ptype = 0;
    unsigned int readLen = 0;
    int ret = 0;
    AVFrame* frame = av_frame_alloc();

    if (frame == NULL)
    {
        MMLOGE(TAG, "avcodec_alloc_frame failed");
        notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, -1);
        notify(DETACHING, -1, -1);
        return 0;
    }

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

    MMLOGE(TAG, "display video begin time: %lld !!!!", Vidbegin);

#ifdef PRINT_DECODETIME
    HI_U64 putTime = 0;
    HI_U32 decodeDelay = 0;
    HI_Test_Statistic_Init(&decodeDelay);
#endif
    while (mRunning)
    {
        if (mPaused && !mVidNeedSeek)
        {
            mbFristVidFrame = 1;
            usleep(50000);
            continue;
        }

        if (mVidNeedSeek)
        {
            mDecoder->videoFlush();
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

        av_init_packet(&pkt);
        readLen = MAX_VIDEO_FRAME_SIZE;

        if ((ret = mProto->readVideoStream(pBuffer, readLen, pkt.pts, ptype)) < 0)
        {
            MMLOGE(TAG, "ffmpeg readVideoStream failed");
            errHappened = 1;
            break;
        }

        if (ret == ERROR_PROTO_EXIT)
        { break; }

        updateCurPostion(pkt.pts);
#ifdef HI_AV_SYNC

        //sync
        if (mProto->getProtoType() == PROTO_TYPE_FILE_CLIENT
            || mProto->getProtoType() == PROTO_TYPE_REMOTE_CLIENT)
        {
            int64_t timeUs = pkt.pts * 1000;
            int64_t nowTs = -1;

            if (mbFristVidFrame)
            {
                mTimeSourceDelta = getSysTimeUs() - timeUs;
                mbFristVidFrame = 0;
            }

            if (mLastAudioTsUs != -1)
            { nowTs = mLastAudioTsUs + (getSysTimeUs() - mLastAudioRealTsUs); } //compensate for audio frame is too long
            else
            {
                nowTs = getSysTimeUs() - mTimeSourceDelta;
            }

            int64_t latenessTs = nowTs - timeUs;

            if (ptype == 5)
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
        pkt.size = readLen;
        pkt.data = (uint8_t*)pBuffer;
        pkt.dts = pkt.pts;

        if (ptype == 5)
        { pkt.flags |= AV_PKT_FLAG_KEY; }

        //MMLOGI(TAG, "video read pkt  pts: %lld, len: %d data ptr: 0x%08x  got_pic ptr:  0x%08x\n",  pkt.pts, pkt.size, pkt.data, &got_pic);
        if ((mDecoder->decodeVideo(frame, &got_pic, &pkt)) < 0)
        {
            MMLOGE(TAG, "ffmpeg decodeVideo failed");

#ifdef PRINT_DECODETIME
            HI_LiveClient_Get_Loc(pkt.pts, &putTime, HI_TRUE);
#endif
            continue;
        }

        if (!got_pic)
        {
            continue;
        }

#ifdef PRINT_DECODETIME
        HI_U64 endtime = getTimeUs();

        if (HI_LiveClient_Get_Loc(pkt.pts, &putTime, HI_TRUE) < 0)
        {
            putTime = endtime;
        }

        HI_U64 decT = (endtime - putTime) / 1000;

        //MMLOGI(TAG, "ffmpeg decode time:%lld \n",decT);
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

            mWidth = frame->width;
            mHeight = frame->height;
            MMLOGE(TAG, "width: %d height: %d\n", mWidth, mHeight);
            ret = configNativeWindowRender(mWidth, mHeight);

            if (ret < 0)
            {
                MMLOGE(TAG, "configNativeWindowRender error\n");
                errHappened = 1;
                break;
            }

            vidConfigFlag = 1;
        }

        getYUVFrame(frame);

        if (mPixFormat == AV_PIX_FMT_RGB565)
        {
            memcpy(mFrame->data[0], frame->data[0],
                av_image_get_buffer_size(VIDEOPIXELFORMAT, mWidth, mHeight, 1));
        }
        else
        {
#ifndef YUV2RGB_NEON
            if(!mSwsCtx)
            {
                mSwsCtx = (void*)sws_getContext(mWidth, mHeight, (enum AVPixelFormat)frame->format,
                                         mWidth, mHeight, AV_PIX_FMT_RGB565,
                                         SWS_BILINEAR, NULL, NULL, NULL);
            }
            sws_scale((SwsContext*)mSwsCtx, (const uint8_t * const*)frame->data,
                      frame->linesize, 0, mHeight, mFrame->data, mFrame->linesize);
#else
            if (VIDEOPIXELFORMAT == AV_PIX_FMT_RGB565)
            {
                yuv420_2_rgb565_neon(mFrame->data[0], frame->data[0], frame->data[1],
                                     frame->data[2],
                                     mWidth, mHeight, frame->linesize[0], frame->linesize[1],
                                     mWidth << 1);
            }
            else if (VIDEOPIXELFORMAT == AV_PIX_FMT_RGBA)
            {
                yuv420_2_rgba_neon(mFrame->data[0], frame->data[0], frame->data[1],
                                   frame->data[2], mWidth, mHeight,
                                   frame->linesize[0], frame->linesize[1],
                                   mWidth << 2);
            }
#endif
        }
        //MMLOGE(TAG, "rend pts: %lld\n", frame->pkt_dts);
        /*更新NativeWindow 当前显示图像*/
        /*update the display buffer to refresh disp image*/
        AndroidNativeWindow_update(pANWindow, mFrame->data[0], mWidth,
                                   mHeight, WINDOW_FORMAT_RGB_565);


    }

#ifdef PRINT_DECODETIME
    HI_Test_Statistic_PrintInfo(decodeDelay);
    HI_Test_Statistic_DeInit(decodeDelay);
#endif

    if (pBuffer)
    { av_free(pBuffer); }

    if (frame)
    { av_free(frame); }

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

int HiffmpegHandlr::start()
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

int HiffmpegHandlr::stop()
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

int HiffmpegHandlr::getWidth(int* width)
{
    *width = mWidth;
    return 0;
}

int HiffmpegHandlr::getHeight(int* height)
{
    *height = mHeight;
    return 0;
}
void HiffmpegHandlr::getYUVFrame(AVFrame* frame)
{
    pthread_mutex_lock(&m_snaplock);

    memcpy(yuvFrame->ydata, frame->data[0], frame->linesize[0]*mHeight);
    memcpy(yuvFrame->udata, frame->data[1],  frame->linesize[1]*mHeight / 2);
    memcpy(yuvFrame->vdata, frame->data[2], frame->linesize[2]*mHeight / 2);

    yuvFrame->mYpitch = frame->linesize[0];
    yuvFrame->mUpitch = frame->linesize[1];
    yuvFrame->mVpitch = frame->linesize[2];
    yuvFrame->mYuvPts = frame->pkt_pts;

    pthread_mutex_unlock(&m_snaplock);
}

int HiffmpegHandlr::getSnapData(void* ptr, int* ypitch, int* upitch, int* vpitch, int* uoffset, int* voffset, int64_t* pts )
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
    *ypitch = yuvFrame->mYpitch;
    *upitch = yuvFrame->mUpitch;
    *vpitch = yuvFrame->mVpitch;


    pthread_mutex_unlock(&m_snaplock);

    MMLOGI(TAG, "get snap data pts:%d ypitch:%d upitch:%d vpitch:%d ", yuvFrame->mYuvPts, yuvFrame->mYpitch,yuvFrame->mUpitch, yuvFrame->mVpitch);
    return 0;



}


//}
