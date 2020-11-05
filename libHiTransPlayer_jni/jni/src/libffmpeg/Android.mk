LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := libswresample
LOCAL_SRC_FILES := android/arm/lib/libswresample.a
LOCAL_LDLIBS := -lz -lm
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libavresample
LOCAL_SRC_FILES := android/arm/lib/libavresample.a
LOCAL_LDLIBS := -lz -lm
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libavcodec
LOCAL_SRC_FILES := android/arm/lib/libavcodec.a
LOCAL_LDLIBS := -lz -lm
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libavfilter
LOCAL_SRC_FILES := android/arm/lib/libavfilter.a
LOCAL_LDLIBS := -lz -lm
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libavformat
LOCAL_SRC_FILES := android/arm/lib/libavformat.a
LOCAL_LDLIBS := -lz -lm
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libavutil
LOCAL_SRC_FILES := android/arm/lib/libavutil.a
LOCAL_LDLIBS := -lz -lm
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libswscale
LOCAL_SRC_FILES := android/arm/lib/libswscale.a
LOCAL_LDLIBS := -lz -lm
include $(PREBUILT_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libffmpeg
#LOCAL_SRC_FILES := test.c
LOCAL_WHOLE_STATIC_LIBRARIES := libavcodec libavfilter libavformat libavutil libswscale libswresample libavresample
LOCAL_LDLIBS := -lz -lm
include $(BUILD_SHARED_LIBRARY)
