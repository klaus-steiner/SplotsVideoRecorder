/*
 * mp4_wrapper.cpp
 *
 *  Created on: 15.03.2014
 *      Author: klaus
 */

#include "mp4.h"

#ifdef __cplusplus
extern "C" {
#endif

Mp4Encoder *encoder;

JNIEXPORT void JNICALL Java_co_splots_recorder_Mp4Encoder_init(JNIEnv* env,
		jobject thiz, jstring outPath) {
	encoder = new Mp4Encoder();
	encoder->init(env, thiz, outPath);
}

JNIEXPORT void JNICALL Java_co_splots_recorder_Mp4Encoder_initVideo(JNIEnv* env,
		jobject thiz, jfloat frameRate, jint srcWidth, jint srcHeight,
		jobject cropRect, jint destWidth, jint destHeight) {
	encoder->initVideo(env, thiz, frameRate, srcWidth, srcHeight, cropRect,
			destWidth, destHeight);
}

JNIEXPORT void JNICALL Java_co_splots_recorder_Mp4Encoder_initAudio(JNIEnv* env,
		jobject thiz, jint bitrate, jint channels, jint sample_rate) {
	encoder->initAudio(env, thiz, bitrate, channels, sample_rate);
}

JNIEXPORT void JNICALL Java_co_splots_recorder_Mp4Encoder_setFrameRate(
		JNIEnv* env, jobject thiz, jfloat frameRate) {
	encoder->setFrameRate(env, thiz, frameRate);
}

JNIEXPORT jfloat JNICALL Java_co_splots_recorder_Mp4Encoder_getAverageFrameRate(
		JNIEnv* env, jobject thiz) {
	return encoder->getAverageFrameRate(env, thiz);
}

JNIEXPORT void JNICALL Java_co_splots_recorder_Mp4Encoder_setPreviewSize(
		JNIEnv* env, jobject thiz, jint srcWidth, jint srcHeight) {
	encoder->setPreviewSize(env, thiz, srcWidth, srcHeight);
}

JNIEXPORT void JNICALL Java_co_splots_recorder_Mp4Encoder_setVideoBitrate(
		JNIEnv* env, jobject thiz, jint srcWidth, jint bitRate) {
	encoder->setVideoBitrate(env, thiz, bitRate);
}

JNIEXPORT void JNICALL Java_co_splots_recorder_Mp4Encoder_setCropRect(
		JNIEnv* env, jobject thiz, jobject cropRect) {
	encoder->setCropRect(env, thiz, cropRect);
}

JNIEXPORT void JNICALL Java_co_splots_recorder_Mp4Encoder_setCameraInfo(
		JNIEnv* env, jobject thiz, jobject cameraInfo) {
	encoder->setCameraInfo(env, thiz, cameraInfo);
}

JNIEXPORT jboolean JNICALL Java_co_splots_recorder_Mp4Encoder_encodePreviewFrame(
		JNIEnv* env, jobject thiz, jbyteArray preview, jlong timeStamp) {
	return encoder->encodePreviewFrame(env, thiz, preview, timeStamp);
}

JNIEXPORT jboolean JNICALL Java_co_splots_recorder_Mp4Encoder_encodeAudioSample(
		JNIEnv* env, jobject thiz, jbyteArray audio) {
	return encoder->encodeAudioSample(env, thiz, audio);
}

JNIEXPORT jbyteArray JNICALL Java_co_splots_recorder_Mp4Encoder_getThumbnailData(
		JNIEnv* env, jobject thiz) {
	return encoder->getThumbnailData(env, thiz);
}

/*JNIEXPORT jbyteArray JNICALL Java_co_splots_recorder_Mp4Encoder_getThumbnailData(
 JNIEnv* env, jobject thiz, jint width, jint height) {
 return encoder->getThumbnailData(env, thiz, width, height);
 }*/

JNIEXPORT jint JNICALL Java_co_splots_recorder_Mp4Encoder_getThumbnailWidth(
		JNIEnv* env, jobject thiz) {
	return encoder->getThumbnailWidth(env, thiz);
}

JNIEXPORT jint JNICALL Java_co_splots_recorder_Mp4Encoder_getThumbnailHeight(
		JNIEnv* env, jobject thiz) {
	return encoder->getThumbnailHeight(env, thiz);
}

JNIEXPORT void JNICALL Java_co_splots_recorder_Mp4Encoder_release(JNIEnv* env,
		jobject thiz) {
	encoder->release(env, thiz);
}

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM * vm, void * reserved) {
	return JNI_VERSION_1_6;
}

#ifdef __cplusplus
}
#endif
