LOCAL_PATH := $(call my-dir)
MY_PATH := $(call my-dir)

include $(MY_PATH)/libyuv/Android.mk
include $(MY_PATH)/libaacenc/Android.mk
include $(MY_PATH)/mp4v2/Android.mk
include $(BUILD_SHARED_LIBRARIES)

#muxer module
LOCAL_PATH := $(MY_PATH)
include $(CLEAR_VARS)
LOCAL_MODULE    := mp4-encoder
LOCAL_SRC_FILES := mp4.cpp mp4_wrapper.cpp
LOCAL_C_INCLUDES := $(MY_PATH)/mp4v2/include \
					  $(MY_PATH)/openh264/codec/api \
					  $(MY_PATH)/libyuv/include \
					  $(MY_PATH)/libaacenc/inc
LOCAL_LDLIBS := -llog
LOCAL_STATIC_LIBRARIES := libyuv_static mp4v2 wels
LOCAL_SHARED_LIBRARIES := libaacenc $(MY_PATH)/openh264/libwels.so
include $(BUILD_SHARED_LIBRARY)

#openh264 module
LOCAL_PATH := $(MY_PATH)
include $(CLEAR_VARS)
LOCAL_MODULE    := wels
LOCAL_SRC_FILES := openh264/libwels.so
LOCAL_EXPORT_CPP_INCLUDES := $(MY_PATH)/openh264/codec/api
include $(PREBUILT_SHARED_LIBRARY)
