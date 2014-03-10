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
int crop_x;
int crop_y;
int crop_height;
int crop_width;
int dest_height;
int dest_width;
float frame_rate;
uint8_t *thumbnail;
int thumbnail_size;
int thumbnail_width;
int thumbnail_height;
FILE* outfile;

void writeInfoToFile(SFrameBSInfo *info) {
	int i;
	for (i = 0; i < info->iLayerNum; ++i) {
		SLayerBSInfo layerInfo = info->sLayerInfo[i];
		int layerSize = 0;
		int j;
		for (j = 0; j < layerInfo.iNalCount; ++j)
			layerSize += layerInfo.iNalLengthInByte[j];
		fwrite(layerInfo.pBsBuf, 1, layerSize, outfile);
		LOG("writing %d bytes...", layerSize);
	}
}

JNIEXPORT void JNICALL Java_co_splots_recorder_H264Encoder_init(JNIEnv* env,
		jobject thiz, jfloat frameRate, jint srcWidth, jint srcHeight,
		jobject cropRect, jint destWidth, jint destHeight, jstring outputPath) {
	jclass output_rect_class = (*env)->GetObjectClass(env, cropRect);
	jfieldID field_id = (*env)->GetFieldID(env, output_rect_class, "left", "I");
	crop_x = (*env)->GetIntField(env, cropRect, field_id);
	field_id = (*env)->GetFieldID(env, output_rect_class, "top", "I");
	crop_y = (*env)->GetIntField(env, cropRect, field_id);
	field_id = (*env)->GetFieldID(env, output_rect_class, "right", "I");
	crop_width = (*env)->GetIntField(env, cropRect, field_id) - crop_x;
	field_id = (*env)->GetFieldID(env, output_rect_class, "bottom", "I");
	crop_height = (*env)->GetIntField(env, cropRect, field_id) - crop_y;
	dest_width = (int) destWidth;
	dest_height = (int) destHeight;
	src_width = (int) srcWidth;
	src_height = (int) srcHeight;
	frame_rate = (float) frameRate;

	LOG(
			"Encoder initializing...\nCrop x:%d\t\tCrop y:%d\nCrop width:%d\t\tCrop height:%d\ndest width:%d\t\tdest height:%d\nsrc width:%d\t\tsrc height:%d",
			crop_x, crop_y, crop_width, crop_height, dest_width, dest_height,
			src_width, src_height);
	if (CreateSVCEncoder(&encoder) != cmResultSuccess) {
		throwJavaException(env, "java/io/IOException",
				"Couldn't create encoder.");
		return;
	}
	/*SEncParamExt sParam;
	 memset(&sParam, 0, sizeof(SEncParamExt));
	 (*encoder)->GetDefaultParams(encoder, &sParam);*/
	SEncParamBase sParam;
	memset(&sParam, 0, sizeof(SEncParamBase));
	sParam.fMaxFrameRate = frame_rate;
	sParam.iInputCsp = videoFormatI420;
	sParam.iPicWidth = dest_width;
	sParam.iPicHeight = dest_height;
	sParam.iTargetBitrate = 1000000;
	if ((*encoder)->Initialize(encoder, &sParam) != cmResultSuccess) {
		throwJavaException(env, "java/io/IOException",
				"Couldn't initialize encoder.");
		return;
	}
	memset(&info, 0, sizeof(SFrameBSInfo));
	const char* output_file_path = (*env)->GetStringUTFChars(env, outputPath,
			(jboolean) 0);
	outfile = fopen(output_file_path, "wb");
	if (outfile == NULL) {
		throwJavaException(env, "java/io/IOException",
				"Could not open the output file");
		return;
	}
	(*env)->ReleaseStringUTFChars(env, outputPath, output_file_path);
	//(*encoder)->EncodeParameterSets(encoder, &info);
	//writeInfoToFile(&info);
}

JNIEXPORT jbyteArray JNICALL Java_co_splots_recorder_H264Encoder_getThumbnailData(
		JNIEnv* env, jobject thiz) {
	if (thumbnail == NULL)
		return NULL ;
	jbyteArray result = (*env)->NewByteArray(env, thumbnail_size);
	(*env)->SetByteArrayRegion(env, result, 0, thumbnail_size,
			(const jbyte *) thumbnail);
	return result;
}

JNIEXPORT jint JNICALL Java_co_splots_recorder_H264Encoder_getThumbnailWidth(
		JNIEnv* env, jobject thiz) {
	return thumbnail == NULL ? 0 : thumbnail_width;
}

JNIEXPORT jint JNICALL Java_co_splots_recorder_H264Encoder_getThumbnailHeight(
		JNIEnv* env, jobject thiz) {
	return thumbnail == NULL ? 0 : thumbnail_height;
}

JNIEXPORT jboolean JNICALL Java_co_splots_recorder_H264Encoder_encode(
		JNIEnv* env, jobject thiz, jbyteArray data, jobject cameraInfo,
		jlong timeStamp) {
	jclass camera_info_class = (*env)->GetObjectClass(env, cameraInfo);
	jfieldID field_id = (*env)->GetFieldID(env, camera_info_class,
			"orientation", "I");
	jint rotation = (*env)->GetIntField(env, cameraInfo, field_id);
	field_id = (*env)->GetFieldID(env, camera_info_class, "facing", "I");
	jint facing = (*env)->GetIntField(env, cameraInfo, field_id);
	int nv21_length = (*env)->GetArrayLength(env, data);
	uint8_t *nv21_input = (uint8_t *) (*env)->GetByteArrayElements(env, data,
			(jboolean) 0);

	int width = crop_width;
	int height = crop_height;
	int rotate = rotation;

	int converted_half_size = (width + 1) / 2;
	uint8_t convert_i420[width * height + (width * height + 1) / 2];
	uint8_t *convert_i420_y = convert_i420;
	uint8_t *convert_i420_u = convert_i420 + (width * height);
	uint8_t *convert_i420_v = convert_i420_u
			+ (converted_half_size * converted_half_size);

	if (facing == 1) {
		height = 0 - height;
		rotate = (rotate + 180) % 360;
	}
	if (ConvertToI420(nv21_input, nv21_length, convert_i420_y, width,
			convert_i420_v, converted_half_size, convert_i420_u,
			converted_half_size, crop_x, crop_y, src_width, src_height, width,
			height, rotate, FOURCC_NV21) != 0)
		return JNI_FALSE;
	int scaled_half_size = (dest_width + 1) / 2;
	int scaled_data_length = dest_width * dest_height
			+ (dest_width * dest_height + 1) / 2;
	uint8_t scaled_i420[scaled_data_length];
	uint8_t* scaled_i420_y = scaled_i420;
	uint8_t* scaled_i420_u = scaled_i420 + (dest_width * dest_height);
	uint8_t* scaled_i420_v = scaled_i420_u
			+ (scaled_half_size * scaled_half_size);
	if (width != dest_width || height != dest_height) {
		if (I420Scale(convert_i420_y, width, convert_i420_u,
				converted_half_size, convert_i420_v, converted_half_size, width,
				height, scaled_i420_y, dest_width, scaled_i420_u,
				scaled_half_size, scaled_i420_v, scaled_half_size, dest_width,
				dest_height, kFilterNone) != 0)
			return JNI_FALSE;
	} else {
		scaled_i420_y = convert_i420_y;
		scaled_i420_u = convert_i420_u;
		scaled_i420_v = convert_i420_v;
	}
	uint8_t *frame = (uint8_t *) malloc(sizeof(uint8_t) * scaled_data_length);
	uint8_t *frame_y = frame;
	uint8_t* frame_vu = frame + (dest_width * dest_height);
	I420ToNV12(scaled_i420_y, dest_width, scaled_i420_u, scaled_half_size,
			scaled_i420_v, scaled_half_size, frame_y, dest_width, frame_vu,
			dest_width, dest_width, dest_height);
	if (thumbnail == NULL) {
		thumbnail_size = scaled_data_length;
		thumbnail_width = dest_width;
		thumbnail_height = dest_height;
		thumbnail = (uint8_t *) malloc(sizeof(uint8_t) * thumbnail_size);
		uint8_t* thumbnail_y = thumbnail;
		uint8_t* thumbnail_vu = thumbnail + (dest_width * dest_height);
		/*I420ToNV12(scaled_i420_y, dest_width, scaled_i420_v, scaled_half_size,
		 scaled_i420_u, scaled_half_size, thumbnail_y, dest_width,
		 thumbnail_vu, dest_width, dest_width, dest_height);*/
		memcpy(thumbnail, frame, thumbnail_size);
	}

	SSourcePicture pic;
	memset(&pic, 0, sizeof(SSourcePicture));
	pic.iPicWidth = dest_width;
	pic.iPicHeight = dest_width;
	pic.iColorFormat = videoFormatI420;
	pic.iStride[0] = dest_width;
	pic.iStride[1] = pic.iStride[2] = scaled_half_size;
	long long time_stamp = (long long) timeStamp;
	LOG("encoding frame timestamp=%llu", time_stamp);
	pic.uiTimeStamp = time_stamp;
	pic.pData[0] = frame;
	pic.pData[1] = pic.pData[0] + dest_width * dest_height;
	pic.pData[2] = pic.pData[1] + (scaled_half_size * scaled_half_size);

	EVideoFrameType frameType = (*encoder)->EncodeFrame(encoder, &pic, &info);
	if (frameType == videoFrameTypeInvalid)
		return JNI_FALSE;
	if (frameType != videoFrameTypeSkip)
		writeInfoToFile(&info);
	return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_co_splots_recorder_H264Encoder_release(JNIEnv* env,
		jobject thiz) {
	if (outfile != NULL)
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
