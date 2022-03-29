package cz.walle.wallevrcontroller2;

import android.os.Bundle;

import org.freedesktop.gstreamer.GStreamer;

public class ControlActivity extends android.app.NativeActivity {
    static {
        System.loadLibrary("vrapi");
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("WalleVrControllerModule");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
            GStreamer.init(this);
        } catch (Exception e) {
            finish();
        }
    }
}
