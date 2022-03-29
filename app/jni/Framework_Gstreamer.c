#include "Framework_Gstreamer.h"

#define OVR_LOG_TAG "WalleVrController-Gstreamer"

GST_DEBUG_CATEGORY_STATIC (debug_category);
#define GST_CAT_DEFAULT debug_category

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

static void error_cb(GstBus *bus, GstMessage *msg, GstreamerInstance *data) {
    GError *err;
    gchar *debug_info;

    gst_message_parse_error(msg, &err, &debug_info);

    ALOGE("Error received from element %s: %s", GST_OBJECT_NAME(msg->src), err->message);

    gst_element_set_state(data->pipeline, GST_STATE_NULL);
}

static void state_changed_cb(GstBus *bus, GstMessage *msg, GstreamerInstance *data) {
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
    /* Only pay attention to messages coming from the pipeline, not its children */
    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(data->pipeline)) {
        ALOGV("State changed to %s", gst_element_state_get_name(new_state));
    }
}

static GstFlowReturn new_sample_cb(GstElement *sink, GstreamerInstance *data) {
    GstSample *sample;
    GstBuffer *buffer;
    GstMapInfo mapInfo;
    /* Retrieve the buffer */
    g_signal_emit_by_name(sink, "pull-sample", &sample);
    if (sample) {
        //ALOGV("GStreamer new image just arrived!");

        buffer = gst_sample_get_buffer(sample);
        gst_buffer_map(buffer, &mapInfo, GST_MAP_READ);
        data->dataHandle = mapInfo.data;
        data->memorySize = mapInfo.size;

        gst_sample_unref (sample);
        gst_buffer_unmap(buffer, &mapInfo);
        return GST_FLOW_OK;
    }

    return GST_FLOW_ERROR;
}

/* Main method for the native code. This is executed on its own thread. */
static void *
app_function(GstreamerInstance *data) {
    GstBus *bus;
    GSource *bus_source;
    GError *error = NULL;

    /* Create our own GLib Main Context and make it the default one */
    data->context = g_main_context_new();
    g_main_context_push_thread_default(data->context);

    /* Build pipeline */
    GstElement *pipeline = gst_parse_launch("rtspsrc location=rtsp://192.168.1.239:8556/right latency=150 drop-on-latency= ! application/x-rtp,encoding-name=H264 ! decodebin ! videoconvert ! video/x-raw,width=1920,height=1080,format=RGBA ! appsink emit-signals=true name=wallesink sync=false", &error);
    //GstElement *pipeline = gst_parse_launch("videotestsrc pattern=ball ! video/x-raw,width=1920,height=1080,format=RGBA ! appsink emit-signals=true name=wallesink", &error);
    data->pipeline = pipeline;

    if (error) {
        ALOGE("Unable to build pipeline: %s", error->message);
        return NULL;
    }

    GstElement *appsink = gst_bin_get_by_name(GST_BIN(pipeline), "wallesink");
    /* Set the pipeline to READY, so it can already accept a window handle, if we have one */
    gst_element_set_state(data->pipeline, GST_STATE_READY);

    /* Instruct the bus to emit signals for each received message, and connect to the interesting signals */
    bus = gst_element_get_bus(data->pipeline);
    bus_source = gst_bus_create_watch(bus);
    g_source_set_callback(bus_source, (GSourceFunc) gst_bus_async_signal_func, NULL, NULL);
    g_source_attach(bus_source, data->context);
    g_source_unref(bus_source);
    g_signal_connect(G_OBJECT(bus), "message::error", (GCallback) error_cb, data);
    g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback) state_changed_cb, data);
    g_signal_connect(G_OBJECT(appsink), "new-sample", (GCallback)new_sample_cb, data);
    gst_object_unref(bus);

    /* Create a GLib Main Loop and set it to run */
    ALOGV("Entering main loop... (CustomData:%p)", data);
    data->main_loop = g_main_loop_new(data->context, FALSE);
    gst_element_set_state(data->pipeline, GST_STATE_PLAYING);

    g_main_loop_run(data->main_loop);
    ALOGV("Exited main loop");
    g_main_loop_unref(data->main_loop);
    data->main_loop = NULL;

    /* Free resources */
    g_main_context_pop_thread_default(data->context);
    g_main_context_unref(data->context);
    gst_element_set_state(data->pipeline, GST_STATE_NULL);
    gst_object_unref(data->video_sink);
    gst_object_unref(data->pipeline);

    return NULL;
}

/* Set pipeline to PLAYING state */
void gstreamer_play(GstreamerInstance *gstreamerInstance) {
    ALOGV("Setting state to PLAYING");
    gst_element_set_state(gstreamerInstance->pipeline, GST_STATE_PLAYING);
}

void gstreamer_initialize(GstreamerInstance *gstreamerInstance) {
    GST_DEBUG_CATEGORY_INIT(debug_category, "WalleVrController", 0, "Controller for Walle");
    gst_debug_set_threshold_for_name("WalleVrController", GST_LEVEL_DEBUG);

    pthread_create(&gstreamerInstance->gst_app_thread, NULL, &app_function, gstreamerInstance);
}
