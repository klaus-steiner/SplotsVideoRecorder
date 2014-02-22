#include <string.h>
#include <stdio.h>
#include <jni.h>
#include <voAAC.h>
#include <cmnMemory.h>
#include <android/log.h> 
#include "tools.h"

#define DEBUG 0
#define MODUL_NAME "aac-encoder"

#if DEBUG
#define LOG(msg, args...) __android_log_print(ANDROID_LOG_DEBUG, MODUL_NAME, msg, ## args)
#else
#define LOG(msg, args...)
#endif

/* internal storage */
FILE* outfile;
VO_AUDIO_CODECAPI codec_api;
VO_HANDLE handle = 0;
VO_MEM_OPERATOR mem_operator = { 0 };
VO_CODEC_INIT_USERDATA user_data;
AACENC_PARAM params = { 0 };

/**
 * initialize aac encoder
 **/
JNIEXPORT void JNICALL Java_co_splots_recorder_AACEncoder_init(JNIEnv* env,
		jobject thiz, jint bitrate, jint channels, jint sample_rate,
		jint bits_per_sample, jstring output_path) {

	if (bits_per_sample != 16) {
		throwJavaException(env, "java/lang/IllegalArgumentException",
				"Unsupported sample depth. Only 16 bits per sample is supported.");
		return;
	}

	voGetAACEncAPI(&codec_api);

	mem_operator.Alloc = cmnMemAlloc;
	mem_operator.Copy = cmnMemCopy;
	mem_operator.Free = cmnMemFree;
	mem_operator.Set = cmnMemSet;
	mem_operator.Check = cmnMemCheck;
	user_data.memflag = VO_IMF_USERMEMOPERATOR;
	user_data.memData = (VO_PTR) &mem_operator;
	if (codec_api.Init(&handle, VO_AUDIO_CodingAAC, &user_data) != VO_ERR_NONE) {
		throwJavaException(env, "java/lang/IllegalArgumentException",
				"Could not init the coding api.");
		return;
	}

	params.sampleRate = sample_rate;
	params.bitRate = bitrate;
	params.nChannels = channels;
	params.adtsUsed = 1;

	if (codec_api.SetParam(handle, VO_PID_AAC_ENCPARAM, &params) != VO_ERR_NONE) {
		throwJavaException(env, "java/lang/IllegalArgumentException",
				"Unable to set encoding parameters.");
		return;
	}

	const char* output_file = (*env)->GetStringUTFChars(env, output_path,
			(jboolean) 0);
	outfile = fopen(output_file, "wb");
	if (outfile == NULL ) {
		throwJavaException(env, "java/lang/IOException",
				"Could not open the output file.");
		return;
	} LOG("writing to %s", output_file);
	(*env)->ReleaseStringUTFChars(env, output_path, output_file);
}

/**
 * encode data
 */
JNIEXPORT void JNICALL Java_co_splots_recorder_AACEncoder_encode(
		JNIEnv* env, jobject thiz, jbyteArray input_data) {

	jbyte* buffer = (*env)->GetByteArrayElements(env, input_data, (jboolean) 0);
	int input_size = (*env)->GetArrayLength(env, input_data);

	VO_CODECBUFFER input;
	VO_CODECBUFFER output;
	VO_AUDIO_OUTPUTINFO output_info;

	int read_size = params.nChannels * 4 * 1024;
	unsigned char output_buffer[read_size];

	LOG("input buffer: %d", input_size);
	int status = 0;
	int count = 0;
	do {
		int read_bytes = count * read_size;
		input.Buffer = buffer + count * read_size;
		int bytesLeft = input_size - count * read_size;
		input.Length = bytesLeft >= read_size ? read_size : bytesLeft;
		output.Buffer = output_buffer;
		output.Length = read_size;
		status = codec_api.SetInputData(handle, &input);
		if (status == VO_ERR_INPUT_BUFFER_SMALL)
			LOG("input buffer small read_bytes=%d", read_bytes);
		do {
			output.Buffer = output_buffer;
			output.Length = read_size;
			status = codec_api.GetOutputData(handle, &output, &output_info);
			if (status == VO_ERR_LICENSE_ERROR)
				break;
			else if (status == VO_ERR_OUTPUT_BUFFER_SMALL)
				LOG("output buffer small used_bytes=%d",
						(int ) output_info.InputUsed);
			else if (status == VO_ERR_NONE)
				fwrite(output_buffer, 1, output.Length, outfile);
		} while (status != VO_ERR_INPUT_BUFFER_SMALL);
		if (status == VO_ERR_LICENSE_ERROR)
			break;
		count++;
	} while (count * read_size < input_size && status); LOG("finished output");
	(*env)->ReleaseByteArrayElements(env, input_data, buffer, JNI_ABORT);
	free(output_buffer);
}

/**
 * release encoder
 */
void Java_co_splots_recorder_AACEncoder_release(JNIEnv* env, jobject thiz) {
	fclose(outfile);
	codec_api.Uninit(handle);
}

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM * vm, void * reserved) {
	return JNI_VERSION_1_6;
}
