#include <unistd.h>
#include "pthread.h"
#include "HiRtspClient.h"
#include "HiMLoger.h"
#include "Hies_proto.h"
#include "HiLiveRecord.h"
#include "rtsp_client_log.h"

#define TAG "HiRtspClient"

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

/*used for omxcodec use limit, on some android phone*/
volatile HI_S32 clientProtoExit = 0;
//FILE* pRecFile = NULL;

HI_S32 HiRtspClient_CallbackOnMediaControl(HI_VOID* argv,
    HI_LIVE_STREAM_CONTROL_S* pstControlInfo)
{
    HiRtspClient* pClientObj = (HiRtspClient*)argv;
    pstControlInfo->enTransType = HI_LIVE_TRANS_TYPE_TCP;
    pstControlInfo->bAudio = HI_TRUE;
    pstControlInfo->bVideo = HI_TRUE;
    return HI_SUCCESS;
}

HI_S32 HiRtspClient_CallbackOnRecvVideoFrame(const HI_U8* pu8BuffAddr, HI_U32 u32DataLen,
                               const HI_LIVE_RTP_HEAD_INFO_S* pstRtpInfo, HI_VOID* pHandle)
{
    HiRtspClient* pClientObj = (HiRtspClient*)pHandle;

    return pClientObj->onRecvVideo(pu8BuffAddr, u32DataLen, pstRtpInfo);
}


HI_S32 HiRtspClient_CallbackOnRecvAudioFrame(const HI_U8* pu8BuffAddr, HI_U32 u32DataLen,
                               const HI_LIVE_RTP_HEAD_INFO_S* pstRtpInfo, HI_VOID* pHandle)
{
    HiRtspClient* pClientObj = (HiRtspClient*)pHandle;

    return pClientObj->onRecvAudio(pu8BuffAddr, u32DataLen, pstRtpInfo);

}


HI_S32 HiRtspClient_CallbackOnEvent(HI_VOID* pHandle, HI_LIVE_PLAYSTAT_TYPE_E enPlaystate)
{
    HiRtspClient* pClientObj = (HiRtspClient*)pHandle;
    MMLOGE(TAG, "hi RtspClient event : %d happened\n", enPlaystate);

    if(enPlaystate == HI_PLAYSTAT_TYPE_ABORTIBE_DISCONNECT)
    {
        pClientObj->notify(MEDIA_ERROR, MEDIA_ERROR_SERVER_DIED, NOTIFY_EXT_NULL);
        pClientObj->notify(DETACHING, NOTIFY_EXT_NULL, NOTIFY_EXT_NULL);
        clientProtoExit = 1;
    }
    else if(enPlaystate == HI_PLAYSTAT_TYPE_CONNECTED)
    {
        pClientObj->onPlayReady();
    }
    return HI_SUCCESS;
}

static HI_VOID HI_LOG_Print(const HI_CHAR* fmt, ... )
{
    HI_CHAR aszPrintBuf[1024];
    va_list args;

    va_start(args, fmt);
    vsnprintf(aszPrintBuf, 1024,  fmt, args);
    va_end( args );
    MMLOGE(TAG, "%s", aszPrintBuf);
}

HiRtspClient::HiRtspClient(const char* url)
:HiProtocol(url)
{
    bConnected = HI_FALSE;
    mMediaMask = 0;
    bConnectReady = HI_FALSE;
    bRecordStrm = HI_FALSE;
    hAudMbufHdl = 0;
    hVidMbufHdl = 0;
    bLostUnitlIFrameFlag = HI_TRUE;
    enTransType = HI_LIVE_TRANS_TYPE_TCP;
    pu8ConstructBuf = NULL;
    u32ConstuctdataLen = 0;
    mWaterLevel = MAX_WATER_LEVEL;
    mRecordHandle = 0;
    memset(&stStreamInfo, 0x00, sizeof(stStreamInfo));
    pthread_mutex_init(&m_conlock, NULL);
    RTSP_CLIENT_LOG_SetOutputFunc(HI_LOG_Print);
    mProtoType = PROTO_TYPE_RTSP_CLIENT;
}

HiRtspClient::~HiRtspClient()
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

HI_S32 HiRtspClient::connect()
{
    HI_S32 s32Ret  = HI_SUCCESS;
    HI_RTSPCLIENT_CONFIG_S stClientCfg;

    MMLOGI(TAG, "HiRtspClient connect enter\n");
    pthread_mutex_lock(&m_conlock);

    memset(&stClientCfg, 0x00, sizeof(stClientCfg));
    memset(&stStreamInfo, 0x00, sizeof(stStreamInfo));
    mMediaMask |= PROTO_VIDEO_MASK;
    mMediaMask |= PROTO_AUDIO_MASK;

    strncpy(stClientCfg.szUrl, mUrl, RTSPCLIENT_MAX_URI_LEN-1);
    stClientCfg.enStreamType = HI_LIVE_STREAM_TYPE_AV;
    stClientCfg.pfnOnMediaControl = HiRtspClient_CallbackOnMediaControl;
    stClientCfg.pfnOnRecvVideo = HiRtspClient_CallbackOnRecvVideoFrame;
    stClientCfg.pfnOnRecvAudio = HiRtspClient_CallbackOnRecvAudioFrame;
    stClientCfg.argPriv= this;

    s32Ret = HI_RTSPCLIENT_Create(&mClientHandle, &stClientCfg);
    if (s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "HI_RTSPCLIENT_Create error\n");
        pthread_mutex_unlock(&m_conlock);
        return HI_FAILURE;
    }

    s32Ret = HI_RTSPCLIENT_SetListener(mClientHandle, HiRtspClient_CallbackOnEvent);
    if (s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "HI_RTSPCLIENT_Start error\n");
        HI_RTSPCLIENT_Destroy(mClientHandle);
        pthread_mutex_unlock(&m_conlock);
        return HI_FAILURE;
    }

    s32Ret = HI_RTSPCLIENT_Start(mClientHandle);
    if (s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "HI_RTSPCLIENT_Start error\n");
        HI_RTSPCLIENT_Destroy(mClientHandle);
        pthread_mutex_unlock(&m_conlock);
        return HI_FAILURE;
    }
    HI_U32 u32CurTimeMs = getTimeMs();
    while(!bConnectReady)
    {
        if(getTimeMs() - u32CurTimeMs > MAX_RTSP_CONN_TIMEOUTMS)
        {
            MMLOGE(TAG, "connect timeout\n");
            HI_RTSPCLIENT_Stop(mClientHandle);
            HI_RTSPCLIENT_Destroy(mClientHandle);
            pthread_mutex_unlock(&m_conlock);
            return HI_FAILURE;
        }
        usleep(10000);
    }

    s32Ret = HI_RTSPCLIENT_GetMediaInfo(mClientHandle, &stStreamInfo);
    if (s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "HI_RTSPCLIENT_Start error\n");
        HI_RTSPCLIENT_Stop(mClientHandle);
        HI_RTSPCLIENT_Destroy(mClientHandle);
        pthread_mutex_unlock(&m_conlock);
        return HI_FAILURE;
    }

    MMLOGI(TAG, "sps len: %d pps: %d with: %d height: %d frameRate: %d bitrate: %d vidformat: %d\n",
           stStreamInfo.stVencChAttr.u32SpsLen,
           stStreamInfo.stVencChAttr.u32PpsLen, stStreamInfo.stVencChAttr.u32PicWidth,
           stStreamInfo.stVencChAttr.u32PicHeight, stStreamInfo.stVencChAttr.u32FrameRate,
           stStreamInfo.stVencChAttr.u32BitRate,
           stStreamInfo.stVencChAttr.enVideoFormat);

    MMLOGI(TAG, "audformat: %d bitwidth: %d sampleRate: %d chnmode: %d bitrate: %d\n",
           stStreamInfo.stAencChAttr.enAudioFormat,
           stStreamInfo.stAencChAttr.u32BitWidth,
           stStreamInfo.stAencChAttr.u32SampleRate,
           stStreamInfo.stAencChAttr.u32Channel,
           stStreamInfo.stAencChAttr.u32BitRate);

    if (stStreamInfo.stVencChAttr.enVideoFormat != HI_LIVE_VIDEO_H264
        && stStreamInfo.stVencChAttr.enVideoFormat != HI_LIVE_VIDEO_H265)
    {
        MMLOGE(TAG, "video formate are not supported vid: %d\n", stStreamInfo.stVencChAttr.enVideoFormat);
        HI_RTSPCLIENT_Stop(mClientHandle);
        HI_RTSPCLIENT_Destroy(mClientHandle);
        pthread_mutex_unlock(&m_conlock);
        return HI_FAILURE;
    }

    if (stStreamInfo.stAencChAttr.enAudioFormat != HI_LIVE_AUDIO_AAC
        && stStreamInfo.stAencChAttr.enAudioFormat != HI_LIVE_AUDIO_G711A
        && stStreamInfo.stAencChAttr.enAudioFormat != HI_LIVE_AUDIO_G711Mu
        && stStreamInfo.stAencChAttr.enAudioFormat != HI_LIVE_AUDIO_G726
        && stStreamInfo.stAencChAttr.enAudioFormat != HI_LIVE_AUDIO_ADPCM)
    {
        MMLOGE(TAG, "audio formate are not supported aud: %d\n", stStreamInfo.stAencChAttr.enAudioFormat);
        MMLOGE(TAG, "HiRtspClient audio error just ignore audio\n");
        mMediaMask &= (~PROTO_AUDIO_MASK);
    }

    s32Ret = allocMediaBuffer();
    if (s32Ret != HI_SUCCESS)
    {
        MMLOGE(TAG, "allocMediaBuffer error\n");
        HI_RTSPCLIENT_Stop(mClientHandle);
        HI_RTSPCLIENT_Destroy(mClientHandle);
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
    MMLOGI(TAG, "HiRtspClient connect out OK\n");

    clientProtoExit = 0;
    return s32Ret;
}

HI_VOID HiRtspClient::resetMediaBuffer()
{
    HI_S32 s32Ret = HI_SUCCESS;
    if(hAudMbufHdl)
    {
        s32Ret = HI_MBUF_Reset(hAudMbufHdl);
        if(s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HiRtspClient reset audio media buffer failed\n");
        }
    }

    if(hVidMbufHdl)
    {
        s32Ret = HI_MBUF_Reset(hVidMbufHdl);
        if(s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HiRtspClient reset video media buffer failed\n");
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

HI_S32 HiRtspClient::freeMediaBuffer()
{
    HI_S32 s32Ret = HI_SUCCESS;

    if(hAudMbufHdl)
    {
        s32Ret = HI_MBuf_Destroy(hAudMbufHdl);
        if(s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HiRtspClient destory audio media buffer failed\n");
        }
    }

    if(hVidMbufHdl)
    {
        s32Ret = HI_MBuf_Destroy(hVidMbufHdl);
        if(s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HiRtspClient destory video media buffer failed\n");
        }
    }

    if(pu8ConstructBuf)
    {
        free(pu8ConstructBuf);
        pu8ConstructBuf = NULL;
    }
    return HI_SUCCESS;
}

HI_S32 HiRtspClient::allocMediaBuffer()
{
    HI_S32 s32Ret = HI_SUCCESS;

    //free prev first
    freeMediaBuffer();

    pu8ConstructBuf = (HI_U8*)malloc(MAX_VIDEO_FRAME_SIZE);
    if(!pu8ConstructBuf)
    {
        MMLOGE(TAG, "HiRtspClient malloc  pu8ContructBuf failed\n");
        return HI_FAILURE;
    }
    u32ConstuctdataLen = 0;
    memset(pu8ConstructBuf, 0x00, MAX_VIDEO_FRAME_SIZE);
    if(mMediaMask & PROTO_AUDIO_MASK)
    {
        s32Ret = HI_MBuf_Create(&hAudMbufHdl, NULL, MAX_AUD_BUF_SIZE);
        if(s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HiRtspClient create audio media buffer failed\n");
            free(pu8ConstructBuf);
            return HI_FAILURE;
        }
    }

    if(mMediaMask & PROTO_VIDEO_MASK)
    {
        HI_U32 u32MbufSize = stStreamInfo.stVencChAttr.u32PicWidth *
            stStreamInfo.stVencChAttr.u32PicHeight;
        s32Ret = HI_MBuf_Create(&hVidMbufHdl, NULL, u32MbufSize);
        if(s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HiRtspClient create video media buffer failed\n");
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


HI_S32 HiRtspClient::disconnect()
{
    HI_S32 s32Ret  = HI_SUCCESS;
    MMLOGI(TAG, "HiRtspClient disconnect\n");

    if (bConnected)
    {
        clientProtoExit = 1;

        s32Ret = HI_RTSPCLIENT_Stop(mClientHandle);
        if (s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HI_LIVE_CLIENT_Stop error\n");
        }

        s32Ret = HI_RTSPCLIENT_Destroy(mClientHandle);
        if (s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HI_LIVE_CLIENT_DeInit error\n");
        }
        bConnected = HI_FALSE;
    }
    resetMediaBuffer();

    #if 0
    if(pRecFile)
        fclose(pRecFile);
    pRecFile = NULL;
    #endif
    return HI_SUCCESS;
}

HI_S32 HiRtspClient::requestIFrame()
{
    MMLOGI(TAG, "HiRtspClient requestIFrame could  not support\n");
    return HI_FAILURE;
}

/*reconstruct rtp packets into video frames */
HI_S32 HiRtspClient::ReconstructVideoFrame(const HI_LIVE_RTP_HEAD_INFO_S* pRtpInfo,
    const HI_U8* pdata, HI_U32 dataLen, HI_S32 payload, HI_LIVE_VIDEO_TYPE_E type, HI_BOOL* pbCompleteFrame)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U8 au8frameStart[NAL_STARTCODE_LEN] = {0, 0, 0, 1};
    HI_U8 au8hevcNalheader[HEVC_NAL_HEADER_LEN] = {0, 0};
    HI_U8 u8avcNalheader = 0;

    if (CHECK_START_SLICE(pRtpInfo->u8SliceType)
        && CHECK_END_SLICE(pRtpInfo->u8SliceType)) //one frame one slice case just put the framestart
    {
        MMLOGI(TAG, "one frame one slice case  \n");
        u32ConstuctdataLen = 0;
        memcpy(&pu8ConstructBuf[u32ConstuctdataLen], au8frameStart, sizeof(au8frameStart));
        u32ConstuctdataLen += sizeof(au8frameStart);
        memcpy(&pu8ConstructBuf[u32ConstuctdataLen], pdata, dataLen);
        u32ConstuctdataLen += dataLen;

        *pbCompleteFrame = HI_TRUE;
        return HI_SUCCESS;
    }

    if (type == HI_LIVE_VIDEO_H264)
    {
        //ref to rfc3984
        u8avcNalheader = (pdata[0] & 0xE0)|(pdata[1] & 0x1F);
        //skip fua header 2 bytes for h264
        pdata += RTP_FUA_HEADER_LEN_H264;
        dataLen -= RTP_FUA_HEADER_LEN_H264;
    }
    else if (type == HI_LIVE_VIDEO_H265)
    {
        //ref to draft-schierl-payload-rtp-h265
        au8hevcNalheader[0] = ((pdata[0] & 0x81))|((pdata[2] & 0x3F)<<1);
        au8hevcNalheader[1] = pdata[1];
        //skip fua header 3 bytes for h265
        pdata += RTP_FUA_HEADER_LEN_H265;
        dataLen -= RTP_FUA_HEADER_LEN_H265;
    }

    if (CHECK_START_SLICE(pRtpInfo->u8SliceType)) //start slice case
    {
        u32ConstuctdataLen = 0;
        memcpy(&pu8ConstructBuf[u32ConstuctdataLen], au8frameStart, sizeof(au8frameStart));
        u32ConstuctdataLen += sizeof(au8frameStart);

        if (type == HI_LIVE_VIDEO_H264)
        {
            pu8ConstructBuf[u32ConstuctdataLen] = u8avcNalheader;
            u32ConstuctdataLen += sizeof(HI_U8);
        }
        else if (type == HI_LIVE_VIDEO_H265)
        {
            memcpy(&pu8ConstructBuf[u32ConstuctdataLen],
                au8hevcNalheader, HEVC_NAL_HEADER_LEN);
            u32ConstuctdataLen += HEVC_NAL_HEADER_LEN;
        }

        memcpy(&pu8ConstructBuf[u32ConstuctdataLen], pdata, dataLen);
        u32ConstuctdataLen += dataLen;
        stLastRTPInfo  = *pRtpInfo;
    }
    else if (CHECK_END_SLICE(pRtpInfo->u8SliceType)) //end slice case
    {
        if (!u32ConstuctdataLen) //pCurFrame is null while is not start slice
        {
            MMLOGI(TAG, "invalid data package, lost it prev data!!!\n");
            return HI_FAILURE;
        }

        if (u32ConstuctdataLen + dataLen > MAX_VIDEO_FRAME_SIZE)
        {
            MMLOGI(TAG, "frame datalen: %d exceed max size\n", u32ConstuctdataLen);
            u32ConstuctdataLen = 0;
            return HI_FAILURE;
        }

        if (dataLen > 0)
        {
            memcpy(&pu8ConstructBuf[u32ConstuctdataLen], pdata, dataLen);
            u32ConstuctdataLen += dataLen;
        }
        *pbCompleteFrame = HI_TRUE;

    }
    else  if (CHECK_NORMAL_SLICE(pRtpInfo->u8SliceType)) //middle slice case
    {
        if (!u32ConstuctdataLen) //pCurFrame is null while is not start slice
        {
            MMLOGI(TAG, "invalid data package, lost it prev data!!!\n");
            return HI_FAILURE;
        }

        if (pRtpInfo->u32TimeStamp!= stLastRTPInfo.u32TimeStamp)
        {
            MMLOGI(TAG, "this  frame lastpack lost\n");
            MMLOGI(TAG, "prev:%d,now:%d\n", stLastRTPInfo.u32TimeStamp, pRtpInfo->u32TimeStamp );
            return HI_FAILURE;
        }
        else
        {
            if (u32ConstuctdataLen + dataLen > MAX_VIDEO_FRAME_SIZE)
            {
                MMLOGI(TAG, "frame datalen: %d exceed max size\n", u32ConstuctdataLen);
                return HI_FAILURE;
            }

            if (dataLen > 0)
            {
                memcpy(&pu8ConstructBuf[u32ConstuctdataLen], pdata, dataLen);
                u32ConstuctdataLen += dataLen;
            }

            stLastRTPInfo = *pRtpInfo;
        }
    }

    return HI_SUCCESS;
}

HI_S32 HiRtspClient::checkMbufWaterLevel()
{
    HI_U32 u32VidBufSize =  stStreamInfo.stVencChAttr.u32PicHeight*
        stStreamInfo.stVencChAttr.u32PicWidth;

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

HI_S32 HiRtspClient::onRecvVideo(const HI_U8* pu8BuffAddr, HI_U32 u32DataLen,
                               const HI_LIVE_RTP_HEAD_INFO_S* pstRtpInfo)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HI_S32 s32Payload = 0;
    HiFrameInfo stCurFrame;

    if(!bConnected)
    {
        MMLOGE(TAG, "video buffer have not been ready\n");
        return HI_FAILURE;
    }
    if (!mMediaMask & PROTO_VIDEO_MASK)
    {
        MMLOGE(TAG, "video are not supported, should not recv\n");
        return HI_FAILURE;
    }

    if (stStreamInfo.stVencChAttr.enVideoFormat == HI_LIVE_VIDEO_H264)
    {
        //MMLOGI(TAG, "rcv vid frame len: %d pt: %d ts: %d seq: %d slicetype: %d\n", u32DataLen, pstRtpInfo->unFrameType.enH264NaluType,
        //    pstRtpInfo->u32TimeStamp, pstRtpInfo->u32Seqno, pstRtpInfo->u8SliceType);
        if (pstRtpInfo->unFrameType.enH264NaluType == HI_LIVE_H264_NALU_SPS)
        {
            s32Payload = (HI_S32)HI_LIVE_H264_NALU_IDR;
        }
        else
        { s32Payload = (HI_S32)pstRtpInfo->unFrameType.enH264NaluType; }
    }
    else if (stStreamInfo.stVencChAttr.enVideoFormat == HI_LIVE_VIDEO_H265)
    {
        //MMLOGI(TAG, "rcv vid pkt len: %d pt: %d ts: %d seq: %d\n", u32DataLen, pstRtpInfo->unFrameType.enH265NaluType, pstRtpInfo->u32TimeStamp, pstRtpInfo->u32Seqno);
        if (pstRtpInfo->unFrameType.enH265NaluType == HI_LIVE_H265_NALU_VPS)
        {
            s32Payload = (HI_S32)HI_LIVE_H265_NALU_IDR;
        }
        else
        { s32Payload = (HI_S32)pstRtpInfo->unFrameType.enH265NaluType; }
    }
    else
    {
        MMLOGE(TAG, "unknow VideoType ");
        return  HI_FAILURE;
    }

    if (bLostUnitlIFrameFlag
        && (s32Payload == HI_LIVE_H265_NALU_IDR
        || s32Payload == HI_LIVE_H264_NALU_IDR))
        { bLostUnitlIFrameFlag = HI_FALSE; }

    if (bLostUnitlIFrameFlag)
    {
        MMLOGE(TAG, "lost until I frame \n");
        return HI_SUCCESS;
    }
    /*todo: current need reconstruct frame by rtp packet outside*/
    if (HI_LIVE_TRANS_TYPE_TCP == enTransType)
    {
        HI_BOOL bCompleteFrame = HI_FALSE;
        s32Ret = ReconstructVideoFrame(pstRtpInfo, pu8BuffAddr,
            u32DataLen, s32Payload, stStreamInfo.stVencChAttr.enVideoFormat, &bCompleteFrame);
        if (s32Ret != HI_SUCCESS)
        {
            MMLOGE(TAG, "HiLiveReconstructVideoFrame failed(%d)", s32Ret);
            bLostUnitlIFrameFlag = HI_TRUE;
            return  HI_FAILURE;
        }
        if(bCompleteFrame)
        {
            stCurFrame.pData = pu8ConstructBuf;
            stCurFrame.LenData = u32ConstuctdataLen;
            stCurFrame.pts =  (pstRtpInfo->u32TimeStamp) / H264_RTP_CLOCK_FREQ;
            stCurFrame.payload = s32Payload;
            //MMLOGE(TAG, "write video frame pts: %d len: %d pt: %d\n", stCurFrame.pts, u32ConstuctdataLen, s32Payload);
            s32Ret = HI_MBUF_WriteStream(hVidMbufHdl, &stCurFrame);
            if (s32Ret != HI_SUCCESS)
            {
                MMLOGI(TAG, "HI_MBUF_WriteStream faied Ret: %d!!!\n", s32Ret);
                u32ConstuctdataLen = 0;
                bLostUnitlIFrameFlag = HI_TRUE;
                return HI_FAILURE;
            }
            //if(pRecFile)
            //    fwrite(pu8ConstructBuf, 1, u32ConstuctdataLen, pRecFile);
            u32ConstuctdataLen = 0;
            /*write record buffer*/
            if (bRecordStrm)
            {
                if(!mRecordHandle)
                {
                    HI_U32 u32VidBufSize = stStreamInfo.stVencChAttr.u32PicWidth *
                        stStreamInfo.stVencChAttr.u32PicHeight;
                    s32Ret = HI_LiveRecord_Create(&mRecordHandle, mMediaMask,
                        u32VidBufSize, MAX_AUD_BUF_SIZE);
                    if (s32Ret != HI_SUCCESS)
                    {
                        MMLOGE(TAG, "HI_LiveRecord_Create failed Ret: %d!!!\n", s32Ret);
                    }
                    MMLOGE(TAG, "HI_LiveRecord_Create success\n");
                }
                else
                {
                    s32Ret = HI_LiveRecord_WriteStream(mRecordHandle, &stCurFrame,
                        HI_RECORD_TYPE_VIDEO);
                    if (s32Ret != HI_SUCCESS)
                    {
                        MMLOGE(TAG, "HI_LiveRecord_WriteStream video faied Ret: %d!!!\n", s32Ret);
                    }
                }
            }
            checkMbufWaterLevel();
        }

    }

    return HI_SUCCESS;
}

HI_S32 HiRtspClient::onRecvAudio(const HI_U8* pu8BuffAddr, HI_U32 u32DataLen,
                               const HI_LIVE_RTP_HEAD_INFO_S* pstRtpInfo)
{
    HiFrameInfo stCurFrame;
    HI_S32 s32Ret = HI_SUCCESS;

    if(!bConnected)
    {
        MMLOGE(TAG, "audio buffer have not been ready\n");
        return HI_FAILURE;
    }

    if (!mMediaMask & PROTO_AUDIO_MASK)
    {
        MMLOGE(TAG, "audio are not supported, should not recv\n");
        return HI_FAILURE;
    }

    if(stStreamInfo.stAencChAttr.enAudioFormat == HI_LIVE_AUDIO_G711A
        || stStreamInfo.stAencChAttr.enAudioFormat == HI_LIVE_AUDIO_G711Mu
        || stStreamInfo.stAencChAttr.enAudioFormat == HI_LIVE_AUDIO_G726
        || stStreamInfo.stAencChAttr.enAudioFormat == HI_LIVE_AUDIO_ADPCM)
    {
        pu8BuffAddr += 4;
        u32DataLen -= 4;
    }

    stCurFrame.pData = (HI_U8*)pu8BuffAddr;
    stCurFrame.LenData = u32DataLen;
    stCurFrame.pts =  (pstRtpInfo->u32TimeStamp);
    stCurFrame.payload = 0;
    s32Ret = HI_MBUF_WriteStream(hAudMbufHdl, &stCurFrame);
    if (s32Ret != HI_SUCCESS)
    {
        MMLOGI(TAG, "HI_MBUF_WriteStream faied Ret: %d!!!\n", s32Ret);
        return HI_FAILURE;
    }

    /*write record buffer*/
    if (bRecordStrm && mRecordHandle)
    {
        s32Ret = HI_LiveRecord_WriteStream(mRecordHandle, &stCurFrame,
            HI_RECORD_TYPE_AUDIO);
        if (s32Ret != HI_SUCCESS)
        {
            MMLOGI(TAG, "HI_LiveRecord_WriteStream audio faied Ret: %d!!!\n", s32Ret);
        }
    }

    return HI_SUCCESS;
}

HI_S32 HiRtspClient::onPlayReady()
{
    MMLOGI(TAG, "HiRtspClient onPlayReady\n");
    bConnectReady = HI_TRUE;
    return HI_SUCCESS;
}

HI_S32 HiRtspClient::getSPSPPS(HI_VOID* sps, HI_S32* spsSize, HI_VOID* pps, HI_S32* ppsSize)
{
    MMLOGI(TAG, "HiRtspClient getSPSPPS\n");

    if (stStreamInfo.stVencChAttr.u32SpsLen> (HI_U32) *spsSize
        || stStreamInfo.stVencChAttr.u32PpsLen> (HI_U32) *ppsSize)
    {
        MMLOGE(TAG, "sps or pps buffer size is too small\n");
        return -1;
    }

    memcpy((HI_U8*)sps, stStreamInfo.stVencChAttr.au8Sps, stStreamInfo.stVencChAttr.u32SpsLen);
    *spsSize = (HI_S32)stStreamInfo.stVencChAttr.u32SpsLen;
    memcpy((HI_U8*)pps, stStreamInfo.stVencChAttr.au8Pps, stStreamInfo.stVencChAttr.u32PpsLen);
    *ppsSize = (HI_S32)stStreamInfo.stVencChAttr.u32PpsLen;
    MMLOGI(TAG, "sps  len: %d pps len: %d\n",stStreamInfo.stVencChAttr.u32SpsLen,
           stStreamInfo.stVencChAttr.u32PpsLen);
    MMLOGI(TAG, "sps:\n");
    HI_S32 i = 0;

    for (; i < *spsSize; i++)
    { MMLOGI(TAG, "0x%02x\n", stStreamInfo.stVencChAttr.au8Sps[i]); }

    MMLOGI(TAG, "pps:\n");

    for (i = 0; i < *ppsSize; i++)
    { MMLOGI(TAG, "0x%02x\n", stStreamInfo.stVencChAttr.au8Pps[i]); }

    return 0;
}

HI_S32 HiRtspClient::getVideoWidth()
{
    MMLOGI(TAG, "HiRtspClient getVideoWidth\n");
    return (HI_S32)stStreamInfo.stVencChAttr.u32PicWidth;
}

HI_S32 HiRtspClient::getVideoHeight()
{
    MMLOGI(TAG, "HiRtspClient getVideoHeight\n");
    return (HI_S32)stStreamInfo.stVencChAttr.u32PicHeight;
}

HI_S32 HiRtspClient::readMbuffer(HI_HANDLE hMbufHdl, HiFrameInfo* pstFrame)
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

HI_S32 HiRtspClient::readVideoStream(void* ptr, HI_U32& dataSize, int64_t& pts, HI_S32& ptype)
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

HI_S32 HiRtspClient::readAudioStream(void* ptr, HI_U32& dataSize, int64_t& pts)
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

HI_S32 HiRtspClient::getMediaMask(HI_S32& mediaMask)
{
    mediaMask = mMediaMask;
    return HI_SUCCESS;
}

void HiRtspClient::notify(HI_S32 msg, HI_S32 ext1, HI_S32 ext2)
{
    mlistener->notify(msg, ext1, ext2);
}

HI_S32 HiRtspClient::getVideoType(eProtoVidType& type)
{
    HI_S32 s32Ret = 0;
    pthread_mutex_lock(&m_conlock);

    if (bConnected)
    {
        switch (stStreamInfo.stVencChAttr.enVideoFormat)
        {
            case HI_LIVE_VIDEO_H264:
                type = Protocol_Video_H264;
                break;

            case HI_LIVE_VIDEO_H265:
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
    { s32Ret = HI_FAILURE;}

    pthread_mutex_unlock(&m_conlock);
    return s32Ret;
}

HI_S32 HiRtspClient::getAudioType(stProtoAudAttr& stAudAttr)
{
    HI_S32 s32Ret = 0;
    pthread_mutex_lock(&m_conlock);

    if (bConnected)
    {
        stAudAttr.sampleRate = (HI_S32)stStreamInfo.stAencChAttr.u32SampleRate;
        stAudAttr.bitwidth = stStreamInfo.stAencChAttr.u32BitWidth;
        stAudAttr.chnNum= stStreamInfo.stAencChAttr.u32Channel;

        stAudAttr.u32BitRate = stStreamInfo.stAencChAttr.u32BitRate;

        switch (stStreamInfo.stAencChAttr.enAudioFormat)
        {
            case HI_LIVE_AUDIO_AAC:
                stAudAttr.type = Protocol_Audio_AAC;
                break;

            case HI_LIVE_AUDIO_G711A:
                stAudAttr.type = Protocol_Audio_G711A;
                break;

            case HI_LIVE_AUDIO_G711Mu:
                stAudAttr.type = Protocol_Audio_G711Mu;
                break;

            case HI_LIVE_AUDIO_G726:
                stAudAttr.type = Protocol_Audio_G726LE;
                break;

            default:
                MMLOGE(TAG, "audio format are not suported\n");
                s32Ret = HI_FAILURE;
                stAudAttr.type = Protocol_Audio_BUTT;
                break;
        }
    }
    else
    { s32Ret = HI_FAILURE; }
    pthread_mutex_unlock(&m_conlock);

    return s32Ret;
}

void HiRtspClient::setVideoMbufLimit(HI_U32 dropLimit, HI_U32 clearLimit)
{
    MMLOGI(TAG, "HiRtspClient setVideoMbufLimit could not support\n");
    mWaterLevel = dropLimit;
}

void  HiRtspClient::setVideoSaveFlag(HI_S32 flag) const
{
    MMLOGI(TAG, "HiRtspClient setVideoSaveFlag flag: %d\n", flag);
}

void  HiRtspClient::setRecordFlag(HI_S32 flag)
{
    HI_S32 s32Ret = HI_SUCCESS;

    MMLOGI(TAG, "HiRtspClient setRecordFlag bRec: %d\n", flag);
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

HI_S32 HiRtspClient::getRecordVideo(void* ptr, HI_U32& dataSize, int64_t& pts, HI_S32& ptype)
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

HI_S32 HiRtspClient::getRecordAudio(void* ptr, HI_U32& dataSize, int64_t& pts,HI_S32& ptype)
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
