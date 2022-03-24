/************************************************************************************

Filename    :   VrCubeWorld_Vulkan.c
Content     :   This sample demonstrates how to use the Vulkan VrApi.
                This sample uses the Android NativeActivity class. This sample does
                not use the application framework.
                This sample only uses the VrApi.
Created     :   October, 2017
Authors     :   J.M.P. van Waveren, Gloria Kennickell

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h> // for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/native_window_jni.h> // for native window JNI
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <limits.h>

#include "Framework_Vulkan.h"

#include "VrApi.h"
#include "VrApi_Vulkan.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"

#define DEBUG 1
#define OVR_LOG_TAG "VrCubeWorldVk"

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

static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;
static ovrSampleCount SAMPLE_COUNT = OVR_SAMPLE_COUNT_1;

const uint32_t VERTICES_LENGTH = 3;
const ovrVertex vertices[] = {
        {{-1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}},
        {{0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{-1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
};

typedef struct {
    char *shaderFile;
    int shaderFileLength;
} ovrShaderFile;

static void
readShaderFile(AAssetManager *aassetManager, char *filename, ovrShaderFile *shaderFile) {
    AAsset *file = AAssetManager_open(aassetManager, filename, AASSET_MODE_BUFFER);
    shaderFile->shaderFileLength = (int) AAsset_getLength(file);
    shaderFile->shaderFile = malloc(sizeof(char) * shaderFile->shaderFileLength);
    AAsset_read(file, shaderFile->shaderFile, shaderFile->shaderFileLength);
}

/*
================================================================================

ovrSwapChain

================================================================================
*/

typedef struct {
    int SwapChainLength;
    ovrTextureSwapChain *SwapChain;
    VkImage *ColorTextures;
} ovrColorSwapChain;

static bool ovrColorSwapChain_Init(
        ovrColorSwapChain *swapChain,
        const VkFormat colorFormat,
        const int width,
        const int height) {
    swapChain->SwapChain = vrapi_CreateTextureSwapChain3(VRAPI_TEXTURE_TYPE_2D,
                                                         colorFormat,
                                                         width,
                                                         height,
                                                         1,
                                                         3);
    swapChain->SwapChainLength = vrapi_GetTextureSwapChainLength(swapChain->SwapChain);

    swapChain->ColorTextures = (VkImage *) malloc(swapChain->SwapChainLength * sizeof(VkImage));

    for (int i = 0; i < swapChain->SwapChainLength; i++) {
        swapChain->ColorTextures[i] = vrapi_GetTextureSwapChainBufferVulkan(swapChain->SwapChain,
                                                                            i);
    }

    return true;
}

static bool ovrColorSwapChain_Destroy(ovrColorSwapChain *swapChain) {
    // Don't call vrapi destroy because the images are used by the framebuffer
    free(swapChain->ColorTextures);
    return true;
}

/*
================================================================================

ovrFramebuffer

================================================================================
*/

typedef struct {
    int Width;
    int Height;
    ovrSampleCount SampleCount;
    int TextureSwapChainLength;
    int TextureSwapChainIndex;
    ovrTextureSwapChain *ColorTextureSwapChain;
    ovrVkFramebuffer Framebuffer;
} ovrFrameBuffer;

static void ovrFramebuffer_Clear(ovrFrameBuffer *frameBuffer) {
    frameBuffer->Width = 0;
    frameBuffer->Height = 0;
    frameBuffer->SampleCount = OVR_SAMPLE_COUNT_1;
    frameBuffer->TextureSwapChainLength = 0;
    frameBuffer->TextureSwapChainIndex = 0;
    frameBuffer->ColorTextureSwapChain = NULL;

    memset(&frameBuffer->Framebuffer, 0, sizeof(ovrVkFramebuffer));
}

static bool ovrFramebuffer_Create(
        ovrVkContext *context,
        ovrFrameBuffer *frameBuffer,
        ovrVkRenderPass *renderPass,
        ovrColorSwapChain *swapChain,
        const VkFormat colorFormat,
        const int width,
        const int height) {
    assert(
            width >= 1 &&
            width <=
            (int) context->device->physicalDeviceProperties.properties.limits.maxFramebufferWidth);
    assert(
            height >= 1 &&
            height <=
            (int) context->device->physicalDeviceProperties.properties.limits.maxFramebufferHeight);

    frameBuffer->Width = width;
    frameBuffer->Height = height;
    frameBuffer->SampleCount = renderPass->sampleCount;
    frameBuffer->ColorTextureSwapChain = swapChain->SwapChain;
    frameBuffer->TextureSwapChainLength = swapChain->SwapChainLength;

    memset(&frameBuffer->Framebuffer, 0, sizeof(ovrVkFramebuffer));

    frameBuffer->Framebuffer.colorImageViews =
            (VkImageView *) malloc(frameBuffer->TextureSwapChainLength * sizeof(VkImageView));
    frameBuffer->Framebuffer.framebuffers =
            (VkFramebuffer *) malloc(frameBuffer->TextureSwapChainLength * sizeof(VkFramebuffer));
    frameBuffer->Framebuffer.renderPass = renderPass;
    frameBuffer->Framebuffer.width = width;
    frameBuffer->Framebuffer.height = height;
    frameBuffer->Framebuffer.numLayers = 1;
    frameBuffer->Framebuffer.numBuffers = frameBuffer->TextureSwapChainLength;
    frameBuffer->Framebuffer.currentBuffer = 0;
    frameBuffer->Framebuffer.currentLayer = 0;

    for (int i = 0; i < frameBuffer->TextureSwapChainLength; i++) {
        // Create the ovrVkTexture from the texture swapchain.
        VkImageView *imageView = &frameBuffer->Framebuffer.colorImageViews[i];

        // create a view
        VkImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = NULL;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.image = swapChain->ColorTextures[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = colorFormat;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        VK(context->device->vkCreateImageView(
                context->device->device, &imageViewCreateInfo, VK_ALLOCATOR, imageView));
    }

    for (int i = 0; i < frameBuffer->TextureSwapChainLength; i++) {
        VkImageView attachments[1];

        attachments[0] = frameBuffer->Framebuffer.colorImageViews[i];

        VkFramebufferCreateInfo framebufferCreateInfo;
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.pNext = NULL;
        framebufferCreateInfo.flags = 0;
        framebufferCreateInfo.renderPass = renderPass->renderPass;
        framebufferCreateInfo.attachmentCount = 1;
        framebufferCreateInfo.pAttachments = attachments;
        framebufferCreateInfo.width = frameBuffer->Framebuffer.width;
        framebufferCreateInfo.height = frameBuffer->Framebuffer.height;
        framebufferCreateInfo.layers = 1;

        VK(context->device->vkCreateFramebuffer(
                context->device->device,
                &framebufferCreateInfo,
                VK_ALLOCATOR,
                &frameBuffer->Framebuffer.framebuffers[i]));
    }

    return true;
}

static void ovrFramebuffer_Destroy(ovrVkContext *context, ovrFrameBuffer *frameBuffer) {
    for (int i = 0; i < frameBuffer->TextureSwapChainLength; i++) {
        if (frameBuffer->Framebuffer.framebuffers != NULL) {
            VC(context->device->vkDestroyFramebuffer(
                    context->device->device, frameBuffer->Framebuffer.framebuffers[i],
                    VK_ALLOCATOR));
        }
    }

    free(frameBuffer->Framebuffer.framebuffers);
    free(frameBuffer->Framebuffer.colorImageViews);

    vrapi_DestroyTextureSwapChain(frameBuffer->ColorTextureSwapChain);

    ovrFramebuffer_Clear(frameBuffer);
}

/*
================================================================================

ovrScene

================================================================================
*/

typedef struct {
    ovrVkRenderPass RenderPassSingleView;
    ovrVkCommandBuffer EyeCommandBuffer[VRAPI_FRAME_LAYER_EYE_MAX];
    ovrFrameBuffer Framebuffer[VRAPI_FRAME_LAYER_EYE_MAX];
    ovrBuffer VertexBuffer[VRAPI_FRAME_LAYER_EYE_MAX];
    int NumEyes;
} ovrRenderer;

typedef struct {
    bool CreatedScene;
    ovrVkGraphicsProgram Program;
    ovrVkGraphicsPipeline Pipelines;
} ovrScene;

static void ovrScene_Clear(ovrScene *scene) {
    scene->CreatedScene = false;
    memset(&scene->Program, 0, sizeof(ovrVkGraphicsProgram));
    memset(&scene->Pipelines, 0, sizeof(ovrVkGraphicsPipeline));
}

static void ovrScene_Create(AAssetManager *aassetManager, ovrVkContext *context, ovrScene *scene,
                            ovrRenderer *renderer) {
    ovrShaderFile vertexShaderFile;
    readShaderFile(aassetManager, "shaders/shader.vert.spv", &vertexShaderFile);

    ovrShaderFile fragmentShaderFile;
    readShaderFile(aassetManager, "shaders/shader.frag.spv", &fragmentShaderFile);

    ovrVkGraphicsProgram_Create(
            context,
            &scene->Program,
            vertexShaderFile.shaderFile,
            vertexShaderFile.shaderFileLength,
            fragmentShaderFile.shaderFile,
            fragmentShaderFile.shaderFileLength);

    // Set up the graphics pipeline.
    ovrVkGraphicsPipelineParms pipelineParms;
    ovrVkGraphicsPipelineParms_Init(&pipelineParms);
    pipelineParms.renderPass = &renderer->RenderPassSingleView;
    pipelineParms.program = &scene->Program;

    // ROP state.
    pipelineParms.rop.blendEnable = false;
    pipelineParms.rop.redWriteEnable = true;
    pipelineParms.rop.blueWriteEnable = true;
    pipelineParms.rop.greenWriteEnable = true;
    pipelineParms.rop.alphaWriteEnable = false;
    pipelineParms.rop.frontFace = OVR_FRONT_FACE_CLOCKWISE;
    pipelineParms.rop.cullMode = OVR_CULL_MODE_BACK;
    pipelineParms.rop.depthCompare = OVR_COMPARE_OP_LESS_OR_EQUAL;
    pipelineParms.rop.blendColor.x = 0.0f;
    pipelineParms.rop.blendColor.y = 0.0f;
    pipelineParms.rop.blendColor.z = 0.0f;
    pipelineParms.rop.blendColor.w = 0.0f;
    pipelineParms.rop.blendOpColor = OVR_BLEND_OP_ADD;
    pipelineParms.rop.blendSrcColor = OVR_BLEND_FACTOR_ONE;
    pipelineParms.rop.blendDstColor = OVR_BLEND_FACTOR_ZERO;
    pipelineParms.rop.blendOpAlpha = OVR_BLEND_OP_ADD;
    pipelineParms.rop.blendSrcAlpha = OVR_BLEND_FACTOR_ONE;
    pipelineParms.rop.blendDstAlpha = OVR_BLEND_FACTOR_ZERO;

    const ovrScreenRect screenRect =
            ovrVkFramebuffer_GetRect(&renderer->Framebuffer[0].Framebuffer);
    ovrVkGraphicsPipeline_Create(context, &scene->Pipelines, &pipelineParms, screenRect);

    scene->CreatedScene = true;
}

static void ovrScene_Destroy(ovrVkContext *context, ovrScene *scene) {
    ovrVkContext_WaitIdle(context);

    ovrVkGraphicsPipeline_Destroy(context, &scene->Pipelines);

    ovrVkGraphicsProgram_Destroy(context, &scene->Program);

    scene->CreatedScene = false;
}

static void ovrScene_Render(ovrVkCommandBuffer *commandBuffer, ovrBuffer *vertexBuffer, ovrScene *scene) {
    ovrVkGraphicsCommand command;
    ovrVkGraphicsCommand_Init(&command);
    ovrVkGraphicsCommand_SetPipeline(&command, &scene->Pipelines);

    ovrVkCommandBuffer_SubmitGraphicsCommand(commandBuffer, vertexBuffer, &command, VERTICES_LENGTH);
}

/*
================================================================================

ovrRenderer

================================================================================
*/

static void ovrRenderer_Clear(ovrRenderer *renderer) {
    memset(&renderer->RenderPassSingleView, 0, sizeof(ovrVkRenderPass));

    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        memset(&renderer->EyeCommandBuffer[eye], 0, sizeof(ovrVkCommandBuffer));

        ovrFramebuffer_Clear(&renderer->Framebuffer[eye]);
    }

    renderer->NumEyes = VRAPI_FRAME_LAYER_EYE_MAX;
}

static void ovrRenderer_Create(ovrRenderer *renderer, ovrVkContext *context, const ovrJava *java) {
    renderer->NumEyes = VRAPI_FRAME_LAYER_EYE_MAX;

    // Get swapchain images from vrapi first so that we know what attachments to use for the
    // renderpass
    ovrColorSwapChain colorSwapChains[renderer->NumEyes];
    for (int eye = 0; eye < renderer->NumEyes; eye++) {
        ovrColorSwapChain_Init(
                &colorSwapChains[eye],
                VK_FORMAT_R8G8B8A8_UNORM,
                vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH),
                vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT)
        );
    }

    ovrVector4f clearColor = {0.125f, 0.0f, 0.125f, 1.0f};
    int flags = OVR_RENDERPASS_FLAG_CLEAR_COLOR_BUFFER;
    ovrVkRenderPass_Create(
            context,
            &renderer->RenderPassSingleView,
            OVR_SURFACE_COLOR_FORMAT_R8G8B8A8,
            SAMPLE_COUNT,
            OVR_RENDERPASS_TYPE_INLINE,
            flags,
            &clearColor);

    for (int eye = 0; eye < renderer->NumEyes; eye++) {
        ovrFramebuffer_Create(
                context,
                &renderer->Framebuffer[eye],
                &renderer->RenderPassSingleView,
                &colorSwapChains[eye],
                VK_FORMAT_R8G8B8A8_UNORM,
                vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH),
                vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT));
        ovrColorSwapChain_Destroy(&colorSwapChains[eye]);

        ovrBuffer_Vertex_Create(context, vertices, VERTICES_LENGTH, &renderer->VertexBuffer[eye]);
        ovrVkCommandBuffer_Create(
                context,
                &renderer->EyeCommandBuffer[eye],
                OVR_COMMAND_BUFFER_TYPE_PRIMARY,
                ovrVkFramebuffer_GetBufferCount(&renderer->Framebuffer[eye].Framebuffer));
    }
}

static void ovrRenderer_Destroy(ovrRenderer *renderer, ovrVkContext *context) {
    for (int eye = 0; eye < renderer->NumEyes; eye++) {
        ovrFramebuffer_Destroy(context, &renderer->Framebuffer[eye]);

        ovrVkCommandBuffer_Destroy(context, &renderer->EyeCommandBuffer[eye]);
    }

    ovrVkRenderPass_Destroy(context, &renderer->RenderPassSingleView);
}

static ovrLayerProjection2 ovrRenderer_RenderFrame(
        ovrRenderer *renderer,
        long long frameIndex,
        ovrScene *scene,
        const ovrTracking2 *tracking) {

    // Render the scene.
    for (int eye = 0; eye < renderer->NumEyes; eye++) {
        const ovrScreenRect screenRect =
                ovrVkFramebuffer_GetRect(&renderer->Framebuffer[eye].Framebuffer);

        ovrVkCommandBuffer_BeginPrimary(&renderer->EyeCommandBuffer[eye]);
        ovrVkCommandBuffer_BeginFramebuffer(
                &renderer->EyeCommandBuffer[eye],
                &renderer->Framebuffer[eye].Framebuffer,
                0 /*eye*/);

        ovrVkCommandBuffer_BeginRenderPass(
                &renderer->EyeCommandBuffer[eye],
                &renderer->RenderPassSingleView,
                &renderer->Framebuffer[eye].Framebuffer,
                &screenRect);

        ovrScene_Render(&renderer->EyeCommandBuffer[eye], &renderer->VertexBuffer[eye], scene);

        ovrVkCommandBuffer_EndRenderPass(
                &renderer->EyeCommandBuffer[eye], &renderer->RenderPassSingleView);

        ovrVkCommandBuffer_EndFramebuffer(
                &renderer->EyeCommandBuffer[eye],
                &renderer->Framebuffer[eye].Framebuffer,
                0 /*eye*/);
        ovrVkCommandBuffer_EndPrimary(&renderer->EyeCommandBuffer[eye]);

        ovrVkCommandBuffer_SubmitPrimary(&renderer->EyeCommandBuffer[eye]);
    }

    ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
    layer.HeadPose = tracking->HeadPose;
    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        layer.Textures[eye].ColorSwapChain =
                renderer->Framebuffer[eye].ColorTextureSwapChain;
        layer.Textures[eye].SwapChainIndex =
                renderer->Framebuffer[eye].Framebuffer.currentBuffer;
    }

    return layer;
}

/*
================================================================================

ovrApp

================================================================================
*/

typedef struct {
    ovrJava Java;
    ANativeWindow *NativeWindow;
    bool Resumed;
    ovrVkDevice Device;
    ovrVkContext Context;
    ovrMobile *Ovr;
    ovrScene Scene;
    long long FrameIndex;
    double DisplayTime;
    int SwapInterval;
    int CpuLevel;
    int GpuLevel;
    int MainThreadTid;
    int RenderThreadTid;
    ovrRenderer Renderer;
} ovrApp;

static void ovrApp_Clear(ovrApp *app) {
    app->Java.Vm = NULL;
    app->Java.Env = NULL;
    app->Java.ActivityObject = NULL;
    app->NativeWindow = NULL;
    app->Resumed = false;
    memset(&app->Device, 0, sizeof(ovrVkDevice));
    memset(&app->Context, 0, sizeof(ovrVkContext));
    app->Ovr = NULL;
    app->FrameIndex = 1;
    app->DisplayTime = 0;
    app->SwapInterval = 1;
    app->CpuLevel = 2;
    app->GpuLevel = 2;
    app->MainThreadTid = 0;
    app->RenderThreadTid = 0;

    ovrScene_Clear(&app->Scene);
    ovrRenderer_Clear(&app->Renderer);
}

static void ovrApp_HandleVrModeChanges(ovrApp *app) {
    if (app->Resumed != false && app->NativeWindow != NULL) {
        if (app->Ovr == NULL) {
            ovrModeParmsVulkan parms =
                    vrapi_DefaultModeParmsVulkan(&app->Java,
                                                 (unsigned long long) app->Context.queue);
            // No need to reset the FLAG_FULLSCREEN window flag when using a View
            parms.ModeParms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;

            parms.ModeParms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
            parms.ModeParms.WindowSurface = (size_t) app->NativeWindow;
            // Leave explicit egl objects defaulted.
            parms.ModeParms.Display = 0;
            parms.ModeParms.ShareContext = 0;

            ALOGV("        vrapi_EnterVrMode()");

            app->Ovr = vrapi_EnterVrMode((ovrModeParms *) &parms);

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
            ALOGV("        vrapi_LeaveVrMode()");

            vrapi_LeaveVrMode(app->Ovr);
            app->Ovr = NULL;
        }
    }
}

static void ovrApp_HandleInput(ovrApp *app) {}

static void ovrApp_HandleVrApiEvents(ovrApp *app) {
    ovrEventDataBuffer eventDataBuffer = {};

    // Poll for VrApi events
    for (;;) {
        ovrEventHeader *eventHeader = (ovrEventHeader *) (&eventDataBuffer);
        ovrResult res = vrapi_PollEvent(eventHeader);
        if (res != ovrSuccess) {
            break;
        }

        switch (eventHeader->EventType) {
            case VRAPI_EVENT_DATA_LOST:
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_DATA_LOST");
                break;
            case VRAPI_EVENT_VISIBILITY_GAINED:
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_GAINED");
                break;
            case VRAPI_EVENT_VISIBILITY_LOST:
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_LOST");
                break;
            case VRAPI_EVENT_FOCUS_GAINED:
                // FOCUS_GAINED is sent when the application is in the foreground and has
                // input focus. This may be due to a system overlay relinquishing focus
                // back to the application.
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_GAINED");
                break;
            case VRAPI_EVENT_FOCUS_LOST:
                // FOCUS_LOST is sent when the application is no longer in the foreground and
                // therefore does not have input focus. This may be due to a system overlay taking
                // focus from the application. The application should take appropriate action when
                // this occurs.
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_LOST");
                break;
            default:
                ALOGV("vrapi_PollEvent: Unknown event");
                break;
        }
    }
}

/*
================================================================================

Native Activity

================================================================================
*/

/**
 * Process the next main command.
 */
static void app_handle_cmd(struct android_app *app, int32_t cmd) {
    ovrApp *appState = (ovrApp *) app->userData;

    switch (cmd) {
        // There is no APP_CMD_CREATE. The ANativeActivity creates the
        // application thread from onCreate(). The application thread
        // then calls android_main().
        case APP_CMD_START: {
            ALOGV("onStart()");
            ALOGV("    APP_CMD_START");
            break;
        }
        case APP_CMD_RESUME: {
            ALOGV("onResume()");
            ALOGV("    APP_CMD_RESUME");
            appState->Resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            ALOGV("onPause()");
            ALOGV("    APP_CMD_PAUSE");
            appState->Resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            ALOGV("onStop()");
            ALOGV("    APP_CMD_STOP");
            break;
        }
        case APP_CMD_DESTROY: {
            ALOGV("onDestroy()");
            ALOGV("    APP_CMD_DESTROY");
            appState->NativeWindow = NULL;
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            ALOGV("surfaceCreated()");
            ALOGV("    APP_CMD_INIT_WINDOW");
            appState->NativeWindow = app->window;
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            ALOGV("surfaceDestroyed()");
            ALOGV("    APP_CMD_TERM_WINDOW");
            appState->NativeWindow = NULL;
            break;
        }
    }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app *app) {
    ALOGV("----------------------------------------------------------------");
    ALOGV("android_app_entry()");
    ALOGV("    android_main()");

    ovrJava java;
    java.Vm = app->activity->vm;
    (*java.Vm)->AttachCurrentThread(java.Vm, &java.Env, NULL);
    java.ActivityObject = app->activity->clazz;

    AAssetManager *aassetManager = app->activity->assetManager;

    // Note that AttachCurrentThread will reset the thread name.
    prctl(PR_SET_NAME, (long) "OVR::Main", 0, 0, 0);

    ovrInitParms initParms = vrapi_DefaultInitParms(&java);
    initParms.GraphicsAPI = VRAPI_GRAPHICS_API_VULKAN_1;
    int32_t initResult = vrapi_Initialize(&initParms);
    if (initResult != VRAPI_INITIALIZE_SUCCESS) {
        ALOGE("Failed to initialize VrApi");
        // If intialization failed, vrapi_* function calls will not be available.
        exit(0);
    }

    ovrApp appState;
    ovrApp_Clear(&appState);
    appState.Java = java;

    char instanceExtensionNames[4096];
    uint32_t instanceExtensionNamesSize = sizeof(instanceExtensionNames);

    // Get the required instance extensions.
    if (vrapi_GetInstanceExtensionsVulkan(instanceExtensionNames, &instanceExtensionNamesSize)) {
        ALOGE("vrapi_GetInstanceExtensionsVulkan FAILED");
        vrapi_Shutdown();
        exit(0);
    }

    // Create the Vulkan instance.
    ovrVkInstance instance;
    if (ovrVkInstance_Create(&instance, instanceExtensionNames, instanceExtensionNamesSize) ==
        false) {
        ALOGE("Failed to create vulkan instance");
        vrapi_Shutdown();
        exit(0);
    }

    // Get the required device extensions.
    char deviceExtensionNames[4096];
    uint32_t deviceExtensionNamesSize = sizeof(deviceExtensionNames);

    if (vrapi_GetDeviceExtensionsVulkan(deviceExtensionNames, &deviceExtensionNamesSize)) {
        ALOGE("vrapi_GetDeviceExtensionsVulkan FAILED");
        vrapi_Shutdown();
        exit(0);
    }

    // Select the physical device.
    if (ovrVkDevice_SelectPhysicalDevice(
            &appState.Device, &instance, deviceExtensionNames, deviceExtensionNamesSize) == false) {
        ALOGE("Failed to select physical device");
        vrapi_Shutdown();
        exit(0);
    }

    // Create the Vulkan device
    if (ovrVkDevice_Create(&appState.Device, &instance) == false) {
        ALOGE("Failed to create vulkan device");
        vrapi_Shutdown();
        exit(0);
    }

    // Set up the vulkan queue and command buffer
    if (ovrVkContext_Create(&appState.Context, &appState.Device, 0 /* queueIndex */) == false) {
        ALOGE("Failed to create a valid vulkan context");
        vrapi_Shutdown();
        exit(0);
    }

    ovrSystemCreateInfoVulkan systemInfo;
    systemInfo.Instance = instance.instance;
    systemInfo.Device = appState.Device.device;
    systemInfo.PhysicalDevice = appState.Device.physicalDevice;
    initResult = vrapi_CreateSystemVulkan(&systemInfo);
    if (initResult != ovrSuccess) {
        ALOGE("Failed to create VrApi Vulkan System");
        vrapi_Shutdown();
        exit(0);
    }

    appState.CpuLevel = CPU_LEVEL;
    appState.GpuLevel = GPU_LEVEL;
    appState.MainThreadTid = gettid();

    ovrRenderer_Create(&appState.Renderer, &appState.Context, &appState.Java);

    app->userData = &appState;
    app->onAppCmd = app_handle_cmd;

    while (app->destroyRequested == 0) {
        // Read all pending events.
        for (;;) {
            int events;
            struct android_poll_source *source;
            const int timeoutMilliseconds =
                    (appState.Ovr == NULL && app->destroyRequested == 0) ? -1 : 0;
            if (ALooper_pollAll(timeoutMilliseconds, NULL, &events, (void **) &source) < 0) {
                break;
            }

            // Process this event.
            if (source != NULL) {
                source->process(app, source);
            }

            ovrApp_HandleVrModeChanges(&appState);
        }

        // We must read from the event queue with regular frequency.
        ovrApp_HandleVrApiEvents(&appState);

        ovrApp_HandleInput(&appState);

        if (appState.Ovr == NULL) {
            continue;
        }

        // Create the scene if not yet created.
        // The scene is created here to be able to show a loading icon.
        if (!appState.Scene.CreatedScene) {
            // Show a loading icon.
            int frameFlags = 0;
            frameFlags |= VRAPI_FRAME_FLAG_FLUSH;

            ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
            blackLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

            ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
            iconLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

            const ovrLayerHeader2 *layers[] = {
                    &blackLayer.Header,
                    &iconLayer.Header,
            };

            ovrSubmitFrameDescription2 frameDesc = {0};
            frameDesc.Flags = frameFlags;
            frameDesc.SwapInterval = 1;
            frameDesc.FrameIndex = appState.FrameIndex;
            frameDesc.DisplayTime = appState.DisplayTime;
            frameDesc.LayerCount = 2;
            frameDesc.Layers = layers;

            vrapi_SubmitFrame2(appState.Ovr, &frameDesc);

            // Create the scene.
            ovrScene_Create(aassetManager, &appState.Context, &appState.Scene, &appState.Renderer);
        }

        //----------------------
        // Simulation
        //----------------------

        // This is the only place the frame index is incremented, right before
        // calling vrapi_GetPredictedDisplayTime().
        appState.FrameIndex++;

        // Get the HMD pose, predicted for the middle of the time period during which
        // the new eye images will be displayed. The number of frames predicted ahead
        // depends on the pipeline depth of the engine and the synthesis rate.
        // The better the prediction, the less black will be pulled in at the edges.
        const double predictedDisplayTime =
                vrapi_GetPredictedDisplayTime(appState.Ovr, appState.FrameIndex);
        const ovrTracking2 tracking =
                vrapi_GetPredictedTracking2(appState.Ovr, predictedDisplayTime);

        appState.DisplayTime = predictedDisplayTime;

        // Render eye images and setup ovrFrameParms using ovrTracking2.
        const ovrLayerProjection2 worldLayer = ovrRenderer_RenderFrame(
                &appState.Renderer,
                appState.FrameIndex,
                &appState.Scene,
                &tracking);

        const ovrLayerHeader2 *layers[] = {&worldLayer.Header};

        ovrSubmitFrameDescription2 frameDesc = {0};
        frameDesc.Flags = 0;
        frameDesc.SwapInterval = appState.SwapInterval;
        frameDesc.FrameIndex = appState.FrameIndex;
        frameDesc.DisplayTime = appState.DisplayTime;
        frameDesc.LayerCount = 1;
        frameDesc.Layers = layers;

        // Hand over the eye images to the time warp.
        vrapi_SubmitFrame2(appState.Ovr, &frameDesc);
    }

    ovrRenderer_Destroy(&appState.Renderer, &appState.Context);

    ovrScene_Destroy(&appState.Context, &appState.Scene);

    vrapi_DestroySystemVulkan();

    vrapi_Shutdown();

    ovrVkContext_Destroy(&appState.Context);
    ovrVkDevice_Destroy(&appState.Device);
    ovrVkInstance_Destroy(&instance);

    (*java.Vm)->DetachCurrentThread(java.Vm);
}
