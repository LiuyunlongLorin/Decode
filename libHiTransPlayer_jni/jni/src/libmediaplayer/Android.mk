LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libffmpeg  \
    $(LOCAL_PATH)/hardCodec 

LOCAL_MODULE := libhiesproto
ALL_DEFAULT_INSTALLED_MODULES += $(LOCAL_MODULE)
LOCAL_STATIC_LIBRARIES := libyuv2rgb_neon
LOCAL_SRC_FILES += Hies_proto.c \
    comm_utils.c \
    mbuffer/HiMbuffer.c

include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libh265decoder
LOCAL_SRC_FILES := hardCodec/H265/lib/libHW_H265dec_Andr.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CFLAGS := -D__STDC_CONSTANT_MACROS -DHI_OS_ANDROID -DHI_MEDIA_CODEC_DECODING


WITH_ANDROID_VECTOR := true
include $(LOCAL_PATH)/../utils.mk
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../libffmpeg \
    $(LOCAL_PATH)/../arm_neon \
    $(LOCAL_PATH)/../jni \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/hardCodec \
    $(LOCAL_PATH)/hardCodec/protocol \
    $(LOCAL_PATH)/hardCodec/protocol/HiRtspClient \
    $(LOCAL_PATH)/hardCodec/H265 \
    $(LOCAL_PATH)/hardCodec/ffmpeg \
    $(LOCAL_PATH)/hardCodec/MediaCodec \
    $(LOCAL_PATH)/hardCodec/demuxer \
    $(LOCAL_PATH)/hardCodec/protocol/HiFileClient \
    $(LOCAL_PATH)/hardCodec/protocol/HiAirClient \
    $(LOCAL_PATH)/mbuffer \
    $(LOCAL_PATH)/hardCodec/protocol/HiLiveRecord \
    $(LOCAL_PATH)/hardCodec/protocol/HiUdpClient

LOCAL_SRC_FILES := \
    mediaplayer.cpp \
    nativeWindow.cpp \
    mbuffer/HiMbuffer.c \
    hardCodec/HiMediaHandlr.cpp \
    hardCodec/H265/HiH265Decoder.cpp \
    hardCodec/H265/HiH265Handlr.cpp \
    hardCodec/ffmpeg/HiffmpegDecoder.cpp \
    hardCodec/ffmpeg/HiffmpegHandlr.cpp \
    hardCodec/protocol/HiRtspClient/HiRtspClient.cpp \
    hardCodec/protocol/HiUdpClient/HiUdpClient.cpp \
    hardCodec/protocol/HiUdpClient/HiffmpegCache.cpp \
    hardCodec/protocol/HiFileClient/HiFileClient.cpp \
    hardCodec/protocol/HiFileClient/HiFileCacheBuf.cpp \
    hardCodec/protocol/HiAirClient/HiAirPlayClient.cpp \
    hardCodec/protocol/HiAirClient/HiCacheSource.cpp \
    hardCodec/protocol/HiAirClient/HiMbufManager.cpp \
    hardCodec/protocol/HiLiveRecord/HiLiveRecord.cpp \
    hardCodec/demuxer/HiffmpegDemux.cpp \
    hardCodec/MediaCodec/HiMediaCodecHandlr.cpp

    
RTSP_PROTO_INC := \
    $(LOCAL_PATH)/hardCodec/protocol/HiRtspClient \
    $(LOCAL_PATH)/hardCodec/protocol/HiRtspClient/rtspclient/include \
    $(LOCAL_PATH)/hardCodec/protocol/HiRtspClient/rtspclient/include/inner \
    $(LOCAL_PATH)/hardCodec/protocol/HiRtspClient/rtspclient/src

    
RTSP_PROTO_SRC := $(wildcard $(LOCAL_PATH)/hardCodec/protocol/HiRtspClient/rtspclient/src/*.c)


LOCAL_C_INCLUDES += $(RTSP_PROTO_INC)
LOCAL_SRC_FILES += $(patsubst $(LOCAL_PATH)/%, %, $(RTSP_PROTO_SRC))


LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := libffmpeg

LOCAL_STATIC_LIBRARIES :=libHWAHStreaming libyuv2rgb_neon libhiesproto libh265decoder 

LOCAL_LDLIBS += -llog -lz -lm -ldl

LOCAL_MODULE := libahplayer
ALL_DEFAULT_INSTALLED_MODULES += $(LOCAL_MODULE)
include $(BUILD_STATIC_LIBRARY)
