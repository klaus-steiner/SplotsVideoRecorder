package co.splots.recorder;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.graphics.YuvImage;
import android.hardware.Camera.CameraInfo;

public class H264Encoder {

	/**
	 * Initialize H264 encoder
	 * 
	 * @param frameRate
	 *           the maximum frame rate.
	 * @param srcWidth
	 *           Input width of the preview frame.
	 * @param srcHeight
	 *           Input height of the preview frame.
	 * @param srcRect
	 * 
	 */
	public native void init(float frameRate, int srcWidth, int srcHeight, Rect cropRect, int destWidth, int destHeight,
			String outputFile) throws IOException;

	/**
	 * Encodes a preview frame
	 * 
	 * @param preview
	 *           Preview frame in nv21 format.
	 * @param info
	 *           Frame info. This contains orientation and facing.
	 * @return true if success.
	 * 
	 */
	public native boolean encode(byte[] preview, CameraInfo info, long timeStamp);

	/**
	 * Thumbnail data (first frame of the video)
	 * 
	 * @return thumbnail byte array in nv21 yuv format.
	 */
	public native byte[] getThumbnailData();

	public native int getThumbnailWidth();

	public native int getThumbnailHeight();

	/**
	 * Release H264 encoder and flush file
	 */
	public native void release();

	static {
		System.loadLibrary("h264-encoder");
	}
}
