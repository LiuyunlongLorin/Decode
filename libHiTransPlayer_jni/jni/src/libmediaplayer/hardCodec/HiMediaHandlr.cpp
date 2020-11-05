#include "HiMediaHandlr.h"
#include "HiMLoger.h"

#define TAG "HiMediaHandlr"

HiMediaHandlr::HiMediaHandlr(jobject jsurface, int apiVer, HiProtocol* protocol)
    :mjsurface(jsurface),
    mApiVersion(apiVer),
    mProto(protocol)
{
    MMLOGI(TAG, "HiMediaHandlr::HiMediaHandlr");
    mPosUpdateCnt = MAX_POS_UPDATE_DURATION;
    mCurPosMs = 0;
#ifdef HI_AV_SYNC
    mbFristVidFrame = 1;
    mTimeSourceDelta = -1;
    mLastAudioTsUs = -1;
    mLastAudioRealTsUs = -1;
    mAudioLatency = 0;
    mListener=NULL;
    mMediaMask=0;
    mVidNeedSeek=0;
    mSeekTimeUs=-1;
    mAudNeedSeek=0;
    mPaused=0;

#endif
}

void HiMediaHandlr::seekTo(int mSec, bool bAtEos)
{
    if(bAtEos)
    {
        mProto->seekTo(mSec);
        mCurPosMs = mSec;
        mPosUpdateCnt = 0;
        mLastAudioTsUs = -1;
        if(mMediaMask & PROTO_AUDIO_MASK)
            mAudNeedSeek = 1;
        return;
    }
    MMLOGI(TAG, "HiMediaHandlr::seekTo %d ", mSec);
    if(mMediaMask & PROTO_VIDEO_MASK)
    {
        mVidNeedSeek = 1;
    }
    if(mMediaMask & PROTO_AUDIO_MASK)
    {
        mAudNeedSeek = 1;
    }
    mSeekTimeUs = (int64_t)mSec*1000;
    mCurPosMs = mSec;
    mPosUpdateCnt = 0;
}

int HiMediaHandlr::getCurPostion(int& mPos)
{
    //MMLOGI(TAG, "HiMediaHandlr::getCurPostion");
    int duration = 0;
    mProto->getDuration(duration);
    if(duration && (mCurPosMs + 500) >  duration)
        mPos = duration;
    else
        mPos = mCurPosMs;
    if(mProto->getProtoType() == PROTO_TYPE_RTSP_CLIENT)
        mPos = 0;
    if(mProto->getProtoType() == PROTO_TYPE_UDP_CLIENT)
        mPos = 0;
    return 0;
}

void HiMediaHandlr::updateCurPostion(int64_t& pts)
{
    //MMLOGI(TAG, "HiMediaHandlr::updateCurPostion");
    if(mPosUpdateCnt < MAX_POS_UPDATE_DURATION)
        mPosUpdateCnt++;
    else
        mCurPosMs = pts;
}

void  HiMediaHandlr::pause()
{
    mPaused = 1;
}

void HiMediaHandlr::avsyncAudioUpdate(int64_t pts)
{
#ifdef HI_AV_SYNC
    if(mProto->getProtoType() == PROTO_TYPE_FILE_CLIENT
        || mProto->getProtoType() == PROTO_TYPE_REMOTE_CLIENT)
    {
        mLastAudioTsUs = ((pts > mAudioLatency) ? (pts-mAudioLatency) : 0)*1000;
        mLastAudioRealTsUs = getSysTimeUs();
    }
#endif
}
