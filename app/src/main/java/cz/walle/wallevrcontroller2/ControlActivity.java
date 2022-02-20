package cz.walle.wallevrcontroller2;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.RequiresApi;

import org.freedesktop.gstreamer.GStreamer;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;

import io.github.controlwear.virtual.joystick.android.JoystickView;
import timber.log.Timber;

public class ControlActivity extends Activity implements SurfaceHolder.Callback {

    static {
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("GStreamerModule");
        GstreamerJNILib.nativeClassInit();
    }

    private GstreamerJNILib gstreamerLib = new GstreamerJNILib(this);

    private DatagramSocket datagramSocket;
    private Handler handler;

    private boolean streamRunning = false;

    private String[] speedMultipliers = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};

    // Called when the activity is first created.
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Initialize GStreamer and warn if it fails
        try {
            GStreamer.init(this);
        } catch (Exception e) {
            Toast.makeText(this, e.getMessage(), Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        setContentView(R.layout.activity_control);

        SurfaceView sv = (SurfaceView) this.findViewById(R.id.surface_video);
        SurfaceHolder sh = sv.getHolder();
        sh.addCallback(this);

        gstreamerLib.nativeInit();

        HandlerThread mHandlerThread = new HandlerThread("HandlerThread");
        mHandlerThread.start();
        handler = new Handler(mHandlerThread.getLooper());
    }

    @RequiresApi(api = Build.VERSION_CODES.Q)
    @Override
    protected void onResume() {
        super.onResume();

        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, speedMultipliers);

        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        Spinner spinner = findViewById(R.id.speed_multiplier);
        spinner.setAdapter(adapter);

        Button startButton = findViewById(R.id.start_button);
        startButton.setOnClickListener(view -> {
            String ipAddress = ((TextView) findViewById(R.id.ip_address)).getText().toString();
            String port = ((TextView) findViewById(R.id.port)).getText().toString();


            if (!ipAddress.equals("") && !port.equals("") && !streamRunning) {
                startButton.setText("STREAM RUNNING");

                try {
                    datagramSocket = new DatagramSocket(Integer.parseInt(port));
                } catch (SocketException e) {
                    e.printStackTrace();
                }

                streamRunning = true;
                handler.post(statusChecker);
            } else if (streamRunning) {
                handler.removeCallbacks(statusChecker);
                datagramSocket.close();
                streamRunning = false;
                startButton.setText("START STREAM");
            }

        });
    }

    @Override
    protected void onStop() {
        super.onStop();

        handler.removeCallbacks(statusChecker);
    }

    protected void onDestroy() {
        gstreamerLib.nativeFinalize();
        super.onDestroy();
    }

    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Timber.tag("GStreamer").d("Surface changed to format " + format + " width " + width + " height " + height);
        gstreamerLib.nativeSurfaceInit(holder.getSurface());
    }

    public void surfaceCreated(SurfaceHolder holder) {
        Timber.tag("GStreamer").d("Surface created: %s", holder.getSurface());
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        Timber.tag("GStreamer").d("Surface destroyed");
        gstreamerLib.nativeSurfaceFinalize();
    }

    // Called from native code. This sets the content of the TextView from the UI thread.
    public void setMessage(final String message) {
        final TextView tv = (TextView) this.findViewById(R.id.status_text);
        runOnUiThread(() -> tv.setText(message));
    }

    // Called from native code. Native code calls this once it has created its pipeline and
    // the main loop is running, so it is ready to accept commands.
    public void onGStreamerInitialized() {
        Timber.tag("GStreamer").d("Gst initialized");
        // Restore previous playing state
        gstreamerLib.nativePlay();
    }

    private Runnable statusChecker = new Runnable() {
        @Override
        public void run() {
            try {
                String ipAddress = ((TextView) findViewById(R.id.ip_address)).getText().toString();
                int port = Integer.parseInt(((TextView) findViewById(R.id.port)).getText().toString());
                int multiplier = ((Spinner) findViewById(R.id.speed_multiplier)).getSelectedItemPosition();

                JoystickView leftJoystick = findViewById(R.id.left);
                JoystickView rightJoystick = findViewById(R.id.right);

                int left = 100 - leftJoystick.getNormalizedY();
                left = mapToInterval(left, -500, 500, 0, 100);

                int right = 100 - rightJoystick.getNormalizedY();
                right = mapToInterval(right, -500, 500, 0, 100);

                int mSpeedMultiplier = Integer.parseInt(speedMultipliers[multiplier]);

                sendPacket(
                        ipAddress,
                        port,
                        left * mSpeedMultiplier,
                        right * mSpeedMultiplier
                );
            } finally {
                handler.postDelayed(this, 50);
            }
        }
    };

    @SuppressLint("DefaultLocale")
    private void sendPacket(String ipAddr, int port, int left, int right) {
        try {
            InetAddress receiverAddress = InetAddress.getByName(ipAddr);

            int leftDir = left > 0 ? 1 : 0;
            int rightDir = right > 0 ? 1 : 0;

            String buffer = String.format("s0:000,s1:000,m0:%04d,%01d,m1:%04d,%01d\n", Math.abs(left), leftDir, Math.abs(right), rightDir);

            Timber.tag("Walle").i(buffer);

            DatagramPacket datagramPacket = new DatagramPacket(buffer.getBytes(), buffer.getBytes().length, receiverAddress, port);

            datagramSocket.send(datagramPacket);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private int mapToInterval(int value, int newIntervalMin, int newIntervalMax, int oldIntervalMin, int oldIntervalMax) {
        int numerator = (value - oldIntervalMin) * (newIntervalMax - newIntervalMin);
        int denominator = oldIntervalMax - oldIntervalMin;

        return newIntervalMin + (numerator / denominator);
    }
}
