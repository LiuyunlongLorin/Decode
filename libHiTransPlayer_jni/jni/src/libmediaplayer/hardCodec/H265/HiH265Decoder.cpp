#include <stdlib.h>
#include <stdarg.h>
#include "HiH265Decoder.h"
#include "HiMLoger.h"
#include "hi_type.h"

#define TAG "HiH265Decoder"

//namespace android{

static HI_U32 g_MallocCnt = 0;
static HI_U32 g_FreeCnt = 0;

#define MAX_265_DECODE_WIDTH (2592)
#define MAX_265_DECODE_HEIGHT (1944)
#define DEFAULT_265_DECODE_WIDTH (1920)
#define DEFAULT_265_DECODE_HEIGHT (1080)
#define MAX_265_DECODE_REF_FRAME (15)

void ffmpegNotify(void* ptr, int level, const char* fmt, va_list vl)
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
            MMLOGI(TAG, "%s", tmpBuffer);
            break;

        case AV_LOG_DEBUG:
            MMLOGI(TAG, "%s", tmpBuffer);
            break;

    }
}

static void* HiH265Decoder_Malloc( UINT32 uiChannelID, UINT32 uiSize)
{
    HI_VOID* pMem = HI_NULL;
    if(uiSize > 0)
    {
        pMem = malloc(uiSize);
        if(!pMem)
        {
            MMLOGE(TAG, "malloc size: %d failed\n", uiSize);
            return NULL;
        }
    }
    g_MallocCnt++;
    return pMem;
}

static void  HiH265Decoder_Free( UINT32 uiChannelID, void *pMem)
{
    if(pMem)
    {
        free(pMem);
        g_FreeCnt++;
    }
}

static void  HiH265Decoder_LOG( UINT32 uiChannelID, IHWVIDEO_ALG_LOG_LEVEL eLevel, INT8 *pszMsg, ...)
{
    MMLOGE(TAG, "chnid: %d level: %d ",uiChannelID,  eLevel);
    HI_CHAR tmpBuffer[1024];
    va_list args;

    va_start(args, pszMsg);
    vsnprintf(tmpBuffer,1024,(HI_CHAR*)pszMsg,args);
    va_end(args);

    //MMLOGE(TAG, tmpBuffer);
}

HiH265Decoder::HiH265Decoder()
{
    mFirstVideoFlag = 0;
    mRunning = 0;
    mVidWidth = 0;
    mVidHeight = 0;
    mhDecoder = 0;
    mMaxWidth = DEFAULT_265_DECODE_WIDTH;
    mMaxHeight = DEFAULT_265_DECODE_HEIGHT;
}

HiH265Decoder::~HiH265Decoder()
{

}

int HiH265Decoder::openVideoDecoder()
{
    IHW265D_INIT_PARAM stInitParam = {0};
    IHWVIDEO_ALG_VERSION_STRU stVersion;
    int iRet = 0;

    IHW265D_GetVersion(&stVersion);
    MMLOGI(TAG, "CODEC version    : %s\n", stVersion.cVersionChar);
    MMLOGI(TAG, "CompileVersion   : %d\n", stVersion.uiCompileVersion);
    MMLOGI(TAG, "Release time     : %s\n", stVersion.cReleaseTime);

    MMLOGI(TAG, "HiH265Decoder create width : %d height: %d\n", mMaxWidth, mMaxHeight);
    /*create decode handle*/
    stInitParam.uiChannelID = 0;
    stInitParam.iMaxWidth  = mMaxWidth;
    stInitParam.iMaxHeight = mMaxHeight;
    stInitParam.iMaxRefNum = MAX_265_DECODE_REF_FRAME;

    stInitParam.iMaxVPSNum = 16;
    stInitParam.iMaxSPSNum = 16;
    stInitParam.iMaxPPSNum = 64;

    stInitParam.uiBitDepth = 10;

    stInitParam.eThreadType = IH265D_SINGLE_THREAD;
    stInitParam.eOutputOrder= IH265D_DISPLAY_ORDER;

    stInitParam.MallocFxn  = HiH265Decoder_Malloc;
    stInitParam.FreeFxn    = HiH265Decoder_Free;
    stInitParam.LogFxn     = HiH265Decoder_LOG;
    iRet = IHW265D_Create(&mhDecoder, &stInitParam);
    if (iRet != IHW265D_OK)
    {
        MMLOGE(TAG, "ERROR: IHW265D_Create failed!\n");
        return -1;
    }
    return iRet;
}

int HiH265Decoder::open()
{
    if(!mRunning)
    {
        if(openVideoDecoder() < 0)
        {
            MMLOGE(TAG,  "openVideoDecoder error \n");
            close();
            return -1;
        }
        MMLOGE(TAG,  "HiH265Decoder openVideoDecoder \n");
        mRunning = 1;
    }
    else
    {
        MMLOGE(TAG,  "HiH265Decoder already opened \n");
    }
    return 0;
}

int HiH265Decoder::close()
{
    if(mhDecoder)
    {
        MMLOGE(TAG,"destroy 265 decoder\n");
        clearDecodeCacheData();
        int ret = 0;
        ret = IHW265D_Delete(mhDecoder);
        if(ret != IHW265D_OK)
            MMLOGE(TAG,"IHW265D_Delete failed ret: %d\n",ret);
        mhDecoder = NULL;
        if (g_MallocCnt != g_FreeCnt)
        {
            MMLOGE(TAG, "warning; there is some memory leak!\n");
        }
    }
    mRunning = 0;
    mFirstVideoFlag = 0;
    return 0;
}

int HiH265Decoder::decodeVideo(stOutVidFrameInfo *pFrame, int *gotpic, stInVidDataInfo *pVidIn)
{
    int ret = 0;
    IH265DEC_INARGS stInArgs;
    IH265DEC_OUTARGS stOutArgs;

    memset(&stInArgs, 0x00, sizeof(IH265DEC_INARGS));
    memset(&stOutArgs, 0x00, sizeof(IH265DEC_OUTARGS));
    stInArgs.eDecodeMode =  IH265D_DECODE;
    stInArgs.pStream = pVidIn->pData;
    stInArgs.uiStreamLen = pVidIn->dataLen;
    stInArgs.uiTimeStamp = pVidIn->timestamp;

    //stOutArgs.eDecodeStatus = -1;
    stOutArgs.uiBytsConsumed = 0;

    //MMLOGE(TAG,"input frame pts: %lld len: %u\n", stInArgs.uiTimeStamp, stInArgs.uiStreamLen);
    ret = IHW265D_DecodeAU(mhDecoder, &stInArgs, &stOutArgs);
    if ((ret != IHW265D_OK) && (ret != IHW265D_NEED_MORE_BITS))
    {
        MMLOGE(TAG,"ERROR: IHW265D_DecodeFrame failed! ret: %d\n", ret);
        return -1;
    }
#if 1
    if (stOutArgs.eDecodeStatus == IH265D_NEED_MORE_BITS)
    {
        MMLOGE(TAG,"need more bits, no decode frame return\n");
        *gotpic = 0;
        return 0;
    }
    if(stOutArgs.eDecodeStatus == IH265D_NO_PICTURE)
    {
        MMLOGE(TAG,"there no pic in this frame\n");
        *gotpic = 0;
        return 0;
    }
    /*if(stInArgs.uiStreamLen != stOutArgs.uiBytsConsumed)
    {
        MMLOGE(TAG,"input len not been consumed, this condition should not happen stInArgs:%d  uiBytsConsumed:%d\n",stInArgs.uiStreamLen,stOutArgs.uiBytsConsumed);
        return -1;
    }*/
    //MMLOGE(TAG,"output frame pts: %lld\n", stOutArgs.uiTimeStamp);
    if (stOutArgs.eDecodeStatus == IH265D_GETDISPLAY)
    {
        pFrame->pY = stOutArgs.pucOutYUV[0];
        pFrame->pU = stOutArgs.pucOutYUV[1];
        pFrame->pV = stOutArgs.pucOutYUV[2];
        pFrame->pts = stOutArgs.uiTimeStamp;
        pFrame->decWidth= stOutArgs.uiDecWidth;
        pFrame->decHeight = stOutArgs.uiDecHeight;
        pFrame->YStride = stOutArgs.uiYStride;
        pFrame->UVStride = stOutArgs.uiUVStride;
    }
    else
    {
        MMLOGE(TAG,"some err in deocder\n");
        return -1;
    }
    #endif
    if(!mFirstVideoFlag)
    {
        mFirstVideoFlag = true;
        mVidWidth = pFrame->decWidth;
        mVidHeight = pFrame->decHeight;
    }
    *gotpic = 1;
    //MMLOGE(TAG,"out pts: %lld \n", stOutArgs.uiTimeStamp);
    //MMLOGE(TAG,"44444444444444\n");
    return 0;
}

void HiH265Decoder::clearDecodeCacheData()
{
    IH265DEC_INARGS stInArgs;
    IH265DEC_OUTARGS stOutArgs = {0};
    int ret = IHW265D_OK;

    memset(&stInArgs, 0x00, sizeof(IH265DEC_INARGS));
    memset(&stOutArgs, 0x00, sizeof(IH265DEC_OUTARGS));
    stInArgs.eDecodeMode =  IH265D_DECODE_END;
    stInArgs.pStream = NULL;
    stInArgs.uiStreamLen = 0;

    while(stOutArgs.eDecodeStatus != IH265D_NO_PICTURE)
    {
        ret = IHW265D_DecodeFrame(mhDecoder, &stInArgs, &stOutArgs);
        if ((ret != IHW265D_OK) && (ret != IHW265D_NEED_MORE_BITS))
        {
            MMLOGI(TAG, "clearCached: IHW265D_DecodeFrame failed!ret: %d \n", ret);
            break;
        }
    }
}

int HiH265Decoder::getVideoAttr(int* width, int* height, int* pixFormat)
{
    if(mFirstVideoFlag)
    {
        *width = mVidWidth;
        *height = mVidHeight;
        *pixFormat = AV_PIX_FMT_YUV420P;
        return 0;
    }
    else
    {
        MMLOGI(TAG, "there no video frame are decoded\n");
        return -1;
    }
}

void HiH265Decoder::setMaxResolution(int width, int height)
{
    MMLOGI(TAG, "HiH265Decoder setMaxResolution width: %d height: %d\n", width, height);
    if(width%16 !=0 || height%16 !=0
        || width == 0 || height == 0
        || width > MAX_265_DECODE_WIDTH
        || height > MAX_265_DECODE_HEIGHT)
    {
        MMLOGE(TAG, "HiH265Decoder setMaxResolution width or height value error\n");
        return;
    }
    mMaxWidth = width;
    mMaxHeight = height;
}

//}
