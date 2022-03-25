LOCAL_PATH := $(call my-dir)/../../..

include $(CLEAR_VARS)

LOCAL_MODULE := cglm
LOCAL_ARM_MODE := arm
LOCAL_ARM_NEON := true

include $(LOCAL_PATH)/../../cflags.mk

LOCAL_SRC_FILES := \
  src/clipspace/ortho_lh_no.c \
  src/clipspace/ortho_lh_zo.c \
  src/clipspace/ortho_rh_no.c \
  src/clipspace/ortho_rh_zo.c \
  src/clipspace/persp_lh_no.c \
  src/clipspace/persp_lh_zo.c \
  src/clipspace/persp_rh_no.c \
  src/clipspace/persp_rh_zo.c \
  src/clipspace/project_no.c  \
  src/clipspace/project_zo.c  \
  src/clipspace/view_lh_no.c \
  src/clipspace/view_lh_zo.c \
  src/clipspace/view_rh_no.c \
  src/clipspace/view_rh_zo.c \
  src/swift/empty.c \
  src/affine.c \
  src/affine2d.c \
  src/bezier.c \
  src/box.c \
  src/cam.c \
  src/curve.c \
  src/ease.c \
  src/euler.c \
  src/frustum.c \
  src/io.c \
  src/mat2.c \
  src/mat3.c \
  src/mat4.c \
  src/plane.c \
  src/project.c \
  src/quat.c \
  src/ray.c \
  src/sphere.c \
  src/vec2.c \
  src/vec3.c \
  src/vec4.c

LOCAL_CFLAGS	+= -Wno-shadow

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/src
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_EXPORT_LDLIBS += -lz

include $(BUILD_STATIC_LIBRARY)
