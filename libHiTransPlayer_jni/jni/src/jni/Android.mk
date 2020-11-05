LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_CFLAGS := -D__STDC_CONSTANT_MACROS
include $(LOCAL_PATH)/../utils.mk
WITH_CONVERTOR := false
WITH_PLAYER := true
ifeq ($(WITH_PLAYER),true)
LOCAL_CFLAGS += -DBUILD_WITH_PLAYER
endif
ifeq ($(WITH_CONVERTOR),true)
LOCAL_CFLAGS += -DBUILD_WITH_CONVERTOR
endif

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../libffmpeg \
    $(LOCAL_PATH)/../libmediaplayer \
    $(LOCAL_PATH)/../libmediaplayer/hardCodec \
    $(LOCAL_PATH)/../libmediaplayer/hardCodec/protocol \
    $(LOCAL_PATH)/../libmediaplayer/hardCodec/protocol/HiAirClient \
    $(LOCAL_PATH)/../libmediaplayer/hardCodec/demuxer

LOCAL_SRC_FILES := \
        onLoad.cpp

ifeq ($(WITH_PLAYER),true)
LOCAL_SRC_FILES += \
    com_media_ffmpeg_FFMpegPlayer.cpp \

endif

LOCAL_CFLAGS += -DHAVE_PTHREADS
LOCAL_DISABLE_FATAL_LINKER_WARNINGS := true
LOCAL_PRELINK_MODULE := false
LOCAL_LDLIBS := -llog -landroid -lz -lm -lOpenMAXAL -lmediandk
LOCAL_SHARED_LIBRARIES := libffmpeg
LOCAL_STATIC_LIBRARIES := libahplayer libHWAHStreaming libsf libyuv2rgb_neon
LOCAL_MODULE := libhi_camplayer_mediacodec
ALL_DEFAULT_INSTALLED_MODULES += $(LOCAL_MODULE)
include $(BUILD_SHARED_LIBRARY)
