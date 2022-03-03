package cz.walle.wallevrcontroller2;

public class VrInputJNILib {

    ControlActivity controlActivity;

    public VrInputJNILib(ControlActivity controlActivity) {
        this.controlActivity = controlActivity;
    }

    public native void nativeInit();
    public static native boolean nativeClassInit();

    public native void nativeGetPositions();

}
