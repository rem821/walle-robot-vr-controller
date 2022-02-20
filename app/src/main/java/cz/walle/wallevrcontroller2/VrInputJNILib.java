package cz.walle.wallevrcontroller2;

public class VrInputJNILib {

    ControlActivity controlActivity;

    public VrInputJNILib(ControlActivity controlActivity) {
       this.controlActivity = controlActivity;
    }

    public native void nativeInit();     // Initialize native code, build pipeline, etc


}
