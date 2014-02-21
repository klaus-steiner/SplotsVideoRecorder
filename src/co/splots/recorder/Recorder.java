package co.splots.recorder;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.ConcurrentLinkedQueue;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.graphics.YuvImage;
import android.hardware.Camera;
import android.hardware.Camera.CameraInfo;
import android.hardware.Camera.Parameters;
import android.hardware.Camera.Size;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.util.Log;

import com.coremedia.iso.boxes.Container;
import com.googlecode.mp4parser.FileDataSourceImpl;
import com.googlecode.mp4parser.authoring.Movie;
import com.googlecode.mp4parser.authoring.builder.DefaultMp4Builder;
import com.googlecode.mp4parser.authoring.tracks.AACTrackImpl;
import com.googlecode.mp4parser.authoring.tracks.H264TrackImpl;

/**
 * Class for recording videos. It provides compared to the android {@link MediaRecorder} an pause() and
 * resume() function. IMPORTANT: this VideoRecorder uses the onPreviewFrame() callback of the {@link Camera}
 * class, so you have to initialize the preview in the recommended way.
 * 
 * @author Klaus Steiner
 */
public class Recorder implements Camera.PreviewCallback {

	/**
	 * Interface for notifying that the recording has finished. This can happen either through calling the
	 * stop() function or if the maxDuration is set, by reaching the maximum duration. See also {@link
	 * setMaxDuration()}
	 * 
	 * @author Klaus Steiner
	 */
	public static interface OnRecordingFinishedListener {

		public void onRecordingFinished(File movie);

	}

	/**
	 * Is a periodically callback, which updates the current recorded time. Can be used for usability
	 * 
	 * @author Klaus Steiner
	 */
	public static interface OnProgressUpdateListener {

		/**
		 * This function is only called, if the progressUpdateInterval is > 0. See also {@link
		 * setProgressInterval()}
		 * 
		 * @param recordedTime
		 */
		public void onProgressUpdate(long recordedTime);

	}

	public static interface OnRecordingStoppedListener {

		public void onRecordingStopped();

	}

	private class AudioRecordRunnable implements Runnable {

		private AudioRecord audioRecord;
		private volatile boolean ready;

		public AudioRecordRunnable() {
			ready = false;
		}

		public boolean isReady() {
			return ready;
		}

		@Override
		public void run() {
			ready = false;
			android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_AUDIO);
			int bufferSize = AudioRecord.getMinBufferSize(audioSampleRate, AudioFormat.CHANNEL_IN_MONO,
					AudioFormat.ENCODING_PCM_16BIT);
			ByteBuffer frameDataArray = ByteBuffer.allocate(bufferSize);
			audioRecord = new AudioRecord(MediaRecorder.AudioSource.DEFAULT, audioSampleRate, AudioFormat.CHANNEL_IN_MONO,
					AudioFormat.ENCODING_PCM_16BIT, bufferSize);
			byte[] frameData = new byte[bufferSize];
			audioRecord.startRecording();
			byte[] cache = null;
			String aacStreamPath = new StringBuilder(outputPath).append('.').append("aac").toString();
			AACEncoder aacEncoder = new AACEncoder();
			aacEncoder.init(125000, 1, audioSampleRate, 16, aacStreamPath);
			synchronized (Recorder.LOCK) {
				ready = true;
				Recorder.LOCK.notifyAll();
			}
			while (runAudioThread) {
				int readedDataSize = audioRecord.read(frameData, 0, frameData.length);
				if (readedDataSize > 0) {
					if (runAudioThread && recording) {
						if (cache != null) {
							int frameLength = cache.length + readedDataSize;
							frameDataArray = ByteBuffer.allocate(frameLength);
							frameDataArray.put(cache);
							byte[] readedData = new byte[readedDataSize];
							for (int i = 0; i < readedDataSize; i++)
								readedData[i] = frameData[i];
							frameDataArray.put(readedData);
							cache = null;
						} else
							frameDataArray = ByteBuffer.wrap(frameData, 0, readedDataSize);
						if (frameDataArray != null)
							aacEncoder.encode(frameDataArray.array());
					}
				} else if (runAudioThread && !recording && cache == null)
					cache = frameData.clone();
			}
			Log.d(TAG, "left aac encoding loop.");
			audioRecord.stop();
			audioRecord.release();
			audioRecord = null;
			aacEncoder.release();
			aacEncoder = null;
		}
	}

	private class VideoEncoderRunnable implements Runnable {

		private static final String TAG = "VideoEncoderRunnable";

		@Override
		public void run() {
			VideoData concurrentData = null;
			boolean lastData = false;
			H264Encoder encoder = new H264Encoder();
			String h264Path = new StringBuilder(outputPath).append('.').append("h264").toString();
			try {
				encoder.init(frameRate, previewSize.width, previewSize.height, frameSize, h264Path);
				while (!lastData) {
					concurrentData = viewDataQueue.poll();
					lastData = (concurrentData == null) && !runVideoThread;
					if (concurrentData != null) {
						if (encoder.encode(concurrentData.getData(), concurrentData.getCameraInfo())) {
							if (thumbnail == null) {
								byte[] thumbnailData = encoder.getThumbnail();
								if (thumbnailData != null) {
									YuvImage yuvImage = new YuvImage(thumbnailData, ImageFormat.NV21, frameSize, frameSize, null);
									ByteArrayOutputStream baos = new ByteArrayOutputStream();
									yuvImage.compressToJpeg(new Rect(0, 0, frameSize, frameSize), 100, baos);
									byte[] jdata = baos.toByteArray();
									thumbnail = BitmapFactory.decodeByteArray(jdata, 0, jdata.length);
									File thumbnailFile = new File(
											new StringBuilder(outputPath).append('.').append("jpeg").toString());
									FileOutputStream fos;
									try {
										fos = new FileOutputStream(thumbnailFile);
										thumbnail.compress(Bitmap.CompressFormat.JPEG, 100, fos);
										fos.close();
									} catch (Exception e) {
										e.printStackTrace();
									}
								}
							}
						} else
							Log.d(TAG, "h264 encoding failed.");
					}
				}

				encoder.release();
			} catch (IOException e) {
				e.printStackTrace();
			}
			Log.d(TAG, "left h264 encoding loop.");
		}
	}

	private class MuxerRunnable implements Runnable {

		@Override
		public void run() {
			try {
				audioThread.join();
			} catch (InterruptedException e) {}
			try {
				videoThread.join();
			} catch (InterruptedException e) {}
			try {
				String aacTrackPath = new StringBuilder(outputPath).append('.').append("aac").toString();
				String h264TrackPath = new StringBuilder(outputPath).append('.').append("h264").toString();
				AACTrackImpl aacTrack = new AACTrackImpl(new FileDataSourceImpl(aacTrackPath));
				H264TrackImpl h264Track = new H264TrackImpl(new FileDataSourceImpl(h264TrackPath), "eng", 1000000,
						(int) (1000000.f / frameRate));
				Movie movie = new Movie();
				movie.addTrack(h264Track);
				movie.addTrack(aacTrack);
				Container out = new DefaultMp4Builder().build(movie);
				FileOutputStream fos = new FileOutputStream(new File(outputPath));
				out.writeContainer(fos.getChannel());
				fos.close();
				/*new File(aacTrackPath).delete();
				new File(h264TrackPath).delete();*/
				if (recordingFinishedListener != null && context instanceof Activity) {
					((Activity) context).runOnUiThread(new Runnable() {

						@Override
						public void run() {
							recordingFinishedListener.onRecordingFinished(new File(outputPath));
						}
					});
				}
			} catch (FileNotFoundException e) {
				e.printStackTrace();
			} catch (IOException e) {
				e.printStackTrace();
			}
		}
	}

	public static final Object LOCK = new Object();
	private static final Object RECORDER_SYNC = new Object();
	private static final String TAG = Recorder.class.getName();
	private ConcurrentLinkedQueue<VideoData> viewDataQueue;
	private volatile boolean recording;
	private volatile boolean runAudioThread;
	private volatile boolean runVideoThread;
	private int audioSampleRate;
	private Context context;
	private Camera camera;
	private long maxDuration;
	private long recordedTime;
	private volatile long recordStart;
	private volatile long lastRecordedTimeOffset;
	private OnRecordingFinishedListener recordingFinishedListener;
	private OnRecordingStoppedListener recordingStoppedListener;
	private OnProgressUpdateListener progressUpdateListener;
	private long progressUpdateInterval;
	private long lastPublish;
	private String outputPath;
	private CameraInfo cameraInfo;
	private Size previewSize;
	private int frameSize;
	private Bitmap thumbnail;
	private Thread videoThread;
	private Thread audioThread;
	private float frameRate;

	public Recorder(Context context, String outputPath, int frameSize) {
		this.context = context;
		this.outputPath = outputPath;
		this.frameSize = frameSize;
		this.audioSampleRate = 44100;
		this.recording = false;
		this.maxDuration = -1;
		this.recordedTime = 0;
		this.recordStart = 0;
		this.progressUpdateInterval = -1;
		this.recording = false;
		viewDataQueue = new ConcurrentLinkedQueue<VideoData>();
	}

	public void setOnRecordingFinishedListener(OnRecordingFinishedListener listener) {
		recordingFinishedListener = listener;
	}

	public void setOnProgressUpdateListener(OnProgressUpdateListener listener) {
		progressUpdateListener = listener;
	}

	public void setOnMaxDurationReachedListener(OnRecordingStoppedListener listener) {
		recordingStoppedListener = listener;
	}

	public void setProgressUpdateInterval(long interval) {
		progressUpdateInterval = interval;
	}

	public void setCamera(Camera camera, CameraInfo info) {
		this.camera = camera;
		this.cameraInfo = info;
	}

	public Bitmap getThumbnail() {
		return thumbnail;
	}

	public void prepare() throws IOException {
		if (runVideoThread)
			throw new IllegalStateException("You must first stop the encoder.");
		if (camera == null)
			throw new IllegalStateException("No camera set.");
		recording = false;
		recordStart = 0;
		Parameters cameraParameters = camera.getParameters();
		synchronized (RECORDER_SYNC) {
			previewSize = cameraParameters.getPreviewSize();
			int[] fpsRange = new int[2];
			cameraParameters.getPreviewFpsRange(fpsRange);
			frameRate = ((float) fpsRange[1]) / 1000.f;
		}
		cameraParameters.setPreviewFormat(ImageFormat.NV21);
		camera.setParameters(cameraParameters);
		runAudioThread = true;
		AudioRecordRunnable audioRecordRunnable = new AudioRecordRunnable();
		audioThread = new Thread(audioRecordRunnable, "AudioThread");
		audioThread.start();
		camera.setPreviewCallback(this);
		synchronized (Recorder.LOCK) {
			try {
				while (!audioRecordRunnable.isReady())
					LOCK.wait();
			} catch (InterruptedException e) {}
		}
		runVideoThread = true;
		videoThread = new Thread(new VideoEncoderRunnable(), "VideoThread");
		videoThread.start();
		new Thread(new MuxerRunnable(), "MuxerRunnable").start();
		Log.d(TAG, "everything prepared....");
	}

	public void start() {
		if (!recording) {
			recordStart = System.nanoTime();
			lastRecordedTimeOffset = recordedTime;
			recording = true;
		}
	}

	public void pause() {
		recording = false;
	}

	public void stop() {
		pause();
		camera.setPreviewCallback(null);
		runAudioThread = false;
		runVideoThread = false;
		if (recordingStoppedListener != null)
			recordingStoppedListener.onRecordingStopped();
	}

	public synchronized long getRecordDuration() {
		return recordedTime;
	}

	public void setMaxDuration(long duration) {
		this.maxDuration = duration;
	}

	public void setAudioSampleRate(int sampleRate) {
		this.audioSampleRate = sampleRate;
	}

	@Override
	public void onPreviewFrame(byte[] data, Camera camera) {
		if (((maxDuration < 0) || (recordedTime < maxDuration)) && recording) {
			long currentTime = System.nanoTime();
			long recordDuration = currentTime - recordStart;
			recordedTime = recordDuration + lastRecordedTimeOffset;
			viewDataQueue.offer(new VideoData().set(recordedTime, data.clone(), cameraInfo));
			if ((progressUpdateListener != null) && (progressUpdateInterval > 0)
					&& ((lastPublish + progressUpdateInterval) <= currentTime) || lastPublish < 0) {
				lastPublish = currentTime;
				progressUpdateListener.onProgressUpdate(recordedTime);
			}
		} else if (recordedTime >= maxDuration) {
			Log.d(TAG, "max duration reached, try to stop...");
			recordedTime = maxDuration;
			stop();
		}
	}
}
