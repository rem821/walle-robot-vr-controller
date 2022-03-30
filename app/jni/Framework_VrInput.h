#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"

#ifndef WALLEVRCONTROLLER2_FRAMEWORK_VRINPUT_H
#define WALLEVRCONTROLLER2_FRAMEWORK_VRINPUT_H

typedef struct {
    ovrInputCapabilityHeader caps;
    ovrControllerType type;
    ovrDeviceID deviceId;
    ovrTracking tracking;
    ovrInputTrackedRemoteCapabilities trackedCaps;
    bool isLeftHand;
} ovrInputDevice;

int
FindInputDevice(const ovrDeviceID deviceID, ovrInputDevice *inputDevices, int inputDeviceCount);

void RemoveDevice(const ovrDeviceID deviceID, ovrInputDevice *inputDevices, int *inputDeviceCount);

void EnumerateInputDevices(ovrMobile *ovr, ovrInputDevice *inputDevices, int inputDeviceCount);

void OnDeviceConnected(ovrMobile *ovr, ovrInputCapabilityHeader capsHeader);

bool
IsDeviceTracked(const ovrDeviceID deviceID, ovrInputDevice *inputDevices, int inputDeviceCount);

void OnDeviceDisconnected(const ovrDeviceID deviceID, ovrInputDevice *inputDevices,
                          int *inputDeviceCount);

#endif //WALLEVRCONTROLLER2_FRAMEWORK_VRINPUT_H
