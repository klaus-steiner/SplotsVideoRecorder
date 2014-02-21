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

#define EPSN	  (0.000001f) // (1e-6)	// desired float precision
#define TAG "h264-encoding"

ISVCEncoder *encoder;
SFrameBSInfo info;
int srcWidth;
int srcHeight;
int destSize;
float frameRate;
uint8_t *thumbnail;
int thumbnailLenght;
FILE* outfile;

void FillSpecificParameters(SEncParamExt *sParam) {
	/* Test for temporal, spatial, SNR scalability
	 sParam->fMaxFrameRate = frameRate;		// input frame rate
	 sParam->iPicWidth = destSize;		// width of picture in samples
	 sParam->iPicHeight = destSize;		// height of picture in samples
	 sParam->iTargetBitrate = 500000;		// target bitrate desired
	 sParam->iInputCsp = videoFormatI420;
	 sParam->iTemporalLayerNum = 3;		// layer number at temporal level
	 sParam->iSpatialLayerNum = 4;		// layer number at spatial level
	 sParam->uiFrameToBeCoded = -1;
	 sParam->iLtrMarkPeriod = 30;
	 sParam->bEnableSpsPpsIdAddition = 0;
	 sParam->bEnableFrameCroppingFlag = 0;
	 sParam->iLoopFilterDisableIdc = 0;
	 sParam->iInterLayerLoopFilterDisableIdc = 0;
	 sParam->iInterLayerLoopFilterAlphaC0Offset = 0;
	 sParam->iLoopFilterBetaOffset = 0;
	 sParam->iLoopFilterAlphaC0Offset = 0;
	 sParam->iMultipleThreadIdc = 0;
	 sParam->bEnableRc = 0;
	 sParam->bEnableFrameSkip = 0;
	 sParam->bEnableDenoise = 1;
	 sParam->bEnableSceneChangeDetect = 0;
	 sParam->bEnableBackgroundDetection = 0;
	 sParam->bEnableAdaptiveQuant = 0;
	 sParam->bEnableLongTermReference = 1;
	 sParam->uiIntraPeriod = 0;
	 sParam->bPrefixNalAddingCtrl = 1;

	 int iIndexLayer = 0;
	 sParam->sSpatialLayers[iIndexLayer].iVideoWidth = destSize;
	 sParam->sSpatialLayers[iIndexLayer].iVideoHeight = destSize;
	 sParam->sSpatialLayers[iIndexLayer].uiProfileIdc = 66;
	 sParam->sSpatialLayers[iIndexLayer].fFrameRate = frameRate;
	 sParam->sSpatialLayers[iIndexLayer].iSpatialBitrate = 500000;
	 sParam->sSpatialLayers[iIndexLayer].iInterSpatialLayerPredFlag = 0;
	 sParam->sSpatialLayers[iIndexLayer].sSliceCfg.uiSliceMode = 0;*/

	/* Test for temporal, spatial, SNR scalability */
	sParam->fMaxFrameRate = frameRate;		// input frame rate
	sParam->iPicWidth = destSize;			// width of picture in samples
	sParam->iPicHeight = destSize;			// height of picture in samples
	sParam->iTargetBitrate = 512000;		// target bitrate desired
	sParam->iRCMode = 0;            //  rc mode control
	sParam->iTemporalLayerNum = 3;	// layer number at temporal level
	sParam->iSpatialLayerNum = 2;	// layer number at spatial level
	sParam->bEnableDenoise = 0;    // denoise control
	sParam->bEnableBackgroundDetection = 1; // background detection control
	sParam->bEnableAdaptiveQuant = 1; // adaptive quantization control
	sParam->bEnableFrameSkip = 1; // frame skipping
	sParam->bEnableLongTermReference = 0; // long term reference control
	sParam->iLtrMarkPeriod = 30;

	sParam->iInputCsp = videoFormatI420;		// color space of input sequence
	sParam->uiIntraPeriod = 320;		// period of Intra frame
	sParam->bEnableSpsPpsIdAddition = 1;
	sParam->bPrefixNalAddingCtrl = 1;

	int iIndexLayer = 0;
	sParam->sSpatialLayers[iIndexLayer].iVideoWidth = 160;
	sParam->sSpatialLayers[iIndexLayer].iVideoHeight = 160;
	sParam->sSpatialLayers[iIndexLayer].fFrameRate = 7.5f;
	sParam->sSpatialLayers[iIndexLayer].iSpatialBitrate = 64000;
	sParam->sSpatialLayers[iIndexLayer].iInterSpatialLayerPredFlag = 0;
	sParam->sSpatialLayers[iIndexLayer].sSliceCfg.uiSliceMode = 0;
	sParam->sSpatialLayers[iIndexLayer].sSliceCfg.sSliceArgument.uiSliceNum = 1;

	++iIndexLayer;
	sParam->sSpatialLayers[iIndexLayer].iVideoWidth = 320;
	sParam->sSpatialLayers[iIndexLayer].iVideoHeight = 320;
	sParam->sSpatialLayers[iIndexLayer].fFrameRate = 15.0f;
	sParam->sSpatialLayers[iIndexLayer].iSpatialBitrate = 160000;
	sParam->sSpatialLayers[iIndexLayer].iInterSpatialLayerPredFlag = 0;
	sParam->sSpatialLayers[iIndexLayer].sSliceCfg.uiSliceMode = 0;
	sParam->sSpatialLayers[iIndexLayer].sSliceCfg.sSliceArgument.uiSliceNum = 1;

	float fMaxFr =
			sParam->sSpatialLayers[sParam->iSpatialLayerNum - 1].fFrameRate;
	int32_t i = 0;
	for (i = sParam->iSpatialLayerNum - 2; i >= 0; --i) {
		if (sParam->sSpatialLayers[i].fFrameRate > fMaxFr + EPSN)
			fMaxFr = sParam->sSpatialLayers[i].fFrameRate;
	}
	sParam->fMaxFrameRate = fMaxFr;
}

/**
 * initialize h264 encoder
 **/JNIEXPORT void JNICALL Java_co_splots_recorder_H264Encoder_init(JNIEnv* env,
		jobject thiz, jfloat frame_rate, jint src_width, jint src_height,
		jint dest_size, jstring output_path) {
	frameRate = (float) frame_rate;
	srcWidth = src_width;
	srcHeight = src_height;
	destSize = dest_size;
	if (CreateSVCEncoder(&encoder) != cmResultSuccess) {
		throwJavaException(env, "java/lang/IOException",
				"Couldn't initialize encoder.");
		return;
	}
	SEncParamExt *param = (SEncParamExt*) malloc(sizeof(SEncParamExt));
	memset(param, 0, sizeof(SEncParamExt));
	/*SEncParamBase *param = (SEncParamBase*) malloc(sizeof(SEncParamBase));
	 memset(param, 0, sizeof(SEncParamBase));
	 param->fMaxFrameRate = frameRate;
	 param->iInputCsp = videoFormatI420;
	 param->iPicHeight = dest_size;
	 param->iPicWidth = dest_size;
	 param->iTargetBitrate = 500000;*/
	FillSpecificParameters(param);
	if ((*encoder)->InitializeExt(encoder, param) != cmResultSuccess) {
		throwJavaException(env, "java/lang/IOException",
				"Couldn't initialize encoder.");
		return;
	}
	memset(&info, 0, sizeof(SFrameBSInfo));
	const char* output_file = (*env)->GetStringUTFChars(env, output_path,
			(jboolean) 0);
	outfile = fopen(output_file, "wb");
	if (outfile == NULL) {
		throwJavaException(env, "java/lang/IOException",
				"Could not open the output file.");
		return;
	}
	LOG("voll der test. writing to %s", output_file);
	(*env)->ReleaseStringUTFChars(env, output_path, output_file);
}

JNIEXPORT jbyteArray JNICALL Java_co_splots_recorder_H264Encoder_getThumbnail(
		JNIEnv* env, jobject thiz) {
	if (thumbnail == NULL)
		return NULL ;
	jbyteArray result = (*env)->NewByteArray(env, thumbnailLenght);
	(*env)->SetByteArrayRegion(env, result, 0, thumbnailLenght,
			(const jbyte *) thumbnail);
	return result;
}

JNIEXPORT jboolean JNICALL Java_co_splots_recorder_H264Encoder_encode(
		JNIEnv* env, jobject thiz, jbyteArray data, jobject cameraInfo) {
	jclass cameraInfoClass = (*env)->GetObjectClass(env, cameraInfo);
	jfieldID fieldId = (*env)->GetFieldID(env, cameraInfoClass, "orientation",
			"I");
	jint rotation = (*env)->GetIntField(env, cameraInfo, fieldId);
	fieldId = (*env)->GetFieldID(env, cameraInfoClass, "facing", "I");
	jint facing = (*env)->GetIntField(env, cameraInfo, fieldId);
	int dataLength = (*env)->GetArrayLength(env, data);
	uint8_t *raw = (uint8_t *) (*env)->GetByteArrayElements(env, data,
			(jboolean) 0);

	int width = srcWidth;
	int height = srcHeight;
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
	int res = ConvertToI420(raw, dataLength, convert_i420_y, crop_size,
			convert_i420_u, converted_half_size, convert_i420_v,
			converted_half_size, 0, 0, width, height, crop_size, crop_size,
			rotate, FOURCC_NV21);

	int scaled_half_size = (destSize + 1) / 2;
	int scaled_data_length = destSize * destSize
			+ (destSize * destSize + 1) / 2;
	uint8_t scaled_i420[scaled_data_length];
	uint8_t* scaled_i420_y = scaled_i420;
	uint8_t* scaled_i420_u = scaled_i420 + (destSize * destSize);
	uint8_t* scaled_i420_v = scaled_i420_u
			+ (scaled_half_size * scaled_half_size);
	if (crop_size != destSize)
		res = I420Scale(convert_i420_y, crop_size, convert_i420_u,
				converted_half_size, convert_i420_v, converted_half_size,
				crop_size, crop_size, scaled_i420_y, destSize, scaled_i420_u,
				scaled_half_size, scaled_i420_v, scaled_half_size, destSize,
				destSize, kFilterNone);
	else {
		scaled_i420_y = convert_i420_y;
		scaled_i420_u = convert_i420_u;
		scaled_i420_v = convert_i420_v;
	}
	if (thumbnail == NULL) {
		thumbnailLenght = scaled_data_length;
		thumbnail = (uint8_t *) malloc(sizeof(uint8_t) * thumbnailLenght);
		uint8_t* thumbnail_y = thumbnail;
		uint8_t* thumbnail_vu = thumbnail + (destSize * destSize);
		I420ToNV12(scaled_i420_y, destSize, scaled_i420_v, scaled_half_size,
				scaled_i420_u, scaled_half_size, thumbnail_y, destSize,
				thumbnail_vu, destSize, destSize, destSize);
	}
	SSourcePicture pic;
	memset(&pic, 0, sizeof(SSourcePicture));
	pic.iPicWidth = destSize;
	pic.iPicHeight = destSize;
	pic.iColorFormat = videoFormatI420;
	pic.iStride[0] = pic.iPicWidth;
	pic.iStride[1] = pic.iStride[2] = pic.iPicWidth >> 1;
	pic.pData[0] = (uint8_t *) scaled_i420;
	pic.pData[1] = pic.pData[0] + destSize * destSize;
	pic.pData[2] = pic.pData[1] + (destSize * destSize >> 2);

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
