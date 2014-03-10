LOCAL_PATH := $(call my-dir)
MY_PATH := $(call my-dir)

include $(MY_PATH)/libyuv/Android.mk
include $(MY_PATH)/libaacenc/Android.mk
include $(BUILD_SHARED_LIBRARIES)

#openh264 module
LOCAL_PATH := $(MY_PATH)
include $(CLEAR_VARS)
LOCAL_MODULE    := wels
LOCAL_SRC_FILES := openh264/libwels.so
LOCAL_EXPORT_C_INCLUDES := $(MY_PATH)/openh264/codec/api
include $(PREBUILT_SHARED_LIBRARY)

#h264 encoding
LOCAL_PATH := $(MY_PATH)
include $(CLEAR_VARS)
LOCAL_MODULE    := h264-encoder
LOCAL_SRC_FILES := h264_encoder.c tools.c
LOCAL_C_INCLUDES := $(MY_PATH)/openh264/codec/api $(MY_PATH)/libyuv/include
LOCAL_LDLIBS := -llog
LOCAL_STATIC_LIBRARIES := wels
LOCAL_SHARED_LIBRARIES :=  libyuv_static $(MY_PATH)/openh264/libwels.so
include $(BUILD_SHARED_LIBRARY)

#aac encoding
LOCAL_PATH := $(MY_PATH)
include $(CLEAR_VARS)
LOCAL_MODULE    := aac-encoder
LOCAL_SRC_FILES := aac_encoder.c tools.c
LOCAL_C_INCLUDES := $(MY_PATH)/libaacenc/inc
LOCAL_LDLIBS := -llog
LOCAL_SHARED_LIBRARIES := libaacenc
include $(BUILD_SHARED_LIBRARY)
