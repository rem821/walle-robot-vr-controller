//
// Created by Stanislav Svědiroh on 05.03.2022.
//
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h> // for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/native_window_jni.h> // for native window JNI
#include <android/input.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <GL/gl_format.h>

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"

#define OVR_LOG_TAG "WalleVrController"

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, OVR_LOG_TAG, __VA_ARGS__)

typedef struct {
    ovrJava Java;
    //TODO: ovrEgl Egl;
    ANativeWindow *NativeWindow;
    bool Resumed;
    ovrMobile *Ovr;
    //TODO: ovrScene Scene;
    long long FrameIndex;
    double DisplayTime;
    int SwapInterval;
    int CpuLevel;
    int GpuLevel;
    int MainThreadTid;
    int RenderThreadTid;
    bool GamePadBackButtonDown;
    //TODO: ovrRenderer Renderer;
    bool UseMultiview;
} ovrApp;

typedef struct {
    JavaVM *JavaVm;
    jobject ActivityObject;
    pthread_t Thread;
    //TODO: ovrMessageQueue MessageQueue;
    ANativeWindow *NativeWindow;
} ovrAppThread;

static void ovrApp_HandleVrModeChanges(ovrApp *app) {
    if (app->Resumed != false && app->NativeWindow != NULL) {
        if (app->Ovr == NULL) {
            ovrModeParms parms = vrapi_DefaultModeParms(&app->Java);
            // Must reset the FLAG_FULLSCREEN window flag when using a SurfaceView
            //TODOúparms.Flags |= VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;

            //TODO: parms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
            //TOOD: parms.Display = (size_t) app->Egl.Display;
            //TODO: parms.WindowSurface = (size_t) app->NativeWindow;
            //TODO: parms.ShareContext = (size_t) app->Egl.Context;

            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

            ALOGV("        vrapi_EnterVrMode()");

            app->Ovr = vrapi_EnterVrMode(&parms);

            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

            // If entering VR mode failed then the ANativeWindow was not valid.
            if (app->Ovr == NULL) {
                ALOGE("Invalid ANativeWindow!");
                app->NativeWindow = NULL;
            }

            // Set performance parameters once we have entered VR mode and have a valid ovrMobile.
            if (app->Ovr != NULL) {
                vrapi_SetClockLevels(app->Ovr, app->CpuLevel, app->GpuLevel);

                ALOGV("		vrapi_SetClockLevels( %d, %d )", app->CpuLevel, app->GpuLevel);

                vrapi_SetPerfThread(app->Ovr, VRAPI_PERF_THREAD_TYPE_MAIN, app->MainThreadTid);

                ALOGV("		vrapi_SetPerfThread( MAIN, %d )", app->MainThreadTid);

                vrapi_SetPerfThread(
                        app->Ovr, VRAPI_PERF_THREAD_TYPE_RENDERER, app->RenderThreadTid);

                ALOGV("		vrapi_SetPerfThread( RENDERER, %d )", app->RenderThreadTid);
            }
        }
    } else {
        if (app->Ovr != NULL) {
            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

            ALOGV("        vrapi_LeaveVrMode()");

            vrapi_LeaveVrMode(app->Ovr);
            app->Ovr = NULL;

            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));
        }
    }
}

static void ovrApp_Clear(ovrApp *app) {
    app->Java.Vm = NULL;
    app->Java.Env = NULL;
    app->Java.ActivityObject = NULL;
    app->NativeWindow = NULL;
    app->Resumed = false;
    app->Ovr = NULL;
    app->FrameIndex = 1;
    app->DisplayTime = 0;
    app->SwapInterval = 1;
    app->CpuLevel = 2;
    app->GpuLevel = 2;
    app->MainThreadTid = 0;
    app->RenderThreadTid = 0;
    app->GamePadBackButtonDown = false;
    app->UseMultiview = true;

    //TODO: ovrEgl_Clear(&app->Egl);
    //TODO: ovrScene_Clear(&app->Scene);
    //TODO: ovrRenderer_Clear(&app->Renderer);
}


void *AppThreadFunction(void *parm) {
    ovrAppThread *appThread = (ovrAppThread *) parm;

    ovrJava java;
    java.Vm = appThread->JavaVm;
    (*java.Vm)->AttachCurrentThread(java.Vm, &java.Env, NULL);
    java.ActivityObject = appThread->ActivityObject;

    // Note that AttachCurrentThread will reset the thread name.
    prctl(PR_SET_NAME, (long) "OVR::Main", 0, 0, 0);

    const ovrInitParms initParms = vrapi_DefaultInitParms(&java);
    int32_t initResult = vrapi_Initialize(&initParms);
    if (initResult != VRAPI_INITIALIZE_SUCCESS) {
        // If initialization failed, vrapi_* function calls will not be available.
        exit(0);
    }

    ALOGV("VrApi init successfully");

    //TODO: ovrEgl_CreateContext(&appState.Egl, NULL);

    //TODO: EglInitExtensions();

    //TODO: ovrRenderer_Create(&appState.Renderer, &java, appState.UseMultiview);

    ovrApp_HandleVrModeChanges(&appState);


    ovrApp appState;
    ovrApp_Clear(&appState);
    appState.Java = java;
}


static void ovrAppThread_Create(ovrAppThread *appThread, JNIEnv *env, jobject activityObject) {
    (*env)->GetJavaVM(env, &appThread->JavaVm);
    appThread->ActivityObject = (*env)->NewGlobalRef(env, activityObject);
    appThread->Thread = 0;
    appThread->NativeWindow = NULL;
    //TODO: ovrMessageQueue_Create(&appThread->MessageQueue);

    const int createErr = pthread_create(&appThread->Thread, NULL, AppThreadFunction, appThread);
    if (createErr != 0) {
        ALOGE("pthread_create returned %i", createErr);
    }
}

static void ovrAppThread_Destroy(ovrAppThread *appThread, JNIEnv *env) {
    pthread_join(appThread->Thread, NULL);
    (*env)->DeleteGlobalRef(env, appThread->ActivityObject);
    //TODO: ovrMessageQueue_Destroy(&appThread->MessageQueue);
}


//region JNI
/*
================================================================================

Activity lifecycle

================================================================================
*/
JNIEXPORT jlong JNICALL
Java_cz_walle_wallevrcontroller2_GLES3JNILib_onCreate(JNIEnv *env, jclass clazz, jobject activity) {
    ALOGV("    GLES3JNILib::onCreate()");

    ovrAppThread *appThread = (ovrAppThread *) malloc(sizeof(ovrAppThread));
    ovrAppThread_Create(appThread, env, activity);

    //TODO:ovrMessageQueue_Enable(&appThread->MessageQueue, true);
    //ovrMessage message;
    //ovrMessage_Init(&message, MESSAGE_ON_CREATE, MQ_WAIT_PROCESSED);
    //ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);

    return (jlong) ((size_t) appThread);
}

JNIEXPORT void JNICALL
Java_cz_walle_wallevrcontroller2_GLES3JNILib_onStart(JNIEnv *env, jclass clazz, jlong handle) {
    ALOGV("    GLES3JNILib::onStart()");
    ovrAppThread *appThread = (ovrAppThread *) ((size_t) handle);
    //TODO:ovrMessage message;
    //ovrMessage_Init(&message, MESSAGE_ON_START, MQ_WAIT_PROCESSED);
    //ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}


JNIEXPORT void JNICALL
Java_cz_walle_wallevrcontroller2_GLES3JNILib_onResume(JNIEnv *env, jclass clazz, jlong handle) {
    ALOGV("    GLES3JNILib::onResume()");
    ovrAppThread *appThread = (ovrAppThread *) ((size_t) handle);
    //TODO:ovrMessage message;
    //ovrMessage_Init(&message, MESSAGE_ON_RESUME, MQ_WAIT_PROCESSED);
    //ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL
Java_cz_walle_wallevrcontroller2_GLES3JNILib_onPause(JNIEnv *env, jclass clazz, jlong handle) {
    ALOGV("    GLES3JNILib::onPause()");
    ovrAppThread *appThread = (ovrAppThread *) ((size_t) handle);
    //TODO:ovrMessage message;
    //ovrMessage_Init(&message, MESSAGE_ON_PAUSE, MQ_WAIT_PROCESSED);
    //ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL
Java_cz_walle_wallevrcontroller2_GLES3JNILib_onStop(JNIEnv *env, jclass clazz, jlong handle) {
    ALOGV("    GLES3JNILib::onStop()");
    ovrAppThread *appThread = (ovrAppThread *) ((size_t) handle);
    //TODO:ovrMessage message;
    //ovrMessage_Init(&message, MESSAGE_ON_STOP, MQ_WAIT_PROCESSED);
    //ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}

JNIEXPORT void JNICALL
Java_cz_walle_wallevrcontroller2_GLES3JNILib_onDestroy(JNIEnv *env, jclass clazz, jlong handle) {
    ALOGV("    GLES3JNILib::onDestroy()");
    ovrAppThread *appThread = (ovrAppThread *) ((size_t) handle);
    //TODO:ovrMessage message;
    //ovrMessage_Init(&message, MESSAGE_ON_DESTROY, MQ_WAIT_PROCESSED);
    //ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
    //ovrMessageQueue_Enable(&appThread->MessageQueue, false);

    ovrAppThread_Destroy(appThread, env);
    free(appThread);
}

/*
================================================================================

Surface lifecycle

================================================================================
*/
JNIEXPORT void JNICALL
Java_cz_walle_wallevrcontroller2_GLES3JNILib_onSurfaceCreated(JNIEnv *env, jclass clazz,
                                                              jlong handle, jobject surface) {
    ALOGV("    GLES3JNILib::onSurfaceCreated()");
    ovrAppThread *appThread = (ovrAppThread *) ((size_t) handle);

    ANativeWindow *newNativeWindow = ANativeWindow_fromSurface(env, surface);
    if (ANativeWindow_getWidth(newNativeWindow) < ANativeWindow_getHeight(newNativeWindow)) {
        // An app that is relaunched after pressing the home button gets an initial surface with
        // the wrong orientation even though android:screenOrientation="landscape" is set in the
        // manifest. The choreographer callback will also never be called for this surface because
        // the surface is immediately replaced with a new surface with the correct orientation.
        ALOGE("        Surface not in landscape mode!");
    }

    ALOGV("        NativeWindow = ANativeWindow_fromSurface( env, surface )");
    appThread->NativeWindow = newNativeWindow;
    //TODO:ovrMessage message;
    //ovrMessage_Init(&message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED);
    //ovrMessage_SetPointerParm(&message, 0, appThread->NativeWindow);
    //ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}


JNIEXPORT void JNICALL
Java_cz_walle_wallevrcontroller2_GLES3JNILib_onSurfaceChanged(JNIEnv *env, jclass clazz,
                                                              jlong handle, jobject surface) {
    ALOGV("    GLES3JNILib::onSurfaceChanged()");
    ovrAppThread *appThread = (ovrAppThread *) ((size_t) handle);

    ANativeWindow *newNativeWindow = ANativeWindow_fromSurface(env, surface);
    if (ANativeWindow_getWidth(newNativeWindow) < ANativeWindow_getHeight(newNativeWindow)) {
        // An app that is relaunched after pressing the home button gets an initial surface with
        // the wrong orientation even though android:screenOrientation="landscape" is set in the
        // manifest. The choreographer callback will also never be called for this surface because
        // the surface is immediately replaced with a new surface with the correct orientation.
        ALOGE("        Surface not in landscape mode!");
    }

    if (newNativeWindow != appThread->NativeWindow) {
        if (appThread->NativeWindow != NULL) {
            //TODO:ovrMessage message;
            //ovrMessage_Init(&message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED);
            //ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
            ALOGV("        ANativeWindow_release( NativeWindow )");
            ANativeWindow_release(appThread->NativeWindow);
            appThread->NativeWindow = NULL;
        }
        if (newNativeWindow != NULL) {
            ALOGV("        NativeWindow = ANativeWindow_fromSurface( env, surface )");
            appThread->NativeWindow = newNativeWindow;
            //TODO:ovrMessage message;
            //ovrMessage_Init(&message, MESSAGE_ON_SURFACE_CREATED, MQ_WAIT_PROCESSED);
            //ovrMessage_SetPointerParm(&message, 0, appThread->NativeWindow);
            //ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
        }
    } else if (newNativeWindow != NULL) {
        ANativeWindow_release(newNativeWindow);
    }
}

JNIEXPORT void JNICALL
Java_cz_walle_wallevrcontroller2_GLES3JNILib_onSurfaceDestroyed(JNIEnv *env, jclass clazz,
                                                                jlong handle) {
    ALOGV("    GLES3JNILib::onSurfaceDestroyed()");
    ovrAppThread *appThread = (ovrAppThread *) ((size_t) handle);
    //TODO:ovrMessage message;
    //ovrMessage_Init(&message, MESSAGE_ON_SURFACE_DESTROYED, MQ_WAIT_PROCESSED);
    //ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
    ALOGV("        ANativeWindow_release( NativeWindow )");
    ANativeWindow_release(appThread->NativeWindow);
    appThread->NativeWindow = NULL;
}

/*
================================================================================

Input

================================================================================
*/

JNIEXPORT void JNICALL
Java_cz_walle_wallevrcontroller2_GLES3JNILib_onKeyEvent(JNIEnv *env, jclass clazz, jlong handle,
                                                        jint keyCode, jint action) {
    if (action == AKEY_EVENT_ACTION_UP) {
        ALOGV("    GLES3JNILib::onKeyEvent( %d, %d )", keyCode, action);
    }
    ovrAppThread *appThread = (ovrAppThread *) ((size_t) handle);
    //TODO:ovrMessage message;
    //ovrMessage_Init(&message, MESSAGE_ON_KEY_EVENT, MQ_WAIT_NONE);
    //ovrMessage_SetIntegerParm(&message, 0, keyCode);
    //ovrMessage_SetIntegerParm(&message, 1, action);
    //ovrMessageQueue_PostMessage(&appThread->MessageQueue, &message);
}
//endregion
