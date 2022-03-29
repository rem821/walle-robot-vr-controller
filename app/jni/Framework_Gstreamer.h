#include <string.h>
#include <stdint.h>
#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <pthread.h>

#ifndef WALLEVRCONTROLLER2_FRAMEWORK_GSTREAMER_H
#define WALLEVRCONTROLLER2_FRAMEWORK_GSTREAMER_H

/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct {
    GstElement *pipeline;         /* The running pipeline */
    pthread_t gst_app_thread;
    GMainContext *context;        /* GLib context used to run the main loop */
    GMainLoop *main_loop;         /* GLib main loop */
    gboolean initialized;         /* To avoid informing the UI multiple times about the initialization */
    GstElement *video_sink;       /* The video sink element which receives XOverlay commands */
    unsigned long memorySize;
    void *dataHandle;
} GstreamerInstance;

void gstreamer_play(GstreamerInstance *gstreamerInstance);

void gstreamer_initialize(GstreamerInstance *gstreamerInstance);


#endif //WALLEVRCONTROLLER2_FRAMEWORK_GSTREAMER_H
