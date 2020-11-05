#ifndef _HI_FFMPEG_DECODER_H_
#define _HI_FFMPEG_DECODER_H_

extern "C"
{
#include "libavcodec/avcodec.h"
}
#include "comm_utils.h"

//namespace android{

class HiffmpegDecoder{
public:
    HiffmpegDecoder();

    int openVideoDecoder(enum AVCodecID codecID, int bMultiThread);
    int closeVideoDecoder();
    int decodeVideo(AVFrame *outdata, int *outdata_size, AVPacket *avpkt);
    int getVideoAttr(int* width, int* height, int* pixFormat);
    void videoFlush();

    int openAudioDecoder(const stProtoAudAttr& audAttr);
    int closeAudioDecoder();
    int decodeAudio(uint8_t *outdata, unsigned int *outdata_size, int* pGotData, AVPacket *avpkt);
    int getAudioAttr(int* chnlCnt, int* sampleRate, int* sample_fmt);
    void audioFlush();

    virtual ~HiffmpegDecoder();
protected:
    static void ffmpegNotify(void* ptr, int level, const char* fmt, va_list vl);

private:
    AVCodecContext*  mVdoCodecCtx;
    AVCodecContext*  mAudCodecCtx;
    bool mFirstVideoFlag;
    bool mFirstAudioFlag;
    AVFrame* mAudDecodeFrame;
    /*just for G726 begin*/
    void* pAudResampleHandle;
    uint8_t* pAudCvtOutPtr;
    /*just for G726 end*/
    enum AVCodecID mAudCodecID;
};
//}
#endif
