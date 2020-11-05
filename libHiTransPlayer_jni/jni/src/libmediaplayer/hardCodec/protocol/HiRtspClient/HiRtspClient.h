#ifndef  _HI_RTSP_CLIENT_H_
#define _HI_RTSP_CLIENT_H_

#include "HiProtocol.h"
#include "hi_rtsp_client.h"
#include "HiMbuffer.h"

class HiRtspClient:public HiProtocol{
public:
    HiRtspClient(const char* url);
    virtual HI_S32 getSPSPPS(void* sps, int* spsSize, void* pps, int* ppsSize);
    virtual HI_S32 getVideoWidth();
    virtual HI_S32 getVideoHeight();
    virtual HI_S32 readVideoStream(void* ptr, HI_U32& dataSize, int64_t& pts, int& ptype);
    virtual HI_S32 readAudioStream(void* ptr, HI_U32& dataSize, int64_t& pts);
    virtual HI_S32 connect();
    virtual HI_S32 disconnect();
    virtual HI_S32 getMediaMask(HI_S32& mediaMask);
    virtual HI_S32 requestIFrame();
    void notify(HI_S32 msg, HI_S32 ext1, HI_S32 ext2);
    virtual HI_S32 getVideoType(eProtoVidType& type);
    virtual HI_S32 getAudioType(stProtoAudAttr& audAttr);
    virtual void setVideoMbufLimit(HI_U32 dropLimit, HI_U32 clearLimit);
    void  setVideoSaveFlag(HI_S32 flag) const ;
    void setLivePlayMode(HI_S32 mode);
    void  setRecordFlag(HI_S32 flag) ;
    HI_S32 getRecordVideo(void* ptr, HI_U32& dataSize, int64_t& pts, HI_S32& ptype);
    HI_S32 getRecordAudio(void* ptr, HI_U32& dataSize, int64_t& pts,HI_S32& ptype) ;
    HI_S32 onPlayReady();
    HI_S32 onRecvVideo(const HI_U8* pu8BuffAddr, HI_U32 u32DataLen,
                                   const HI_LIVE_RTP_HEAD_INFO_S* pstRtpInfo);
    HI_S32 onRecvAudio(const HI_U8* pu8BuffAddr, HI_U32 u32DataLen,
                                   const HI_LIVE_RTP_HEAD_INFO_S* pstRtpInfo);
    virtual ~HiRtspClient();

private:
    HI_VOID resetMediaBuffer();
    HI_S32 freeMediaBuffer();
    HI_S32 allocMediaBuffer();
    HI_S32 ReconstructVideoFrame(const HI_LIVE_RTP_HEAD_INFO_S* pRtpInfo,
        const HI_U8* pdata, HI_U32 dataLen, HI_S32 payload, HI_LIVE_VIDEO_TYPE_E type, HI_BOOL* pbCompleteFrame);
    HI_S32 checkMbufWaterLevel();
    HI_S32 readMbuffer(HI_HANDLE hMbufHdl, HiFrameInfo* pstFrame);


private:
    HI_MW_PTR mClientHandle;
    HI_HANDLE mRecordHandle;
    pthread_mutex_t m_conlock;
    HI_LIVE_STREAM_INFO_S stStreamInfo;
    HI_BOOL bConnectReady;
    HI_BOOL bRecordStrm;
    HI_U32 hAudMbufHdl;
    HI_U32 hVidMbufHdl;
    HI_BOOL bLostUnitlIFrameFlag;
    HI_LIVE_TRANS_TYPE_E enTransType;
    /*used to reconstruct rtp packet to frame*/
    HI_U8* pu8ConstructBuf;
    HI_U32 u32ConstuctdataLen;
    HI_LIVE_RTP_HEAD_INFO_S stLastRTPInfo;
    HI_U32 mWaterLevel;
};

#endif
