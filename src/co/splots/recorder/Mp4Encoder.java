package co.splots.recorder;

import java.io.ByteArrayOutputStream;
import java.io.IOException;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.graphics.YuvImage;
import android.hardware.Camera.CameraInfo;

public class Mp4Encoder {

	private Bitmap thumbnail;

	public native void init(String outputPath) throws IllegalArgumentException;

	public native void initAudio(int bitrate, int channels, int sampleRate) throws IllegalArgumentException;

	public void initVideo(float frameRate, int srcWidth, int srcHeight, int destWidth, int destHeight)
			throws IllegalArgumentException, IOException {
		initVideo(frameRate, srcWidth, srcHeight, new Rect(0, 0, destWidth, destHeight), destWidth, destHeight);
	}

	public native void initVideo(float frameRate, int srcWidth, int srcHeight, Rect cropRect, int destWidth,
			int destHeight) throws IllegalArgumentException, IOException;

	public native void setFrameRate(float frameRate);

	public native float getAverageFrameRate();

	public native void setPreviewSize(int width, int height);

	public native void setVideoBitrate(int bitrate);

	public native void setCropRect(Rect crop);

	public native void setCameraInfo(CameraInfo info);

	public native boolean encodePreviewFrame(byte[] preview, long timeStamp) throws IllegalStateException;

	public native boolean encodeAudioSample(byte[] audio) throws IllegalStateException;

	public Bitmap getThumbnail() {
		if (thumbnail == null) {
			byte[] thumbnailData = getThumbnailData();
			if (thumbnailData != null) {
				int width = getThumbnailWidth();
				int height = getThumbnailHeight();
				YuvImage yuvImage = new YuvImage(thumbnailData, ImageFormat.NV21, width, height, null);
				ByteArrayOutputStream baos = new ByteArrayOutputStream();
				yuvImage.compressToJpeg(new Rect(0, 0, width, height), 100, baos);
				byte[] jdata = baos.toByteArray();
				thumbnail = BitmapFactory.decodeByteArray(jdata, 0, jdata.length);
			}
		}
		return thumbnail;
	}

	public Bitmap getThumbnail(int width, int height) {
		return null;
	}

	private native byte[] getThumbnailData();

	private native int getThumbnailWidth();

	private native int getThumbnailHeight();

	public native void release();

	static {
		System.loadLibrary("mp4enc");
	}

}
