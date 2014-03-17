/*
 * mp4.cpp
 *
 *  Created on: 15.03.2014
 *  Author: Klaus Steiner
 */

#include "mp4.h"

#define DEBUG 1
#define MODUL_NAME "mp4-muxer"

#if DEBUG
#define LOG(msg, args...) __android_log_print(ANDROID_LOG_DEBUG, MODUL_NAME, msg, ## args)
#else
#define LOG(msg, args...)
#endif

#define TAG "mp4-muxer"

Mp4Encoder::Mp4Encoder() {
	aacEncoder = new VO_AUDIO_CODECAPI;
	memset(aacEncoder, 0, sizeof(VO_AUDIO_CODECAPI));
	mp4FileHandler = NULL;
	h264Encoder = NULL;
	videoParameter = new SEncParamBase;
	memset(videoParameter, 0, sizeof(SEncParamBase));
	videoParameter->iTargetBitrate = 5000000;
	h264EncoderInfo = new SFrameBSInfo;
	aacMemoryOperator = new VO_MEM_OPERATOR;
	memset(aacMemoryOperator, 0, sizeof(VO_MEM_OPERATOR));
	aacUserData = new VO_CODEC_INIT_USERDATA;
	memset(aacUserData, 0, sizeof(VO_CODEC_INIT_USERDATA));
	aacHandle = 0;
	audioParameter = new AACENC_PARAM;
	memset(audioParameter, 0, sizeof(AACENC_PARAM));
	lastH264TimeStamp = 0;
	lastAACTimeStamp = 0;
	aacTrackId = MP4_INVALID_TRACK_ID;
	h264TrackId = MP4_INVALID_TRACK_ID;
	cropX = 0;
	cropY = 0;
	cropWidth = 0;
	cropHeight = 0;
	previewWidth = 0;
	previewHeight = 0;
	cameraOrientation = 0;
	cameraFacing = 0;
	thumbnail = NULL;
	thumbnailDataLength = 0;
	frameRateSum = 0;
	frameCount = 1;
}

Mp4Encoder::~Mp4Encoder() {
	delete aacEncoder;
	delete h264Encoder;
	delete videoParameter;
	delete h264EncoderInfo;
	delete aacMemoryOperator;
	delete aacUserData;
	delete audioParameter;
	delete thumbnail;
}

void Mp4Encoder::throwJavaException(JNIEnv* env, const char *name,
		const char *msg) {
	jclass cls = env->FindClass(name);
	if (cls != NULL)
		env->ThrowNew(cls, msg);
	env->DeleteLocalRef(cls);
}

void Mp4Encoder::init(JNIEnv* env, jobject thiz, jstring outPath) {
	const char* outputPath = env->GetStringUTFChars(outPath, (jboolean *) 0);
	mp4FileHandler = MP4Create(outputPath);
	if (mp4FileHandler == MP4_INVALID_FILE_HANDLE ) {
		throwJavaException(env, "java/lang/IllegalArgumentException",
				"Could not create mp4 file handler.");
		return;
	}
	env->ReleaseStringUTFChars(outPath, outputPath);
	if (!(MP4SetTimeScale(mp4FileHandler, 90000))) {
		throwJavaException(env, "java/lang/IllegalArgumentException",
				"Could not set time scale.");
		return;
	}
}

void Mp4Encoder::initVideo(JNIEnv* env, jobject thiz, jfloat frameRate,
		jint srcWidth, jint srcHeight, jobject cropRect, jint destWidth,
		jint destHeight) {
	setCropRect(env, thiz, cropRect);
	setFrameRate(env, thiz, frameRate);
	setPreviewSize(env, thiz, srcWidth, srcHeight);
	videoParameter->iPicWidth = (int) destWidth;
	videoParameter->iPicHeight = (int) destHeight;
	videoParameter->iInputCsp = videoFormatI420;
	videoParameter->iRCMode = 0;
	lastH264TimeStamp = 0;
	lastAACTimeStamp = 0;
	frameCount = 1;
	frameRateSum = 0;

	if (CreateSVCEncoder(&h264Encoder) != cmResultSuccess) {
		throwJavaException(env, "java/io/IOException",
				"Couldn't create encoder.");
		return;
	}

	if (h264Encoder->Initialize(videoParameter) != cmResultSuccess) {
		throwJavaException(env, "java/io/IOException",
				"Couldn't initialize encoder.");
		return;
	}
	MP4SetVideoProfileLevel(mp4FileHandler, 0x7F);
	h264Encoder->EncodeParameterSets(h264EncoderInfo);
	SLayerBSInfo layerInfo = h264EncoderInfo->sLayerInfo[0];
	int layerSize = 0;
	for (int j = 0; j < layerInfo.iNalCount; ++j)
		layerSize += layerInfo.iNalLengthInByte[j];
	initH264Track(layerInfo.pBsBuf, layerSize);
	if (h264TrackId == MP4_INVALID_TRACK_ID ) {
		throwJavaException(env, "java/lang/IllegalArgumentException",
				"Couldn't create h264 track.");
		return;
	}
}

int Mp4Encoder::parseSet(unsigned char *input, int inputLength,
		unsigned char **set, int offset) {
	int idx = offset;
	int setIdx = 0;
	unsigned char *temp = (unsigned char *) malloc(
			sizeof(unsigned char) * (inputLength - offset));
	while (idx <= inputLength) {
		if (isNALU(input, inputLength, idx) || idx == inputLength) {
			*set = (unsigned char *) malloc(sizeof(unsigned char) * setIdx);
			memcpy(*set, temp, setIdx);
			break;
		} else {
			temp[setIdx] = input[idx];
			idx++;
			setIdx++;
		}
	}
	return setIdx;
}

void Mp4Encoder::initH264Track(unsigned char *input, int inputSize) {
	initH264Track(input, inputSize, 0);
}

int Mp4Encoder::initH264Track(unsigned char *input, int inputSize, int offset) {
	int idx = offset;
	int parsedBytes = 0;
	unsigned char *sps = (unsigned char *) malloc(sizeof(unsigned char *));
	unsigned char *pps = (unsigned char *) malloc(sizeof(unsigned char *));
	while (idx < inputSize) {
		switch (input[idx]) {
		case 0x00: //identifing nalu
			if (isNALU(input, inputSize, idx))
				idx += initH264Track(input, inputSize, idx + 4);
			else
				idx++;
			break;
		case 0x67: //sps
			//idx++;
			parsedBytes = parseSet(input, inputSize, &sps, idx);
			/*LOG("sps:");
			 for (int i = 0; i < parsedBytes; i++)
			 LOG("0x%02X", sps[i]);*/
			h264TrackId = MP4AddH264VideoTrack(mp4FileHandler, 90000,
					MP4_INVALID_DURATION, videoParameter->iPicWidth,
					videoParameter->iPicHeight, sps[1], sps[2], sps[3], 3);
			MP4AddH264SequenceParameterSet(mp4FileHandler, h264TrackId, sps,
					parsedBytes);
			idx += parsedBytes + 1;
			break;
		case 0x68: //pps
			//idx++;
			parsedBytes = parseSet(input, inputSize, &pps, idx);
			MP4AddH264PictureParameterSet(mp4FileHandler, h264TrackId, pps,
					parsedBytes);
			idx += parsedBytes + 1;
			break;
		default:
			idx++;
			break;
		}
	}
	return idx;
}

bool Mp4Encoder::isNALU(unsigned char *input, int inputLength, int offset) {
	return inputLength - offset >= 4 && input[offset] == 0x00
			&& input[offset + 1] == 0x00 && input[offset + 2] == 0x00
			&& input[offset + 3] == 0x01;
}

void Mp4Encoder::initAudio(JNIEnv* env, jobject thiz, jint bitrate,
		jint channels, jint sampleRate) {
	voGetAACEncAPI(aacEncoder);
	aacMemoryOperator->Alloc = cmnMemAlloc;
	aacMemoryOperator->Copy = cmnMemCopy;
	aacMemoryOperator->Free = cmnMemFree;
	aacMemoryOperator->Set = cmnMemSet;
	aacMemoryOperator->Check = cmnMemCheck;
	aacUserData->memflag = VO_IMF_USERMEMOPERATOR;
	aacUserData->memData = (VO_PTR) aacMemoryOperator;
	if (aacEncoder->Init(&aacHandle, VO_AUDIO_CodingAAC,
			aacUserData) != VO_ERR_NONE) {
		throwJavaException(env, "java/lang/IllegalArgumentException",
				"Could not init aac encoder.");
		return;
	}

	audioParameter->sampleRate = sampleRate;
	audioParameter->bitRate = bitrate;
	audioParameter->nChannels = channels;
	audioParameter->adtsUsed = 1;

	if (aacEncoder->SetParam(aacHandle, VO_PID_AAC_ENCPARAM,
			audioParameter) != VO_ERR_NONE) {
		throwJavaException(env, "java/lang/IllegalArgumentException",
				"Unable to set encoding parameters.");
		return;
	}
	aacTrackId = MP4AddAudioTrack(mp4FileHandler, sampleRate, 1024);
	if (aacTrackId == MP4_INVALID_TRACK_ID ) {
		throwJavaException(env, "java/lang/IllegalArgumentException",
				"Couldn't create aac track.");
		return;
	}
	MP4SetTrackIntegerProperty(mp4FileHandler, aacTrackId,
			"mdia.minf.stbl.stsd.mp4a.channels", 1);
	MP4SetTrackIntegerProperty(mp4FileHandler, aacTrackId,
			"mdia.minf.stbl.stsd.mp4a.sampleSize", 16);
	MP4SetAudioProfileLevel(mp4FileHandler, 0x0F);
}

void Mp4Encoder::updateH264EncoderOptions() {
	if (h264Encoder != NULL && h264TrackId != MP4_INVALID_TRACK_ID )
		h264Encoder->SetOption(ENCODER_OPTION_SVC_ENCODE_PARAM_BASE,
				videoParameter);
}

void Mp4Encoder::setFrameRate(JNIEnv* env, jobject thiz, jfloat frameRate) {
	videoParameter->fMaxFrameRate = (float) frameRate;
	updateH264EncoderOptions();
}

jfloat Mp4Encoder::getAverageFrameRate(JNIEnv* env, jobject thiz) {
	return frameRateSum / ((float) frameCount);
}

void Mp4Encoder::setPreviewSize(JNIEnv* env, jobject thiz, jint srcWidth,
		jint srcHeight) {
	previewWidth = (int) srcWidth;
	previewHeight = (int) srcHeight;
}

void Mp4Encoder::setVideoBitrate(JNIEnv* env, jobject thiz, jint bitRate) {
	videoParameter->iTargetBitrate = (int) bitRate;
	updateH264EncoderOptions();
}

void Mp4Encoder::setCropRect(JNIEnv* env, jobject thiz, jobject cropRect) {
	jclass outputRectClass = env->GetObjectClass(cropRect);
	jfieldID fieldId = env->GetFieldID(outputRectClass, "left", "I");
	cropX = env->GetIntField(cropRect, fieldId);
	fieldId = env->GetFieldID(outputRectClass, "top", "I");
	cropY = env->GetIntField(cropRect, fieldId);
	fieldId = env->GetFieldID(outputRectClass, "right", "I");
	cropWidth = env->GetIntField(cropRect, fieldId) - cropX;
	fieldId = env->GetFieldID(outputRectClass, "bottom", "I");
	cropHeight = env->GetIntField(cropRect, fieldId) - cropY;
}

void Mp4Encoder::setCameraInfo(JNIEnv* env, jobject thiz, jobject cameraInfo) {
	jclass cameraInfoClass = env->GetObjectClass(cameraInfo);
	jfieldID fieldId = env->GetFieldID(cameraInfoClass, "orientation", "I");
	cameraOrientation = (int) env->GetIntField(cameraInfo, fieldId);
	fieldId = env->GetFieldID(cameraInfoClass, "facing", "I");
	cameraFacing = (int) env->GetIntField(cameraInfo, fieldId);
}

jboolean Mp4Encoder::encodePreviewFrame(JNIEnv* env, jobject thiz,
		jbyteArray preview, jlong timeStamp) {
	if (h264TrackId == MP4_INVALID_TRACK_ID ) {
		throwJavaException(env, "java/lang/IllegalStateException",
				"video not initialized.");
		return JNI_FALSE;
	}
	size_t nv21Length = env->GetArrayLength(preview);
	uint8_t *nv21Input = (uint8_t *) env->GetByteArrayElements(preview,
			(jboolean *) 0);

	int width = cropWidth;
	int height = cropHeight;
	int rotate = cameraOrientation;

	int convertedHalfSize = (width + 1) / 2;
	uint8 convertI420[width * height + (width * height + 1) / 2];
	uint8 *convertI420y = convertI420;
	uint8 *convertI420u = convertI420 + (width * height);
	uint8 *convertI420v = convertI420u
			+ (convertedHalfSize * convertedHalfSize);

	libyuv::RotationMode rotationMode;
	if (cameraFacing == 1) {
		height = 0 - height;
		rotate = (rotate + 180) % 360;
	}
	switch (rotate) {
	case 0:
		rotationMode = libyuv::kRotate0;
		break;
	case 90:
		rotationMode = libyuv::kRotate90;
		break;
	case 180:
		rotationMode = libyuv::kRotate180;
		break;
	case 270:
		rotationMode = libyuv::kRotate270;
		break;
	default:
		rotationMode = libyuv::kRotate0;
		break;
	}
	if (libyuv::ConvertToI420(nv21Input, nv21Length, convertI420y, width,
			convertI420v, convertedHalfSize, convertI420u, convertedHalfSize,
			cropX, cropY, previewWidth, previewHeight, width, height,
			rotationMode, libyuv::FOURCC_NV21) != 0)
		return JNI_FALSE;
	int scaledHalfSize = (videoParameter->iPicWidth + 1) / 2;
	int scaledDataLength = videoParameter->iPicWidth
			* videoParameter->iPicHeight
			+ (videoParameter->iPicWidth * videoParameter->iPicHeight + 1) / 2;
	uint8 scaledI420[scaledDataLength];
	uint8* scaledI420y = scaledI420;
	uint8* scaledI420u = scaledI420
			+ (videoParameter->iPicWidth * videoParameter->iPicHeight);
	uint8* scaledI420v = scaledI420u + (scaledHalfSize * scaledHalfSize);
	if (width != videoParameter->iPicWidth
			|| height != videoParameter->iPicHeight) {
		if (libyuv::I420Scale(convertI420y, width, convertI420u,
				convertedHalfSize, convertI420v, convertedHalfSize, width,
				height, scaledI420y, videoParameter->iPicWidth, scaledI420u,
				scaledHalfSize, scaledI420v, scaledHalfSize,
				videoParameter->iPicWidth, videoParameter->iPicHeight,
				libyuv::kFilterNone) != 0)
			return JNI_FALSE;
	} else {
		scaledI420y = convertI420y;
		scaledI420u = convertI420u;
		scaledI420v = convertI420v;
	}
	uint8 *frame = (uint8 *) malloc(sizeof(uint8) * scaledDataLength);
	uint8 *frameY = frame;
	uint8 *frameU = frameY
			+ (videoParameter->iPicWidth * videoParameter->iPicHeight);
	uint8 *frameV = frameU + (scaledHalfSize * scaledHalfSize);
	memcpy(frameY, scaledI420y,
			videoParameter->iPicWidth * videoParameter->iPicHeight);
	memcpy(frameU, scaledI420v, scaledHalfSize * scaledHalfSize);
	memcpy(frameV, scaledI420u, scaledHalfSize * scaledHalfSize);
	if (thumbnail == NULL) {
		thumbnailDataLength = scaledDataLength;
		thumbnail = (uint8 *) malloc(sizeof(uint8) * thumbnailDataLength);
		uint8* thumbnailY = thumbnail;
		uint8* thumbnailVU = thumbnail
				+ (videoParameter->iPicWidth * videoParameter->iPicHeight);
		libyuv::I420ToNV12(scaledI420y, videoParameter->iPicWidth, scaledI420u,
				scaledHalfSize, scaledI420v, scaledHalfSize, thumbnailY,
				videoParameter->iPicWidth, thumbnailVU,
				videoParameter->iPicWidth, videoParameter->iPicWidth,
				videoParameter->iPicHeight);
	}
	SSourcePicture pic;
	memset(&pic, 0, sizeof(SSourcePicture));
	pic.iPicWidth = videoParameter->iPicWidth;
	pic.iPicHeight = videoParameter->iPicHeight;
	pic.iColorFormat = videoFormatI420;
	pic.iStride[0] = videoParameter->iPicWidth;
	pic.iStride[1] = pic.iStride[2] = scaledHalfSize;
	pic.uiTimeStamp = (long long) timeStamp;
	pic.pData[0] = frame;
	pic.pData[1] = pic.pData[0]
			+ videoParameter->iPicWidth * videoParameter->iPicHeight;
	pic.pData[2] = pic.pData[1] + (scaledHalfSize * scaledHalfSize);

	float currentFrameRate = ((float) frameCount * 1000) / ((float) timeStamp);
	frameRateSum += currentFrameRate;
	LOG("H264: current fps: %f", currentFrameRate);
	int frameType = h264Encoder->EncodeFrame(&pic, h264EncoderInfo);
	if (frameType == videoFrameTypeInvalid)
		return JNI_FALSE;
	frameCount++;
	if (frameType != videoFrameTypeSkip) {
		for (int i = 0; i < h264EncoderInfo->iLayerNum; ++i) {
			SLayerBSInfo layerInfo = h264EncoderInfo->sLayerInfo[i];
			int layerSize = 0;
			for (int j = 0; j < layerInfo.iNalCount; ++j)
				layerSize += layerInfo.iNalLengthInByte[j];
			long duration = timeStamp - lastH264TimeStamp;
			lastH264TimeStamp = timeStamp;
			int sync = 0;
			uint32_t dflags = 0;
			switch (frameType) {
			case videoFrameTypeIDR:
				sync = 1;
				break;
			case videoFrameTypeI:
				dflags |= MP4_SDT_EARLIER_DISPLAY_TIMES_ALLOWED;
				break;
			case videoFrameTypeP:
				dflags |= MP4_SDT_EARLIER_DISPLAY_TIMES_ALLOWED;
				break;
			case videoFrameTypeIPMixed:
			default:
				break; /* nothing to mark */
			}
			if (!MP4WriteSampleDependency(mp4FileHandler, h264TrackId,
					layerInfo.pBsBuf, layerSize, 90 * duration, 0, sync,
					dflags))
				return JNI_FALSE;
			/*if (!MP4WriteSample(mp4FileHandler, h264TrackId, layerInfo.pBsBuf,
			 layerSize, 90 * duration, 0, true))
			 return JNI_FALSE;*/
		}
	}
	return JNI_TRUE;
}

jboolean Mp4Encoder::encodeAudioSample(JNIEnv* env, jobject thiz,
		jbyteArray audio) {
	if (aacTrackId == MP4_INVALID_TRACK_ID ) {
		throwJavaException(env, "java/lang/IllegalStateException",
				"audio not initialized.");
		return JNI_FALSE;
	}
	jbyte* buffer = env->GetByteArrayElements(audio, (jboolean *) 0);
	int inputSize = env->GetArrayLength(audio);

	VO_CODECBUFFER input;
	VO_CODECBUFFER output;
	VO_AUDIO_OUTPUTINFO outputInfo;

	int readSize = audioParameter->nChannels * 4 * 1024;
	unsigned char outputBuffer[readSize];

	int status = 0;
	int count = 0;
	bool success = true;
	do {
		int readBytes = count * readSize;
		input.Buffer = (unsigned char *) buffer + count * readSize;
		int bytesLeft = inputSize - count * readSize;
		input.Length = bytesLeft >= readSize ? readSize : bytesLeft;
		output.Buffer = outputBuffer;
		output.Length = readSize;
		status = aacEncoder->SetInputData(aacHandle, &input);
		if (status == VO_ERR_INPUT_BUFFER_SMALL)
			LOG("AAC: input buffer small read_bytes=%d", readBytes);
		do {
			output.Buffer = outputBuffer;
			output.Length = readSize;
			status = aacEncoder->GetOutputData(aacHandle, &output, &outputInfo);
			if (status == VO_ERR_LICENSE_ERROR)
				break;
			else if (status == VO_ERR_OUTPUT_BUFFER_SMALL)
				LOG("AAC: output buffer small used_bytes=%d",
						(int ) outputInfo.InputUsed);
			else if (status == VO_ERR_NONE) {
				if (!MP4WriteSample(mp4FileHandler, aacTrackId, output.Buffer,
						output.Length, MP4_INVALID_DURATION ))
					success = false;
			}
		} while (status != VO_ERR_INPUT_BUFFER_SMALL);
		if (status == VO_ERR_LICENSE_ERROR)
			break;
		count++;
	} while (count * readSize < inputSize && status);
	if (!success)
		return JNI_FALSE;
	env->ReleaseByteArrayElements(audio, buffer, JNI_ABORT);
	return JNI_TRUE;
}

jbyteArray Mp4Encoder::getThumbnailData(JNIEnv* env, jobject thiz) {
	if (thumbnail == NULL)
		return NULL;
	jbyteArray result = env->NewByteArray(thumbnailDataLength);
	env->SetByteArrayRegion(result, 0, thumbnailDataLength,
			(const jbyte *) thumbnail);
	return result;
}

jint Mp4Encoder::getThumbnailWidth(JNIEnv* env, jobject thiz) {
	return videoParameter->iPicWidth;
}

jint Mp4Encoder::getThumbnailHeight(JNIEnv* env, jobject thiz) {
	return videoParameter->iPicHeight;
}

void Mp4Encoder::release(JNIEnv* env, jobject thiz) {
	if (mp4FileHandler != NULL)
		MP4Close(mp4FileHandler, 0);
	if (aacHandle != NULL)
		aacEncoder->Uninit(aacHandle);
	if (h264Encoder != NULL) {
		h264Encoder->Uninitialize();
		DestroySVCEncoder(h264Encoder);
	}
}
