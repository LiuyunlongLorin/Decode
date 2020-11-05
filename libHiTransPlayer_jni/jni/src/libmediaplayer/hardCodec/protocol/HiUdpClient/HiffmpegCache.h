#ifndef __HI_FFMPEG_Cache_H__
#define __HI_FFMPEG_Cache_H__

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}
#define MAX_PARASET_LENGTH 128
#define HI_RET_FILE_EOF  (2)
#define HI_RET_OTHER_STREAM  (3)

// typedef struct _HiHevcParaSetInfo
// {
//     uint8_t sps[MAX_PARASET_LENGTH];
//     int spsLen;
//     uint8_t pps[MAX_PARASET_LENGTH];
//     int ppsLen;
//     uint8_t vps[MAX_PARASET_LENGTH];
//     int vpsLen;
//     uint8_t sei[MAX_PARASET_LENGTH];
//     int seiLen;
// }HiHevcParaSetInfo;

typedef struct tagHiAvcParaSetInfo
{
    uint8_t sps[MAX_PARASET_LENGTH];
    int spsLen;
    uint8_t pps[MAX_PARASET_LENGTH];
    int ppsLen;
}stHiAvcParaSetInfo;

typedef struct tagHiMetaTrackParaInfo
{
    int width;
    int height;
    enum AVCodecID codecId;
    stHiAvcParaSetInfo stAvcParaInfo;
}stHiMetaTrackParaInfo;

class HiffmpegCache
{
    public:
        HiffmpegCache();
        int open(char* pUrl);
        int close();
        int getVideoType(enum AVCodecID& vidCodecId);
        int getAudioType(enum AVCodecID& audCodecId, int& chnNum, int& sampleRate);
        int readFrame(AVPacket& readPkt, int& bIsVideo, int& payload);
        int getMediaMask(int& bHaveAudio, int& bHaveVideo);
        int getVideoWidth(int& width);
        int getVideoHeight(int& height);
        int getDuration(int& mSec);
        void seekTo(int mSec);
        int seekStream(int mSec);
        int getCurPostion(int& mPos);

        int getSPSPPS(void* sps, int *spsSize, void* pps, int* ppsSize);

        int selectStreamIndex(unsigned char index);
        int getMediaInfo(ProtoMediaInfo& mediaInfo);

        ~HiffmpegCache();

private:
    int readDataFFmpeg(AVPacket& readPkt, int& bIsVideo, int& payload);
    int getMetaResolution(enum AVCodecID codec_id, AVPacket* pPkt, int* pWidth, int* pHeight);
    int parseMetaSPS(enum AVCodecID codecId, uint8_t* pStrmPtr, int dataLen);
    int getMetaCodecpar(AVFormatContext *pInCtx, enum AVCodecID* pRawCodecId, int* pwidth , int* pheight);
    void probeMetaTrack(AVStream*  pVideoStream);
    int createStreamFilter();
    void destroyStreamFilter();
    void discardStream();

    private:
        int mOpened;
        AVFormatContext* mpFmtCtx;
        AVStream *mVideoStream;
        int mVideoStreamIdx;
        enum AVCodecID mVidCodecId;
        AVStream *mAudioStream;
        int mAudioStreamIdx;
        enum AVCodecID mAudCodecId;
        int mbHaveVideo;
        int mbHaveAudio;
        // HiHevcParaSetInfo mHevcParaInfo;
        AVBSFContext* mH264StreamFilter;
        int64_t mLastReadPts;
        stHiMetaTrackParaInfo mMetaTrkInfo;
};

#endif
