package co.splots.recorder;

import java.io.IOException;

import android.hardware.Camera.CameraInfo;

public class H264Encoder {

	/**
	 * Native JNI - initialize H264 encoder
	 * 
	 */
	public native void init(float frameRate, int srcWidth, int srcHeight, int destSize, String outputFile)
			throws IOException;

	/**
	 * Native JNI - encode one or more frames
	 * 
	 */
	public native boolean encode(byte[] inputArray, CameraInfo info);

	/**
	 * Thumbnail data
	 * @return thumbnail in nv21 yuv format
	 */
	public native byte[] getThumbnail();

	/**
	 * Native JNI - release H264 encoder and flush file
	 * 
	 */
	public native void release();

	static {
		System.loadLibrary("h264-encoder");
	}
}
