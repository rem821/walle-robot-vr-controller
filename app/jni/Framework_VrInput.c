#include "Framework_VrInput.h"
#include <android/log.h>

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

void EnumerateInputDevices(ovrMobile *ovr, ovrInputDevice *inputDevices, int inputDeviceCount) {
    for (uint32_t deviceIndex = 0;; deviceIndex++) {
        ovrInputCapabilityHeader curCaps;

        if (vrapi_EnumerateInputDevices(ovr, deviceIndex, &curCaps) < 0) {
            //ALOGV("Input - No more devices!");
            break; // no more devices
        }

        if (!IsDeviceTracked(curCaps.DeviceID, inputDevices, inputDeviceCount)) {
            //ALOG("Input -      tracked");
            OnDeviceConnected(ovr, curCaps);
        }
    }
}

void OnDeviceConnected(ovrMobile *ovr, ovrInputCapabilityHeader capsHeader) {
    ovrInputDevice *device = malloc(sizeof(ovrInputDevice));
    ovrResult result = ovrError_NotInitialized;

    // Currently the app doesn't support any other controller type
    if (capsHeader.Type == ovrControllerType_TrackedRemote) {
        //ALOG("Controller connected, ID = %u", capsHeader.DeviceID);

        ovrInputTrackedRemoteCapabilities trackedCaps;
        trackedCaps.Header = capsHeader;
        result = vrapi_GetInputDeviceCapabilities(ovr, &trackedCaps.Header);

        if (result == ovrSuccess) {
            device->trackedCaps = trackedCaps;
            device->isLeftHand = trackedCaps.ControllerCapabilities & ovrControllerCaps_LeftHand;
        }
        if (result != ovrSuccess) {
            //ALOG("vrapi_GetInputDeviceCapabilities: Error %i", result);
        }
    } else return;

    if (result != ovrSuccess) {
        //ALOG("vrapi_GetInputDeviceCapabilities: Error %i", result);
    }
    if (device != NULL) {
        //ALOG("Added device '%s', id = %u", device->GetName(), capsHeader.DeviceID);
        //InputDevices.push_back(device);
    } else {
        // ALOG("Device creation failed for id = %u", capsHeader.DeviceID);
    }
}

void OnDeviceDisconnected(const ovrDeviceID deviceID, ovrInputDevice *inputDevices,
                          int *inputDeviceCount) {
    //ALOG("Controller disconnected, ID = %i", deviceID);
    RemoveDevice(deviceID, inputDevices, inputDeviceCount);
}
