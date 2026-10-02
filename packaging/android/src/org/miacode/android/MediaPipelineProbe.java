package org.miacode.android;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.media.MediaMuxer;
import android.media.MediaExtractor;
import android.opengl.EGL14;
import android.opengl.EGLExt;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLSurface;
import android.opengl.GLES20;
import android.os.Build;
import android.util.AtomicFile;
import android.view.Surface;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.function.BooleanSupplier;

// Platform proof only. Production chart frames/audio remain owned by the v2
// scene and mix engines; this deterministic probe cannot establish their parity.
final class MediaPipelineProbe {
    private static final int WIDTH = 720, HEIGHT = 720, FPS = 30, FRAMES = 60;
    private static final int SAMPLE_RATE = 48000, SAMPLE_COUNT = SAMPLE_RATE * 2;

    static String run(Context context, BooleanSupplier cancelled, String token) throws Exception {
        File directory = new File(context.getFilesDir(), "probe");
        if (!directory.isDirectory() && !directory.mkdirs()) throw new java.io.IOException("Cannot create probe directory");
        long begin = android.os.SystemClock.elapsedRealtime();
        JSONObject report = new JSONObject().put("schema", 1).put("run", token)
            .put("device", Build.MODEL).put("api", Build.VERSION.SDK_INT)
            .put("abis", new JSONArray(java.util.Arrays.asList(Build.SUPPORTED_ABIS)))
            .put("pageSize", android.system.Os.sysconf(android.system.OsConstants._SC_PAGESIZE))
            .put("capabilities", capabilities()).put("sceneParityVerified", false)
            .put("pvVerified", false).put("softwareFallbackVerified", false);
        File video = new File(directory, token + ".mp4");
        File wav = new File(directory, token + ".wav");
        File cover = new File(directory, token + ".png");
        try {
            byte[] pcm = makePcm();
            writeWav(wav, pcm);
            writeCover(cover);
            encodeVideo(video, pcm, cancelled);
            JSONObject verification = verifyMp4(video);
            report.put("ok", true).put("video", video.getName()).put("wav", wav.getName())
                .put("cover", cover.getName()).put("width", WIDTH).put("height", HEIGHT)
                .put("fps", FPS).put("frames", FRAMES).put("audioSamples", SAMPLE_COUNT)
                .put("containerVerification", verification)
                .put("elapsedMs", android.os.SystemClock.elapsedRealtime() - begin);
        } catch (Exception error) {
            report.put("ok", false).put("error", error.toString());
            writeReport(new File(directory, token + ".json"), report.toString());
            throw error;
        }
        writeReport(new File(directory, token + ".json"), report.toString());
        return report.toString();
    }

    private static JSONObject verifyMp4(File file) throws Exception {
        MediaExtractor extractor = new MediaExtractor();
        try {
            extractor.setDataSource(file.getAbsolutePath());
            JSONObject result = new JSONObject();
            int videoFrames = 0;
            boolean audioFound = false;
            for (int track = 0; track < extractor.getTrackCount(); ++track) {
                MediaFormat format = extractor.getTrackFormat(track);
                String mime = format.getString(MediaFormat.KEY_MIME);
                if (mime.startsWith("audio/")) { audioFound = true; result.put("audioMime", mime); }
                if (!mime.startsWith("video/")) continue;
                extractor.selectTrack(track);
                long previous = -1;
                while (extractor.getSampleTrackIndex() >= 0) {
                    long pts = extractor.getSampleTime();
                    if (pts < previous) throw new java.io.IOException("Video timestamps are not monotonic");
                    previous = pts; videoFrames++;
                    if (!extractor.advance()) break;
                }
                result.put("videoMime", mime).put("lastVideoPtsUs", previous);
                extractor.unselectTrack(track);
            }
            if (videoFrames != FRAMES || !audioFound) throw new java.io.IOException("MP4 frame count / audio track mismatch");
            return result.put("videoFrames", videoFrames).put("audioTrackPresent", true);
        } finally { extractor.release(); }
    }

    static void writeReport(File file, String json) throws Exception {
        File parent = file.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) throw new java.io.IOException("Cannot create report directory");
        AtomicFile atomic = new AtomicFile(file);
        FileOutputStream output = atomic.startWrite();
        try { output.write(json.getBytes(StandardCharsets.UTF_8)); atomic.finishWrite(output); }
        catch (Exception error) { atomic.failWrite(output); throw error; }
    }

    private static JSONArray capabilities() throws Exception {
        int[][] sizes = {{720,720},{1024,1024},{960,720},{1280,720},{1080,1080},
            {1440,1080},{1920,1080},{1440,1440},{1920,1440},{2560,1440}};
        JSONArray encoders = new JSONArray();
        for (MediaCodecInfo info : new MediaCodecList(MediaCodecList.REGULAR_CODECS).getCodecInfos()) {
            if (!info.isEncoder()) continue;
            for (String type : info.getSupportedTypes()) {
                if (!type.equalsIgnoreCase("video/avc")) continue;
                JSONObject encoder = new JSONObject().put("name", info.getName())
                    .put("hardware", info.isHardwareAccelerated()).put("software", info.isSoftwareOnly());
                try {
                    MediaCodecInfo.VideoCapabilities video = info.getCapabilitiesForType(type).getVideoCapabilities();
                    JSONArray presets = new JSONArray();
                    for (int[] size : sizes) for (int fps : new int[]{30,60,120}) {
                        presets.put(new JSONObject().put("width", size[0]).put("height", size[1]).put("fps", fps)
                            .put("advertised", video.areSizeAndRateSupported(size[0], size[1], fps))
                            .put("exportTested", false));
                    }
                    encoder.put("presets", presets);
                } catch (Exception error) { encoder.put("error", error.toString()); }
                encoders.put(encoder);
            }
        }
        return encoders;
    }

    private static byte[] makePcm() {
        ByteBuffer samples = ByteBuffer.allocate(SAMPLE_COUNT * 4).order(ByteOrder.LITTLE_ENDIAN);
        for (int i = 0; i < SAMPLE_COUNT; ++i) {
            double time = i / (double)SAMPLE_RATE;
            double beatPhase = time % 0.5;
            double tone = Math.sin(2 * Math.PI * 440 * time) * 0.1;
            double click = beatPhase < 0.015 ? Math.sin(2 * Math.PI * 1800 * time) * 0.3 * (1 - beatPhase / 0.015) : 0;
            short value = (short)((tone + click) * 32767);
            samples.putShort(value).putShort(value);
        }
        return samples.array();
    }

    private static void writeWav(File file, byte[] pcm) throws Exception {
        ByteBuffer header = ByteBuffer.allocate(44).order(ByteOrder.LITTLE_ENDIAN);
        header.put("RIFF".getBytes(StandardCharsets.US_ASCII)).putInt(36 + pcm.length)
            .put("WAVEfmt ".getBytes(StandardCharsets.US_ASCII)).putInt(16).putShort((short)1)
            .putShort((short)2).putInt(SAMPLE_RATE).putInt(SAMPLE_RATE * 4).putShort((short)4)
            .putShort((short)16).put("data".getBytes(StandardCharsets.US_ASCII)).putInt(pcm.length);
        try (FileOutputStream output = new FileOutputStream(file)) {
            output.write(header.array()); output.write(pcm); output.getFD().sync();
        }
    }

    private static void writeCover(File file) throws Exception {
        Bitmap bitmap = Bitmap.createBitmap(720, 720, Bitmap.Config.ARGB_8888);
        try {
            Canvas canvas = new Canvas(bitmap);
            canvas.drawColor(Color.rgb(23, 25, 30));
            Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
            paint.setColor(Color.rgb(71, 138, 201)); paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(12);
            canvas.drawCircle(360, 350, 240, paint);
            paint.setStyle(Paint.Style.FILL); paint.setColor(Color.WHITE); paint.setTextSize(38);
            canvas.drawText("MiaCode Android P0", 155, 350, paint);
            paint.setTextSize(26); canvas.drawText("Offline media pipeline probe", 185, 405, paint);
            try (FileOutputStream output = new FileOutputStream(file)) {
                if (!bitmap.compress(Bitmap.CompressFormat.PNG, 100, output)) throw new java.io.IOException("PNG encoding failed");
            }
        } finally { bitmap.recycle(); }
    }

    private static void encodeVideo(File file, byte[] pcm, BooleanSupplier cancelled) throws Exception {
        MediaCodec video = null, audio = null;
        MediaMuxer muxer = null;
        Surface inputSurface = null;
        EglTarget egl = null;
        boolean muxerStarted = false;
        try {
            MediaFormat vf = MediaFormat.createVideoFormat("video/avc", WIDTH, HEIGHT);
            vf.setInteger(MediaFormat.KEY_COLOR_FORMAT, MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
            vf.setInteger(MediaFormat.KEY_BIT_RATE, 4000000); vf.setInteger(MediaFormat.KEY_FRAME_RATE, FPS);
            vf.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1);
            video = MediaCodec.createEncoderByType("video/avc");
            video.configure(vf, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
            inputSurface = video.createInputSurface();
            egl = new EglTarget(inputSurface);
            video.start();
            MediaFormat af = MediaFormat.createAudioFormat("audio/mp4a-latm", SAMPLE_RATE, 2);
            af.setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectLC);
            af.setInteger(MediaFormat.KEY_BIT_RATE, 192000);
            audio = MediaCodec.createEncoderByType("audio/mp4a-latm");
            audio.configure(af, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE); audio.start();
            muxer = new MediaMuxer(file.getAbsolutePath(), MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            int videoTrack = -1, audioTrack = -1, frame = 0, audioOffset = 0;
            boolean videoEosSent = false, audioEosSent = false, videoDone = false, audioDone = false;
            // Samples produced before both track formats are available must be retained.
            java.util.ArrayList<Packet> packets = new java.util.ArrayList<>();
            MediaCodec.BufferInfo bufferInfo = new MediaCodec.BufferInfo();
            long deadline = android.os.SystemClock.elapsedRealtime() + 120000;
            while (!videoDone || !audioDone) {
                if (cancelled.getAsBoolean()) throw new InterruptedException("Background export is disabled; probe cancelled on leaving app");
                if (android.os.SystemClock.elapsedRealtime() > deadline) throw new java.io.IOException("Codec drain timed out");
                if (frame < FRAMES) {
                    egl.draw(frame);
                    EGLExt.eglPresentationTimeANDROID(egl.display, egl.surface, frame * 1000000000L / FPS);
                    if (!EGL14.eglSwapBuffers(egl.display, egl.surface)) throw new java.io.IOException("EGL swap failed");
                    frame++;
                } else if (!videoEosSent) { video.signalEndOfInputStream(); videoEosSent = true; }
                if (!audioEosSent) {
                    int index = audio.dequeueInputBuffer(0);
                    if (index >= 0) {
                        ByteBuffer input = audio.getInputBuffer(index);
                        input.clear();
                        int count = Math.min(input.remaining() / 4 * 4, pcm.length - audioOffset);
                        input.put(pcm, audioOffset, count);
                        audio.queueInputBuffer(index, 0, count, audioOffset / 4 * 1000000L / SAMPLE_RATE,
                            count == 0 ? MediaCodec.BUFFER_FLAG_END_OF_STREAM : 0);
                        audioOffset += count;
                        if (count == 0) audioEosSent = true;
                    }
                }
                for (int stream = 0; stream < 2; ++stream) {
                    MediaCodec codec = stream == 0 ? video : audio;
                    if ((stream == 0 && videoDone) || (stream == 1 && audioDone)) continue;
                    int index;
                    while ((index = codec.dequeueOutputBuffer(bufferInfo, 1000)) != MediaCodec.INFO_TRY_AGAIN_LATER) {
                        if (index == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
                            int track = muxer.addTrack(codec.getOutputFormat());
                            if (stream == 0) videoTrack = track; else audioTrack = track;
                            if (videoTrack >= 0 && audioTrack >= 0 && !muxerStarted) {
                                muxer.start(); muxerStarted = true;
                                for (Packet packet : packets) packet.write(muxer, videoTrack, audioTrack);
                                packets.clear();
                            }
                        } else if (index >= 0) {
                            try {
                                if (bufferInfo.size > 0 && (bufferInfo.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) == 0) {
                                    ByteBuffer output = codec.getOutputBuffer(index);
                                    output.position(bufferInfo.offset); output.limit(bufferInfo.offset + bufferInfo.size);
                                    Packet packet = new Packet(stream, output, bufferInfo);
                                    if (muxerStarted) packet.write(muxer, videoTrack, audioTrack); else packets.add(packet);
                                }
                                if ((bufferInfo.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                                    if (stream == 0) videoDone = true; else audioDone = true;
                                }
                            } finally { codec.releaseOutputBuffer(index, false); }
                        }
                    }
                }
            }
            if (!muxerStarted) throw new java.io.IOException("Codec did not produce both tracks");
            muxer.stop(); muxerStarted = false;
        } finally {
            if (video != null) video.release();
            if (audio != null) audio.release();
            if (egl != null) egl.close();
            if (inputSurface != null) inputSurface.release();
            if (muxer != null) {
                if (muxerStarted) { try { muxer.stop(); } catch (RuntimeException ignored) {} }
                muxer.release();
            }
        }
    }

    private static final class Packet {
        final int stream;
        final byte[] bytes;
        final MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
        Packet(int stream, ByteBuffer output, MediaCodec.BufferInfo original) {
            this.stream = stream; bytes = new byte[original.size]; output.get(bytes);
            info.set(0, bytes.length, original.presentationTimeUs, original.flags);
        }
        void write(MediaMuxer muxer, int videoTrack, int audioTrack) {
            muxer.writeSampleData(stream == 0 ? videoTrack : audioTrack, ByteBuffer.wrap(bytes), info);
        }
    }

    private static final class EglTarget implements AutoCloseable {
        EGLDisplay display = EGL14.EGL_NO_DISPLAY;
        EGLContext context = EGL14.EGL_NO_CONTEXT;
        EGLSurface surface = EGL14.EGL_NO_SURFACE;
        EglTarget(Surface input) throws Exception {
            try {
                display = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
                int[] version = new int[2];
                if (!EGL14.eglInitialize(display, version, 0, version, 1)) throw new java.io.IOException("EGL initialize failed");
                EGLConfig[] configs = new EGLConfig[1]; int[] count = new int[1];
                int[] attributes = {EGL14.EGL_RED_SIZE,8,EGL14.EGL_GREEN_SIZE,8,EGL14.EGL_BLUE_SIZE,8,
                    EGL14.EGL_RENDERABLE_TYPE,EGL14.EGL_OPENGL_ES2_BIT,0x3142,1,EGL14.EGL_NONE};
                if (!EGL14.eglChooseConfig(display, attributes, 0, configs, 0, 1, count, 0) || count[0] == 0)
                    throw new java.io.IOException("Recordable EGL config missing");
                context = EGL14.eglCreateContext(display, configs[0], EGL14.EGL_NO_CONTEXT,
                    new int[]{EGL14.EGL_CONTEXT_CLIENT_VERSION,2,EGL14.EGL_NONE}, 0);
                surface = EGL14.eglCreateWindowSurface(display, configs[0], input, new int[]{EGL14.EGL_NONE}, 0);
                if (context == EGL14.EGL_NO_CONTEXT || surface == EGL14.EGL_NO_SURFACE
                        || !EGL14.eglMakeCurrent(display, surface, surface, context))
                    throw new java.io.IOException("EGL encoder surface creation failed");
            } catch (Exception error) { close(); throw error; }
        }
        void draw(int frame) {
            GLES20.glViewport(0,0,WIDTH,HEIGHT); GLES20.glDisable(GLES20.GL_SCISSOR_TEST);
            GLES20.glClearColor(0.09f,0.10f,0.12f,1); GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
            GLES20.glEnable(GLES20.GL_SCISSOR_TEST);
            if (frame < 15) {
                GLES20.glScissor(80,300,560,120); GLES20.glClearColor(0.28f,0.54f,0.79f,1);
                GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
            } else {
                for (int lane = 0; lane < 8; ++lane) {
                    double angle = lane * Math.PI / 4;
                    int radius = 80 + (frame - 15) * 4;
                    int x = 360 + (int)(Math.sin(angle) * radius), y = 360 + (int)(Math.cos(angle) * radius);
                    GLES20.glScissor(x - 14,y - 14,28,28); GLES20.glClearColor(1,0.4f,0.7f,1);
                    GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);
                }
            }
            GLES20.glDisable(GLES20.GL_SCISSOR_TEST);
        }
        public void close() {
            if (display == EGL14.EGL_NO_DISPLAY) return;
            EGL14.eglMakeCurrent(display,EGL14.EGL_NO_SURFACE,EGL14.EGL_NO_SURFACE,EGL14.EGL_NO_CONTEXT);
            if (surface != EGL14.EGL_NO_SURFACE) EGL14.eglDestroySurface(display,surface);
            if (context != EGL14.EGL_NO_CONTEXT) EGL14.eglDestroyContext(display,context);
            EGL14.eglReleaseThread(); EGL14.eglTerminate(display); display = EGL14.EGL_NO_DISPLAY;
        }
    }
}
