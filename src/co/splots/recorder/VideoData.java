package co.splots.recorder;

import android.annotation.SuppressLint;
import android.hardware.Camera.CameraInfo;
import android.os.Build;

public class VideoData {

	private long timeStamp;

	private byte[] data;

	private CameraInfo info;

	public VideoData set(long timeStamp, byte[] data, CameraInfo info) {
		this.timeStamp = timeStamp;
		this.data = data;
		this.info = cloneCameraInfo(info);
		return this;
	}

	@SuppressLint("NewApi")
	private static CameraInfo cloneCameraInfo(CameraInfo info) {
		if (info == null)
			return null;
		CameraInfo clone = new CameraInfo();
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1)
			clone.canDisableShutterSound = info.canDisableShutterSound;
		clone.facing = info.facing;
		clone.orientation = info.orientation;
		return clone;
	}

	@Override
	public Object clone() throws CloneNotSupportedException {
		VideoData clone = new VideoData();
		clone.data = data.clone();
		clone.timeStamp = timeStamp;
		clone.info = cloneCameraInfo(info);
		return clone;
	}

	public CameraInfo getCameraInfo() {
		return info;
	}

	public byte[] getData() {
		return data;
	}

	public long getTimeStamp() {
		return timeStamp;
	}
}
