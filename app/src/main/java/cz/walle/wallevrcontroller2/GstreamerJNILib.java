package cz.walle.wallevrcontroller2;

public class GstreamerJNILib {

    ControlActivity controlActivity;

    public GstreamerJNILib(ControlActivity controlActivity) {
       this.controlActivity = controlActivity;
    }

    public native void nativeInit();     // Initialize native code, build pipeline, etc

    public native void nativeFinalize(); // Destroy pipeline and shutdown native code

    public native void nativePlay();     // Set pipeline to PLAYING

    public native void nativePause();    // Set pipeline to PAUSED

    public static native boolean nativeClassInit(); // Initialize native class: cache Method IDs for callbacks

    public native void nativeSurfaceInit(Object surface);

    public native void nativeSurfaceFinalize();

    public long native_custom_data;      // Native code will use this to keep private data

    // Called from native code. This sets the content of the TextView from the UI thread.
    private void setMessage(final String message) {
        this.controlActivity.setMessage(message);
    }

    // Called from native code. Native code calls this once it has created its pipeline and
    // the main loop is running, so it is ready to accept commands.
    private void onGStreamerInitialized() {
        this.controlActivity.onGStreamerInitialized();
    }

}
