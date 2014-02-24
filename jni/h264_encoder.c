#include <string.h>
#include <stdio.h>
#include <jni.h>
#include <android/log.h> 
#include "tools.h"
#include "openh264/codec/api/svc/codec_api.h"
#include "libyuv/include/libyuv.h"

#define DEBUG 1
#define MODUL_NAME "h264-encoder"

#if DEBUG
#define LOG(msg, args...) __android_log_print(ANDROID_LOG_DEBUG, MODUL_NAME, msg, ## args)
#else
#define LOG(msg, args...)
#endif

#define TAG "h264-encoding"

ISVCEncoder *encoder;
SFrameBSInfo info;
int src_width;
int src_height;
int dest_x;
int dest_y;
int dest_height;
int dest_width;
float frame_rate;
uint8_t *thumbnail;
int thumbnail_size;
FILE* outfile;

JNIEXPORT void JNICALL Java_co_splots_recorder_H264Encoder_init(JNIEnv* env,
		jobject thiz, jfloat frameRate, jint srcWidth, jint srcHeight,
		jobject outputRect, jstring outputPath) {
	jclass output_rect_class = (*env)->GetObjectClass(env, outputRect);
	jfieldID field_id = (*env)->GetFieldID(env, output_rect_class, "left", "I");
	dest_x = (*env)->GetIntField(env, outputRect, field_id);
	field_id = (*env)->GetFieldID(env, output_rect_class, "top", "I");
	dest_y = (*env)->GetIntField(env, outputRect, field_id);
	field_id = (*env)->GetFieldID(env, output_rect_class, "right", "I");
	dest_width = (*env)->GetIntField(env, outputRect, field_id) - dest_x;
	field_id = (*env)->GetFieldID(env, output_rect_class, "bottom", "I");
	dest_height = (*env)->GetIntField(env, outputRect, field_id) - dest_y;
	frame_rate = (float) frameRate;
	src_width = (int) srcWidth;
	src_height = (int) srcHeight;
	if (CreateSVCEncoder(&encoder) != cmResultSuccess) {
		throwJavaException(env, "java/io/IOException",
				"Couldn't initialize encoder.");
		return;
	}
	SEncParamBase *param = (SEncParamBase*) malloc(sizeof(SEncParamBase));
	memset(param, 0, sizeof(SEncParamBase));
	param->fMaxFrameRate = frame_rate;
	param->iInputCsp = videoFormatI420;
	param->iPicHeight = dest_width;
	param->iPicWidth = dest_height;
	param->iTargetBitrate = 500000;
	LOG("initializing....");
	if ((*encoder)->Initialize(encoder, param) != cmResultSuccess) {
		throwJavaException(env, "java/lang/IOException",
				"Couldn't initialize encoder.");
		return;
	}
	memset(&info, 0, sizeof(SFrameBSInfo));
	const char* output_file_path = (*env)->GetStringUTFChars(env, outputPath,
			(jboolean) 0);
	outfile = fopen(output_file_path, "wb");
	if (outfile == NULL ) {
		throwJavaException(env, "java/io/IOException",
				"Could not open the output file");
		return;
	}
	(*env)->ReleaseStringUTFChars(env, outputPath, output_file_path);
}

JNIEXPORT jbyteArray JNICALL Java_co_splots_recorder_H264Encoder_getThumbnail(
		JNIEnv* env, jobject thiz) {
	if (thumbnail == NULL )
		return NULL ;
	jbyteArray result = (*env)->NewByteArray(env, thumbnail_size);
	(*env)->SetByteArrayRegion(env, result, 0, thumbnail_size,
			(const jbyte *) thumbnail);
	return result;
}

JNIEXPORT jboolean JNICALL Java_co_splots_recorder_H264Encoder_encode(
		JNIEnv* env, jobject thiz, jbyteArray data, jobject cameraInfo) {
	jclass camera_info_class = (*env)->GetObjectClass(env, cameraInfo);
	jfieldID field_id = (*env)->GetFieldID(env, camera_info_class,
			"orientation", "I");
	jint rotation = (*env)->GetIntField(env, cameraInfo, field_id);
	field_id = (*env)->GetFieldID(env, camera_info_class, "facing", "I");
	jint facing = (*env)->GetIntField(env, cameraInfo, field_id);
	int nv21_length = (*env)->GetArrayLength(env, data);
	uint8_t *nv21_input = (uint8_t *) (*env)->GetByteArrayElements(env, data,
			(jboolean) 0);

	int width = src_width;
	int height = src_height;
	int rotate = rotation;

	int crop_size = (width < height) ? width : height;
	int converted_half_size = (crop_size + 1) / 2;
	uint8_t convert_i420[crop_size * crop_size + (crop_size * crop_size + 1) / 2];
	uint8_t *convert_i420_y = convert_i420;
	uint8_t *convert_i420_u = convert_i420 + (crop_size * crop_size);
	uint8_t *convert_i420_v = convert_i420_u
			+ (converted_half_size * converted_half_size);

	if (facing == 1) {
		height = 0 - height;
		rotate = (rotate + 180) % 360;
	}
	int res = ConvertToI420(nv21_input, nv21_length, convert_i420_y, crop_size,
			convert_i420_u, converted_half_size, convert_i420_v,
			converted_half_size, 0, 0, width, height, crop_size, crop_size,
			rotate, FOURCC_NV21);

	int scaled_half_size = (dest_width + 1) / 2;
	int scaled_data_length = dest_width * dest_width
			+ (dest_width * dest_width + 1) / 2;
	uint8_t scaled_i420[scaled_data_length];
	uint8_t* scaled_i420_y = scaled_i420;
	uint8_t* scaled_i420_u = scaled_i420 + (dest_width * dest_width);
	uint8_t* scaled_i420_v = scaled_i420_u
			+ (scaled_half_size * scaled_half_size);
	if (crop_size != dest_width)
		res = I420Scale(convert_i420_y, crop_size, convert_i420_u,
				converted_half_size, convert_i420_v, converted_half_size,
				crop_size, crop_size, scaled_i420_y, dest_width, scaled_i420_u,
				scaled_half_size, scaled_i420_v, scaled_half_size, dest_width,
				dest_width, kFilterNone);
	else {
		scaled_i420_y = convert_i420_y;
		scaled_i420_u = convert_i420_u;
		scaled_i420_v = convert_i420_v;
	}
	if (thumbnail == NULL ) {
		thumbnail_size = scaled_data_length;
		thumbnail = (uint8_t *) malloc(sizeof(uint8_t) * thumbnail_size);
		uint8_t* thumbnail_y = thumbnail;
		uint8_t* thumbnail_vu = thumbnail + (dest_width * dest_width);
		I420ToNV12(scaled_i420_y, dest_width, scaled_i420_v, scaled_half_size,
				scaled_i420_u, scaled_half_size, thumbnail_y, dest_width,
				thumbnail_vu, dest_width, dest_width, dest_width);
	}
	SSourcePicture pic;
	memset(&pic, 0, sizeof(SSourcePicture));
	pic.iPicWidth = dest_width;
	pic.iPicHeight = dest_width;
	pic.iColorFormat = videoFormatI420;
	pic.iStride[0] = pic.iPicWidth;
	pic.iStride[1] = pic.iStride[2] = pic.iPicWidth >> 1;
	pic.pData[0] = (uint8_t *) scaled_i420;
	pic.pData[1] = pic.pData[0] + dest_width * dest_width;
	pic.pData[2] = pic.pData[1] + (dest_width * dest_width >> 2);

	EVideoFrameType frameType = (*encoder)->EncodeFrame(encoder, &pic, &info);

	if (frameType != videoFrameTypeInvalid && frameType != videoFrameTypeSkip) {
		int i;
		for (i = 0; i < info.iLayerNum; ++i) {
			SLayerBSInfo layerInfo = info.sLayerInfo[i];
			int layerSize = 0;
			int j;
			for (j = 0; j < layerInfo.iNalCount; ++j)
				layerSize += layerInfo.iNalLengthInByte[j];
			fwrite(layerInfo.pBsBuf, 1, layerSize, outfile);
			LOG("written %d bytes.", layerSize);
		}
		return JNI_TRUE;
	}
	return JNI_FALSE;
}

JNIEXPORT void JNICALL Java_co_splots_recorder_H264Encoder_release(JNIEnv* env,
		jobject thiz) {
	if (outfile != NULL )
		fclose(outfile);
	if (encoder) {
		(*encoder)->Uninitialize(encoder);
		DestroySVCEncoder(encoder);
	}
}

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM * vm, void * reserved) {
	return JNI_VERSION_1_6;
}
