package co.splots.recorder;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.ConcurrentLinkedQueue;

import android.graphics.Bitmap;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.hardware.Camera;
import android.hardware.Camera.CameraInfo;
import android.hardware.Camera.Parameters;
import android.hardware.Camera.Size;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Handler;
import android.os.Looper;

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
public class Mp4Recorder implements Camera.PreviewCallback {

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
			synchronized (Mp4Recorder.SYNC_AAC_H264) {
				ready = true;
				Mp4Recorder.SYNC_AAC_H264.notifyAll();
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
			audioRecord.stop();
			audioRecord.release();
			audioRecord = null;
			aacEncoder.release();
			aacEncoder = null;
		}
	}

	private class VideoEncoderRunnable implements Runnable {

		@Override
		public void run() {
			VideoData concurrentData = null;
			boolean lastData = false;
			while (!lastData) {
				concurrentData = viewDataQueue.poll();
				lastData = (concurrentData == null) && !runVideoThread;
				if (concurrentData != null)
					h264Encoder.encode(concurrentData.getData(), concurrentData.getCameraInfo(),
							concurrentData.getTimeStamp());
			}
			h264Encoder.release();
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
				if (!canceled) {
					AACTrackImpl aacTrack = new AACTrackImpl(new FileDataSourceImpl(aacTrackPath));
					long timeScale = Math.round(frameRate);
					H264TrackImpl h264Track = new H264TrackImpl(new FileDataSourceImpl(h264TrackPath), "eng", timeScale, 1);
					Movie movie = new Movie();
					movie.addTrack(h264Track);
					movie.addTrack(aacTrack);
					Container out = new DefaultMp4Builder().build(movie);
					FileOutputStream fos = new FileOutputStream(new File(outputPath));
					out.writeContainer(fos.getChannel());
					fos.close();
					/*new File(aacTrackPath).delete();
					new File(h264TrackPath).delete();*/
					new Handler(Looper.getMainLooper()).post(new Runnable() {

						@Override
						public void run() {
							if (recordingFinishedListener != null)
								recordingFinishedListener.onRecordingFinished(new File(outputPath));
						}
					});

				} else {
					new File(h264TrackPath).delete();
					new File(aacTrackPath).delete();
				}
			} catch (Exception e) {
				e.printStackTrace();
			}
		}
	}

	public static final Object SYNC_AAC_H264 = new Object();
	private ConcurrentLinkedQueue<VideoData> viewDataQueue;
	private volatile boolean recording;
	private volatile boolean runAudioThread;
	private volatile boolean runVideoThread;
	private volatile boolean canceled;
	private int audioSampleRate;
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
	private Thread videoThread;
	private Thread audioThread;
	private float frameRate;
	private H264Encoder h264Encoder;
	private AACEncoder aacEncoder;
	private int outputWidth;
	private int outputHeight;
	private Rect crop;

	public Mp4Recorder(String outputPath, int outputWidth, int outputHeight) {
		this(outputPath, outputWidth, outputHeight, null);
	}

	public Mp4Recorder(String outputPath, int outputWidth, int outputHeight, Rect crop) {
		this.outputPath = outputPath;
		this.outputWidth = outputWidth;
		this.outputHeight = outputHeight;
		if (crop == null)
			this.crop = new Rect(0, 0, outputWidth, outputHeight);
		else
			this.crop = crop;
		this.audioSampleRate = 44100;
		this.recording = false;
		this.maxDuration = -1;
		this.recordedTime = 0;
		this.recordStart = 0;
		this.progressUpdateInterval = -1;
		this.recording = false;
		this.canceled = false;
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
		this.camera.setPreviewCallback(this);
	}

	public Bitmap getThumbnail() {
		if (h264Encoder != null)
			return h264Encoder.getThumbnail();
		return null;
	}

	public void prepare() throws IOException {
		if (runVideoThread || runAudioThread)
			throw new IllegalStateException("You must first stop the encoder.");
		if (camera == null)
			throw new IllegalStateException("No camera set.");
		recording = false;
		canceled = false;
		recordStart = 0;
		Parameters cameraParameters = camera.getParameters();
		previewSize = cameraParameters.getPreviewSize();
		int[] fpsRange = new int[2];
		cameraParameters.getPreviewFpsRange(fpsRange);
		frameRate = ((float) fpsRange[Parameters.PREVIEW_FPS_MAX_INDEX]) / 1000.f;
		cameraParameters.setPreviewFormat(ImageFormat.NV21);
		camera.setParameters(cameraParameters);
		String h264Path = new StringBuilder(outputPath).append('.').append("h264").toString();
		String aacPath = new StringBuilder(outputPath).append('.').append("aac").toString();
		h264Encoder = new H264Encoder();
		aacEncoder = new AACEncoder();
		try {
			h264Encoder.init(frameRate, previewSize.width, previewSize.height, crop, outputWidth, outputHeight, h264Path);
			aacEncoder.init(125000, 1, audioSampleRate, 16, aacPath);
		} catch (Exception e) {
			cancel();
			e.printStackTrace();
		}
		runAudioThread = true;
		AudioRecordRunnable audioRecordRunnable = new AudioRecordRunnable();
		audioThread = new Thread(audioRecordRunnable, "AudioThread");
		audioThread.start();
		camera.setPreviewCallback(this);
		synchronized (Mp4Recorder.SYNC_AAC_H264) {
			try {
				while (!audioRecordRunnable.isReady())
					SYNC_AAC_H264.wait();
			} catch (InterruptedException e) {}
		}
		runVideoThread = true;
		videoThread = new Thread(new VideoEncoderRunnable(), "VideoThread");
		videoThread.start();
		new Thread(new MuxerRunnable(), "MuxerThread").start();
	}

	public void start() {
		if (!recording) {
			recordStart = System.currentTimeMillis();
			lastRecordedTimeOffset = recordedTime;
			recording = true;
		}
	}

	public void pause() {
		recording = false;
	}

	public void stop() {
		pause();
		if (camera != null) {
			try {
				camera.setPreviewCallback(null);
			} catch (Exception e) {}
		}
		runAudioThread = false;
		runVideoThread = false;
		new Handler(Looper.getMainLooper()).post(new Runnable() {

			@Override
			public void run() {
				if (recordingStoppedListener != null)
					recordingStoppedListener.onRecordingStopped();
			}
		});
	}

	public long getRecordDuration() {
		return recordedTime;
	}

	public void setMaxDuration(long duration) {
		this.maxDuration = duration;
	}

	public void setAudioSampleRate(int sampleRate) {
		this.audioSampleRate = sampleRate;
	}

	public void cancel() {
		canceled = true;
		stop();
	}

	@Override
	public void onPreviewFrame(byte[] data, Camera camera) {
		if (((maxDuration < 0) || (recordedTime < maxDuration)) && recording) {
			long currentTime = System.currentTimeMillis();
			long recordDuration = currentTime - recordStart;
			recordedTime = recordDuration + lastRecordedTimeOffset;
			viewDataQueue.offer(new VideoData().set(recordedTime, data, cameraInfo));
			if ((progressUpdateListener != null) && (progressUpdateInterval > 0)
					&& ((lastPublish + progressUpdateInterval) <= currentTime) || lastPublish < 0) {
				lastPublish = currentTime;
				progressUpdateListener.onProgressUpdate(recordedTime);
			}
		} else if (recordedTime >= maxDuration) {
			recordedTime = maxDuration;
			stop();
		}
	}
}
