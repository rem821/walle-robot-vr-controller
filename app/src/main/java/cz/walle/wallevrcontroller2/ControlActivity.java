package cz.walle.wallevrcontroller2;

import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.os.Build;
import android.os.Bundle;

import androidx.annotation.RequiresApi;

import org.freedesktop.gstreamer.GStreamer;

import timber.log.Timber;

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

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            getCodecForMimeType("");
        }
    }

    @RequiresApi(api = Build.VERSION_CODES.Q)
    MediaCodecInfo getCodecForMimeType(String mimeType) {
        MediaCodecList list = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
        MediaCodecInfo[] infos = list.getCodecInfos();

        for (int i=0; i<infos.length; i++) {
            MediaCodecInfo info = infos[i];

            boolean is = info.isHardwareAccelerated();

            Timber.d("d");
        }
        return infos[0];
    }
}
