
#include <stdlib.h>
#include <stdarg.h>
#include "Hies_proto.h"


/*缓存的视频帧，最大个数为MAX_VIDEO_BUF_NUM*/
/*cached video frame , max MAX_VIDEO_BUF_NUM*/
FrameInfo mframeBuf[MAX_VIDEO_BUF_NUM] = {0};


/*缓存的音频帧，最大个数为MAX_AUDIO_BUF_NUM*/
/*cached audio frame , max MAX_AUDIO_BUF_NUM*/
AudFrameInfo mAudframeBuf[MAX_AUDIO_BUF_NUM] = {0};
