#ifndef _HI_H265_DECODER_H_
#define _HI_H265_DECODER_H_

extern "C"
{
#include "libavcodec/avcodec.h"
}

#include "IHW265Dec_Api.h"

typedef struct _stInVidDataInfo
{
    UINT64  timestamp;
    UINT8*  pData;
    UINT32  dataLen;
}stInVidDataInfo;

typedef struct _stOutVidFrameInfo
{
    UINT8* pY;
    UINT8* pU;
    UINT8* pV;
    UINT64 pts;
    UINT32  decWidth;
    UINT32  decHeight;
    UINT32  YStride;
    UINT32  UVStride;
}stOutVidFrameInfo;

//namespace android{

class HiH265Decoder{
public:
    HiH265Decoder();
    int open();
    int close();
    int decodeVideo(stOutVidFrameInfo *pFrame, int *gotpic, stInVidDataInfo *pVidIn);
    int getVideoAttr(int* width, int* height, int* pixFormat);

    virtual ~HiH265Decoder();

    void clearDecodeCacheData();
    void setMaxResolution(int width, int height);

protected:
    int openVideoDecoder();

private:
    bool mFirstVideoFlag;
    bool mRunning;
    IH265DEC_HANDLE mhDecoder;
    UINT32 mVidWidth;
    UINT32 mVidHeight;
    UINT32 mMaxWidth;
    UINT32 mMaxHeight;
};
//}
#endif
