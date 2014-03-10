package co.splots.recorder;

public class AACEncoder {

	/**
	 * Native JNI - initialize AAC encoder
	 * 
	 */
	public native void init(int bitrate, int channels, int sampleRate, int bitsPerSample, String outputFile)
			throws IllegalArgumentException;

	/**
	 * Native JNI - encode one or more frames
	 * 
	 */
	public native void encode(byte[] inputArray);

	/**
	 * Native JNI - release AAC encoder and flush file
	 * 
	 */
	public native void release();

	static {
		System.loadLibrary("aac-encoder");
	}

}
