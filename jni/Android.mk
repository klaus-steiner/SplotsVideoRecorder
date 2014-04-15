LOCAL_PATH := $(call my-dir)
MY_PATH := $(call my-dir)

include $(MY_PATH)/libyuv/Android.mk
include $(MY_PATH)/libaacenc/Android.mk
#include $(MY_PATH)/fdk-aac/Android.mk
include $(MY_PATH)/mp4v2/Android.mk

#muxer module
LOCAL_PATH := $(MY_PATH)
include $(CLEAR_VARS)
LOCAL_MODULE    := mp4enc
LOCAL_SRC_FILES := mp4.cpp mp4_wrapper.cpp
LOCAL_C_INCLUDES := $(MY_PATH)/mp4v2/include \
					  $(MY_PATH)/openh264/codec/api \
					  $(MY_PATH)/libyuv/include \
					  $(MY_PATH)/libaacenc/inc
#					   $(MY_PATH)/fdk-aac/libAACdec/include \
 #       $(MY_PATH)/fdk-aac/libAACenc/include \
 #      	$(MY_PATH)/fdk-aac/libPCMutils/include \
 #       $(MY_PATH)/fdk-aac/libFDK/include \
 #       $(MY_PATH)/fdk-aac/libSYS/include \
 #       $(MY_PATH)/fdk-aac/libMpegTPDec/include \
 #       $(MY_PATH)/fdk-aac/libMpegTPEnc/include \
 #       $(MY_PATH)/fdk-aac/libSBRdec/include \
 #       $(MY_PATH)/fdk-aac/libSBRenc/include
LOCAL_LDLIBS := -llog
LOCAL_STATIC_LIBRARIES := libFraunhoferAAC
LOCAL_SHARED_LIBRARIES := libwels libyuv libmp4v2 libaacenc
include $(BUILD_SHARED_LIBRARY)

#openh264 module
LOCAL_PATH := $(MY_PATH)
include $(CLEAR_VARS)
LOCAL_MODULE    := libwels
LOCAL_SRC_FILES := openh264/libwels.so
LOCAL_EXPORT_CPP_INCLUDES := $(MY_PATH)/openh264/codec/api
include $(PREBUILT_SHARED_LIBRARY)