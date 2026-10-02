package org.miacode.android;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.os.Build;
import android.os.PowerManager;
import android.content.pm.ServiceInfo;
import java.io.File;
import org.json.JSONObject;

// Separate process keeps the platform export proof independent of Qt's Activity
// and graphics surface. Durable production jobs/checkpoints belong to P4.
public final class ProbeExportService extends Service {
    private volatile boolean cancelled;
    private volatile boolean running;
    private PowerManager.WakeLock wakeLock;
    @Override public IBinder onBind(Intent intent) { return null; }
    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent == null) { stopSelf(startId); return START_NOT_STICKY; }
        if ("cancel".equals(intent.getAction())) { cancelled = true; return START_NOT_STICKY; }
        final String token = intent.getStringExtra("run");
        if (token == null || !token.matches("[a-f0-9-]{36}")) { stopSelf(startId); return START_NOT_STICKY; }
        if (running) {
            writeFailure(token, "Another probe is running");
            return START_NOT_STICKY;
        }
        running = true;
        NotificationManager manager = getSystemService(NotificationManager.class);
        manager.createNotificationChannel(new NotificationChannel("export-probe", "媒体探针导出", NotificationManager.IMPORTANCE_LOW));
        PendingIntent cancel = PendingIntent.getService(this, 1,
            new Intent(this, ProbeExportService.class).setAction("cancel"), PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        Notification notification = new Notification.Builder(this, "export-probe")
            .setSmallIcon(android.R.drawable.stat_sys_upload).setContentTitle("MiaCode 正在生成媒体探针")
            .setContentText("本机离线导出，可点击取消").setOngoing(true)
            .addAction(new Notification.Action.Builder(null, "取消", cancel).build()).build();
        startForeground(4103, notification, Build.VERSION.SDK_INT >= 35
            ? ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROCESSING : ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        wakeLock = getSystemService(PowerManager.class).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "MiaCode:export-probe");
        wakeLock.acquire(180000);
        new Thread(() -> {
            try { MediaPipelineProbe.run(this, () -> cancelled, token); }
            catch (Exception error) { writeFailure(token, error.toString()); }
            finally {
                if (wakeLock != null && wakeLock.isHeld()) wakeLock.release();
                running = false; stopForeground(STOP_FOREGROUND_REMOVE); stopSelf();
            }
        }, "miacode-export-probe").start();
        return START_NOT_STICKY;
    }
    private void writeFailure(String token, String error) {
        try { MediaPipelineProbe.writeReport(new File(getFilesDir(), "probe/" + token + ".json"),
            new JSONObject().put("ok", false).put("run", token).put("error", error).toString()); }
        catch (Exception failure) { android.util.Log.e("MiaCode", "Probe report failed", failure); }
    }
    @Override public void onDestroy() { cancelled = true; super.onDestroy(); }
    @Override public void onTimeout(int startId, int foregroundServiceType) {
        cancelled = true;
        stopForeground(STOP_FOREGROUND_REMOVE);
        stopSelf();
    }
}
