/*
 * mp4.h
 *
 *  Created on: 15.03.2014
 *  Author: Klaus Steiner
 */

#ifndef MP4_H_
#define MP4_H_

#include <string.h>
#include <stdio.h>
#include <iostream>
#include <jni.h>
#include <android/log.h>
#include "openh264/codec/api/svc/codec_api.h"
#include "mp4v2/include/mp4v2/mp4v2.h"
#include "libaacenc/inc/voAAC.h"
#include "libaacenc/inc/cmnMemory.h"
#include "libyuv/include/libyuv.h"

#ifdef __cplusplus
extern "C" {
#endif

class Mp4Encoder {
public:
	Mp4Encoder();

	~Mp4Encoder();

	void init(JNIEnv* env, jobject thiz, jstring outPath);

	void initVideo(JNIEnv* env, jobject thiz, jfloat frameRate, jint srcWidth,
			jint srcHeight, jobject cropRect, jint destWidth, jint destHeight);

	void initAudio(JNIEnv* env, jobject thiz, jint bitrate, jint channels,
			jint sample_rate);

	void setFrameRate(JNIEnv* env, jobject thiz, jfloat frameRate);

	jfloat getAverageFrameRate(JNIEnv* env, jobject thiz);

	void setPreviewSize(JNIEnv* env, jobject thiz, jint srcWidth,
			jint srcHeight);

	void setVideoBitrate(JNIEnv* env, jobject thiz, jint bitRate);

	void setCropRect(JNIEnv* env, jobject thiz, jobject cropRect);

	void setCameraInfo(JNIEnv* env, jobject thiz, jobject cameraInfo);

	jboolean encodePreviewFrame(JNIEnv* env, jobject thiz, jbyteArray preview,
			jlong timeStamp);

	jboolean encodeAudioSample(JNIEnv* env, jobject thiz, jbyteArray audio);

	jbyteArray getThumbnailData(JNIEnv* env, jobject thiz);

	jint getThumbnailWidth(JNIEnv* env, jobject thiz);

	jint getThumbnailHeight(JNIEnv* env, jobject thiz);

	void release(JNIEnv* env, jobject thiz);

private:
	ISVCEncoder *h264Encoder;
	VO_AUDIO_CODECAPI *aacEncoder;
	VO_MEM_OPERATOR *aacMemoryOperator;
	VO_CODEC_INIT_USERDATA *aacUserData;
	VO_HANDLE aacHandle;
	AACENC_PARAM *audioParameter;
	MP4FileHandle mp4FileHandler;
	SEncParamBase *videoParameter;
	SFrameBSInfo *h264EncoderInfo;
	MP4TrackId h264TrackId;
	MP4TrackId aacTrackId;
	uint8 *thumbnail;
	float frameRateSum;
	int frameCount;
	int thumbnailDataLength;
	long lastH264TimeStamp;
	long lastAACTimeStamp;
	int previewWidth;
	int previewHeight;
	int cropX;
	int cropY;
	int cropWidth;
	int cropHeight;
	int cameraFacing;
	int cameraOrientation;

	void throwJavaException(JNIEnv* env, const char *name, const char *msg);
};

#ifdef __cplusplus
}
#endif

#endif /* MP4_H_ */
