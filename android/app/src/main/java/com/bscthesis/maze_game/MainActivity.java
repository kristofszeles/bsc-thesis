package com.bscthesis.maze_game;

import android.content.Intent;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {
    private static final String TAG = "MazeMain";
    private static final int REQ_OPEN_MAP = 1001;
    private static final int REQ_SAVE_MAP = 1002;

    private static volatile MainActivity instance;
    // Bytes captured by saveMapPicker() and consumed in onActivityResult.
    private static volatile byte[] pendingSaveData;
    // Last observed soft-keyboard height in pixels (0 when the IME is hidden). Read from the
    // SDL thread via getImeHeightPx() — using `volatile` for a plain word write is sufficient.
    private static volatile int imeHeightPx;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        instance = this;
        installImeHeightTracker();
    }

    // Returns the current IME (soft keyboard) height in pixels, or 0 if hidden. Called from
    // native via JNI to position the chat panel/input above the keyboard.
    public static int getImeHeightPx() {
        return imeHeightPx;
    }

    private void installImeHeightTracker() {
        // ADJUST_NOTHING: the IME just overlays the surface. The HUD layout (dpadGetLayout,
        // viewToggleGetLayout, ...) subtracts getImeHeightPx() from the bottom anchor instead,
        // so the 3D viewport keeps its full size while only the HUD lifts above the keyboard.
        getWindow().setSoftInputMode(
                WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING
                        | WindowManager.LayoutParams.SOFT_INPUT_STATE_HIDDEN);

        // Decor view always receives insets first; attaching here avoids racing SDL's own
        // RelativeLayout setup and survives any later content-view churn.
        final View decor = getWindow().getDecorView();
        decor.setOnApplyWindowInsetsListener((v, insets) -> {
            int h = 0;
            if (Build.VERSION.SDK_INT >= 30) {
                h = insets.getInsets(WindowInsets.Type.ime()).bottom;
            } else {
                // Pre-30 fallback: getSystemWindowInsetBottom mixes IME + nav bar, so use
                // the visible-display-frame difference and filter out values too small to
                // plausibly be a keyboard (i.e. a bare navigation bar).
                Rect r = new Rect();
                v.getWindowVisibleDisplayFrame(r);
                int rootH = v.getRootView().getHeight();
                int diff = rootH - (r.bottom - r.top);
                if (diff > rootH * 0.15) {
                    h = diff;
                }
            }
            imeHeightPx = h;
            return v.onApplyWindowInsets(insets);
        });
    }

    @Override
    protected void onDestroy() {
        if (instance == this) instance = null;
        pendingSaveData = null;
        super.onDestroy();
    }

    // Called from native on the SDL thread. Hops to the UI thread to launch the SAF open picker
    // (Activity.startActivityForResult must run on the main thread).
    public static void openMapPicker() {
        final MainActivity a = instance;
        if (a == null) {
            nativeOnMapPicked(null);
            return;
        }
        a.runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            // SAF does not filter reliably by extension. Allow any file and trust the map parser.
            intent.setType("*/*");
            try {
                a.startActivityForResult(intent, REQ_OPEN_MAP);
            } catch (Exception e) {
                Log.e(TAG, "open picker failed", e);
                nativeOnMapPicked(null);
            }
        });
    }

    // Called from native on the SDL thread. The bytes are stashed and written to the URI the
    // user picks via SAF's ACTION_CREATE_DOCUMENT.
    public static void saveMapPicker(byte[] data) {
        final MainActivity a = instance;
        if (a == null || data == null) {
            return;
        }
        pendingSaveData = data;
        a.runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_TITLE, "untitled.map");
            try {
                a.startActivityForResult(intent, REQ_SAVE_MAP);
            } catch (Exception e) {
                Log.e(TAG, "save picker failed", e);
                pendingSaveData = null;
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == REQ_OPEN_MAP) {
            byte[] bytes = null;
            if (resultCode == RESULT_OK && data != null && data.getData() != null) {
                Uri uri = data.getData();
                try (InputStream in = getContentResolver().openInputStream(uri)) {
                    if (in != null) {
                        ByteArrayOutputStream out = new ByteArrayOutputStream();
                        byte[] buf = new byte[8192];
                        int n;
                        while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
                        bytes = out.toByteArray();
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Failed to read picked map", e);
                }
            }
            nativeOnMapPicked(bytes);
            return;
        }
        if (requestCode == REQ_SAVE_MAP) {
            byte[] toWrite = pendingSaveData;
            pendingSaveData = null;
            if (resultCode == RESULT_OK && data != null && data.getData() != null && toWrite != null) {
                Uri uri = data.getData();
                // "w" is the universally supported mode on SAF providers; "wt" (truncate) is not
                // guaranteed. Document providers truncate on open with "w" anyway.
                try (OutputStream out = getContentResolver().openOutputStream(uri, "w")) {
                    if (out != null) {
                        out.write(toWrite);
                        out.flush();
                    }
                } catch (Exception e) {
                    Log.e(TAG, "Failed to write picked map", e);
                }
            }
            return;
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    private static native void nativeOnMapPicked(byte[] data);
}
