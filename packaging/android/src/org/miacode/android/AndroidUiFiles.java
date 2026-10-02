package org.miacode.android;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.provider.DocumentsContract;
import org.json.JSONObject;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.util.UUID;
import java.util.concurrent.ExecutorService;

final class AndroidUiFiles {
    static final int PICK_FILE = 4110;
    private final MiaCodeActivity activity;
    private final ExecutorService io;
    private volatile JSONObject pending;

    AndroidUiFiles(MiaCodeActivity activity, ExecutorService io) { this.activity = activity; this.io = io; }
    boolean busy() { return pending != null; }

    void request(String payload) {
        activity.runOnUiThread(() -> {
            JSONObject request = null;
            try {
                request = new JSONObject(payload);
                if (busy() || activity.hasPendingFileOperation()) throw new java.io.IOException("已有文件选择正在进行");
                pending = request;
                boolean folder = request.optBoolean("selectFolder"), save = request.optBoolean("saveMode");
                Intent picker = new Intent(folder ? Intent.ACTION_OPEN_DOCUMENT_TREE
                    : save ? Intent.ACTION_CREATE_DOCUMENT : Intent.ACTION_OPEN_DOCUMENT);
                picker.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                    | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                if (!folder) {
                    picker.addCategory(Intent.CATEGORY_OPENABLE);
                    String name = new File(request.optString("startPath", "export.mp4")).getName();
                    String filters = request.optJSONArray("nameFilters") == null ? "" : request.optJSONArray("nameFilters").toString();
                    String mime = save ? ExportFilePublisher.mimeForName(name)
                        : filters.contains("*.png") ? "image/*" : filters.contains("*.wav") ? "audio/*" : "*/*";
                    picker.setType(mime);
                    if (save) picker.putExtra(Intent.EXTRA_TITLE, safeName(name));
                }
                activity.startActivityForResult(picker, PICK_FILE);
            } catch (Exception error) {
                if (pending == request) pending = null;
                send(request, false, error.toString(), null, null, false);
            }
        });
    }

    boolean onResult(int code, int result, Intent data) {
        if (code != PICK_FILE) return false;
        final JSONObject request = pending;
        if (request == null) return true;
        if (result != Activity.RESULT_OK || data == null || data.getData() == null) {
            pending = null; send(request, false, "", null, null, true); return true;
        }
        final Uri uri = data.getData();
        try {
            activity.getContentResolver().takePersistableUriPermission(uri,
                data.getFlags() & (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION));
        } catch (SecurityException ignored) { /* A session grant is still usable; a later revoked grant reports an error. */ }
        io.execute(() -> {
            File local = null;
            try {
                final boolean save = request.optBoolean("saveMode"), folder = request.optBoolean("selectFolder");
                if (folder && !save) local = activity.importUiDirectory(uri);
                else {
                    File directory = new File(activity.getFilesDir(), "ui-files/" + (save ? "exports/" : "imports/") + UUID.randomUUID());
                    if (!directory.mkdirs()) throw new java.io.IOException("无法创建工作目录");
                    if (folder) local = directory;
                    else {
                        String name = new File(request.optString("startPath", "document")).getName();
                        try (Cursor cursor = activity.getContentResolver().query(uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                            if (cursor != null && cursor.moveToFirst()) name = cursor.getString(0);
                        }
                        local = new File(directory, safeName(name));
                        if (!save) {
                            try (InputStream input = activity.getContentResolver().openInputStream(uri); FileOutputStream output = new FileOutputStream(local)) {
                                MiaCodeActivity.copyBounded(input, output, 512L * 1024 * 1024); output.getFD().sync();
                            }
                        }
                    }
                }
                pending = null; send(request, true, "", local, uri, false);
            } catch (Exception error) {
                if (local != null && local.isFile()) local.delete();
                pending = null; send(request, false, error.toString(), null, uri, false);
            }
        });
        return true;
    }

    private static String safeName(String name) {
        if (name == null || name.isEmpty() || name.equals(".") || name.equals("..")) return "document";
        return name.replaceAll("[\\\\/\\x00]", "_");
    }
    private void send(JSONObject request, boolean ok, String error, File local, Uri uri, boolean cancelled) {
        try {
            JSONObject result = new JSONObject().put("kind", "uiFile").put("ok", ok).put("error", error).put("cancelled", cancelled)
                .put("requestId", request == null ? "" : request.optString("requestId"))
                .put("saveMode", request != null && request.optBoolean("saveMode"))
                .put("selectFolder", request != null && request.optBoolean("selectFolder"));
            if (local != null) result.put("localPath", local.getAbsolutePath());
            if (uri != null) {
                result.put("uri", uri.toString());
                result.put("displayPath", displayPath(uri, local));
            }
            activity.runOnUiThread(() -> MiaCodeActivity.deliverResult(result.toString()));
        } catch (Exception errorMakingResult) { android.util.Log.e("MiaCode", "File response failed", errorMakingResult); }
    }
    private String displayPath(Uri uri, File local) {
        try {
            if ("com.android.externalstorage.documents".equals(uri.getAuthority())) {
                String id = DocumentsContract.isTreeUri(uri) ? DocumentsContract.getTreeDocumentId(uri) : DocumentsContract.getDocumentId(uri);
                if (id.startsWith("primary:")) return id.substring("primary:".length());
                return id.replace(':', '/');
            }
            try (Cursor cursor = activity.getContentResolver().query(uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
                if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) return cursor.getString(0);
            }
        } catch (Exception ignored) { /* The label is optional; the URI grant remains authoritative. */ }
        return local == null ? "" : local.getName();
    }
}
