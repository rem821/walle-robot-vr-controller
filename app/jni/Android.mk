LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../cflags.mk

LOCAL_MODULE    := GStreamerModule
LOCAL_SRC_FILES := VrCubeWorld_SurfaceView.c
LOCAL_SHARED_LIBRARIES := gstreamer_android vrapi
LOCAL_LDLIBS := -lEGL -lGLESv3 -landroid -llog -lz

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../../SampleCommon/Src \
    $(LOCAL_PATH)/../../SampleFramework/Src \
    $(LOCAL_PATH)/../../VrApi/Include \
    $(LOCAL_PATH)/../../1stParty/OVR/Include \
    $(LOCAL_PATH)/../../1stParty/utilities/include \
    $(LOCAL_PATH)/../../3rdParty/stb/src \

include $(BUILD_SHARED_LIBRARY)

$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
$(call import-module,SampleFramework/Projects/Android/jni)

ifndef GSTREAMER_ROOT_ANDROID
$(error GSTREAMER_ROOT_ANDROID is not defined!)
endif


ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/armv7
else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/arm64
else ifeq ($(TARGET_ARCH_ABI),x86)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86
else ifeq ($(TARGET_ARCH_ABI),x86_64)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86_64
else
$(error Target arch ABI not supported: $(TARGET_ARCH_ABI))
endif

GSTREAMER_NDK_BUILD_PATH  := $(GSTREAMER_ROOT)/share/gst-android/ndk-build/
include $(GSTREAMER_NDK_BUILD_PATH)/plugins.mk
GSTREAMER_PLUGINS         := $(GSTREAMER_PLUGINS_CORE) $(GSTREAMER_PLUGINS_CODECS) $(GSTREAMER_PLUGINS_ENCODING) $(GSTREAMER_PLUGINS_NET) $(GSTREAMER_PLUGINS_PLAYBACK) $(GSTREAMER_PLUGINS_SYS) $(GSTREAMER_PLUGINS_EFFECTS) $(GSTREAMER_PLUGINS_VIS) $(GSTREAMER_PLUGINS_CAPTURE) $(GSTREAMER_PLUGINS_CODECS_RESTRICTED) $(GSTREAMER_PLUGINS_NET_RESTRICTED) $(GSTREAMER_PLUGINS_VULKAN) $(GSTREAMER_PLUGINS_GES)
GSTREAMER_EXTRA_DEPS      := gstreamer-video-1.0 gobject-2.0
GSTREAMER_EXTRA_LIBS      := -liconv
include $(GSTREAMER_NDK_BUILD_PATH)/gstreamer-1.0.mk
