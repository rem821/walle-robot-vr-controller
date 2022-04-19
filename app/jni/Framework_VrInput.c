#include "Framework_VrInput.h"
#include <android/log.h>
#include <assert.h>

#define OVR_LOG_TAG "WalleVrController-VrInput"

#define DEBUG 1

#if !defined(ALOGE)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__)
#endif

#if !defined(ALOGV)
#if DEBUG
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, OVR_LOG_TAG, __VA_ARGS__)
#else
#define ALOGV(...)
#endif
#endif


int
FindInputDevice(const ovrDeviceID deviceID, ovrInputDevice *inputDevices, int inputDeviceCount) {
    for (int i = 0; i < (int) inputDeviceCount; ++i) {
        if (inputDevices[i].deviceId == deviceID) {
            return i;
        }
    }
    return -1;
}

void RemoveDevice(const ovrDeviceID deviceID, ovrInputDevice *inputDevices, int *inputDeviceCount) {
    int index = FindInputDevice(deviceID, inputDevices, *inputDeviceCount);
    if (index < 0) {
        return;
    }

    for (int i = index; i < *inputDeviceCount; i++) {
        inputDevices[index] = inputDevices[index + 1];
    }

    memset(&inputDevices[*inputDeviceCount], 0, sizeof(ovrInputDevice));
    *inputDeviceCount -= 1;
}

bool
IsDeviceTracked(const ovrDeviceID deviceID, ovrInputDevice *inputDevices, int inputDeviceCount) {
    return FindInputDevice(deviceID, inputDevices, inputDeviceCount) >= 0;
}

void EnumerateInputDevices(ovrMobile *ovr, ovrInputDevice *inputDevices, int *inputDeviceCount) {
    for (uint32_t deviceIndex = 0;; deviceIndex++) {
        ovrInputCapabilityHeader curCaps;

        if (vrapi_EnumerateInputDevices(ovr, deviceIndex, &curCaps) < 0) {
            //ALOGV("Input - No more devices!");
            break; // no more devices
        }

        if (!IsDeviceTracked(curCaps.DeviceID, inputDevices, *inputDeviceCount)) {
            //ALOGV("%s device tracked, ID = %u", *inputDeviceCount == 0 ? "first" : "second",
            //      curCaps.DeviceID);
            OnDeviceConnected(ovr, &curCaps, inputDevices, inputDeviceCount);
        }
    }
}

void OnDeviceConnected(ovrMobile *ovr, ovrInputCapabilityHeader *capsHeader,
                       ovrInputDevice *inputDevices, int *inputDeviceCount) {
    ovrInputDevice *device = calloc(sizeof(ovrInputDevice), 1);
    ovrResult result = ovrError_NotInitialized;

    // Currently the app doesn't support any other controller type
    if (capsHeader->Type == ovrControllerType_TrackedRemote) {
        ALOGV("%s controller connected, ID = %u", *inputDeviceCount == 0 ? "first" : "second",
              capsHeader->DeviceID);

        ovrInputTrackedRemoteCapabilities trackedCaps;
        trackedCaps.Header = *capsHeader;
        result = vrapi_GetInputDeviceCapabilities(ovr, &trackedCaps.Header);

        if (result == ovrSuccess) {
            device->deviceId = trackedCaps.Header.DeviceID;
            device->type = trackedCaps.Header.Type;
            device->caps = *capsHeader;
            device->trackedCaps = trackedCaps;
            device->isLeftHand = trackedCaps.ControllerCapabilities & ovrControllerCaps_LeftHand;
        }
    } else return;

    if (device->deviceId > 0) {
        inputDevices[*inputDeviceCount] = *device;
        *inputDeviceCount += 1;
        ALOGV("Added %s controller, id = %u => current count: %d",
              device->isLeftHand ? "left" : "right", capsHeader->DeviceID, *inputDeviceCount);
    }
}

void OnDeviceDisconnected(const ovrDeviceID deviceID, ovrInputDevice *inputDevices,
                          int *inputDeviceCount) {
    //ALOG("Controller disconnected, ID = %i", deviceID);
    RemoveDevice(deviceID, inputDevices, inputDeviceCount);
}

void
HandleInputFromInputDevices(ovrMobile *ovr, ovrInputDevice *inputDevices, int *inputDeviceCount,
                            double predictedDisplayTime,
                            ovrPoseInput *controllerInput) {

    for (int i = (int) *inputDeviceCount - 1; i >= 0; --i) {
        ovrInputDevice *device = &inputDevices[i];
        if (device == NULL) {
            assert(false); // this should never happen!
        }

        ovrDeviceID deviceID = device->deviceId;
        if (deviceID == ovrDeviceIdType_Invalid) {
            assert(deviceID != ovrDeviceIdType_Invalid);
        }

        if (device->type == ovrControllerType_TrackedRemote) {
            ovrTracking remoteTracking;
            ovrResult res = vrapi_GetInputTrackingState(ovr, deviceID, predictedDisplayTime,
                                                           &remoteTracking);

            if (res != ovrSuccess) {
                OnDeviceDisconnected(deviceID, inputDevices, inputDeviceCount);
                continue;
            }

            ovrInputStateTrackedRemote remoteInputState;
            remoteInputState.Header.ControllerType = device->type;

            ovrResult result;
            result = vrapi_GetCurrentInputState(ovr, deviceID, &remoteInputState.Header);

            if (result != ovrSuccess) {
                OnDeviceDisconnected(deviceID, inputDevices, inputDeviceCount);
                continue;
            }

            device->tracking = remoteTracking;

            if(device->isLeftHand) {
                controllerInput->leftX = remoteInputState.Joystick.x;
                controllerInput->leftY = remoteInputState.Joystick.y;
                controllerInput->x = remoteInputState.Buttons & ovrButton_X;
                controllerInput->y = remoteInputState.Buttons & ovrButton_Y;
            } else {
                controllerInput->rightX = remoteInputState.Joystick.x;
                controllerInput->rightY = remoteInputState.Joystick.y;
                controllerInput->a = remoteInputState.Buttons & ovrButton_A;
                controllerInput->b = remoteInputState.Buttons & ovrButton_B;
            }
        }
    }
}

void GetRelativeHeadPose(ovrRigidBodyPosef defaultHeadPose, ovrRigidBodyPosef currentHeadPose, ovrPoseInput *poseInput) {
    versor initial = {defaultHeadPose.Pose.Orientation.x,
                      defaultHeadPose.Pose.Orientation.y,
                      defaultHeadPose.Pose.Orientation.z,
                      defaultHeadPose.Pose.Orientation.w};
    versor inv;
    glm_quat_conjugate(initial, inv);

    versor current = {currentHeadPose.Pose.Orientation.x,
                      currentHeadPose.Pose.Orientation.y,
                      currentHeadPose.Pose.Orientation.z,
                      currentHeadPose.Pose.Orientation.w};

    versor quat_dest;
    glm_quat_mul(current, inv, quat_dest);

    mat4 mat_dest;
    glm_quat_mat4(quat_dest, mat_dest);

    vec3 dest;
    glm_euler_angles(mat_dest, dest);

    poseInput->headsetHorizontal = dest[1];
    poseInput->headsetVertical = dest[0];
}
