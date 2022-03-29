#include <vulkan/vulkan.h>

#define VULKAN_LOADER "libvulkan.so"

#include <android/log.h>

#include <VrApi_Types.h> // for vector and matrix types

/*
================================
Common headers
================================
*/

#include <assert.h>
#include <dlfcn.h> // for dlopen
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for memset
#include <cglm/cglm.h>

/*
================================
Common defines
================================
*/

#define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))
#define OFFSETOF_MEMBER(type, member) (size_t) & ((type*)0)->member
#define SIZEOF_MEMBER(type, member) sizeof(((type*)0)->member)
#define MAX(x, y) ((x > y) ? (x) : (y))

#define VK_ALLOCATOR NULL

#define USE_API_DUMP \
    0 // place vk_layer_settings.txt in the executable folder and change APIDumpFile = TRUE

#define ICD_SPV_MAGIC 0x07230203

#if !defined(ALOGE)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, "Framework_Vulkan", __VA_ARGS__)
#endif

#if !defined(ALOGV)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "Framework_Vulkan", __VA_ARGS__)
#endif

/*
================================================================================================================================

ovrScreenRect

================================================================================================================================
*/

typedef struct {
    int x;
    int y;
    int width;
    int height;
} ovrScreenRect;

/*
================================================================================================================================

ovrVertex

================================================================================================================================
*/
typedef struct {
    vec2 pos;
    vec3 color;
    vec2 texCoord;
} ovrVertex;

typedef struct {
    VkBuffer buffer;
    VkDeviceMemory bufferMemory;
} ovrBuffer;

typedef struct {
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView imageView;
    VkSampler textureSampler;
} ovrImage;


VkVertexInputBindingDescription *getVertexInputBindingDescription();

VkVertexInputAttributeDescription *getVertexInputAttributeDescriptions();

/*
================================================================================================================================

Vulkan error checking.

================================================================================================================================
*/

#define VK(func) VkCheckErrors(func, #func);
#define VC(func) func;

static const char *VkErrorString(VkResult result) {
    switch (result) {
        case VK_SUCCESS:
            return "VK_SUCCESS";
        case VK_NOT_READY:
            return "VK_NOT_READY";
        case VK_TIMEOUT:
            return "VK_TIMEOUT";
        case VK_EVENT_SET:
            return "VK_EVENT_SET";
        case VK_EVENT_RESET:
            return "VK_EVENT_RESET";
        case VK_INCOMPLETE:
            return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:
            return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_SUBOPTIMAL_KHR:
            return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "VK_ERROR_VALIDATION_FAILED_EXT";
        default: {
            if (result == VK_ERROR_INVALID_SHADER_NV) {
                return "VK_ERROR_INVALID_SHADER_NV";
            }
            return "unknown";
        }
    }
}

static void VkCheckErrors(VkResult result, const char *function) {
    if (result != VK_SUCCESS) {
        ALOGE("Vulkan error: %s: %s\n", function, VkErrorString(result));
    }
}

/*
================================================================================================================================

Vulkan Instance.

================================================================================================================================
*/

typedef struct {
    void *loader;
    VkInstance instance;
    VkBool32 validate;

    // Global functions.
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
    PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
    PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
    PFN_vkCreateInstance vkCreateInstance;

    // Instance functions.
    PFN_vkDestroyInstance vkDestroyInstance;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
    PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR;
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
    PFN_vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceImageFormatProperties;
    PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
    PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties;
    PFN_vkCreateDevice vkCreateDevice;
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;

    // Debug callback.
    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
    VkDebugReportCallbackEXT debugReportCallback;

} ovrVkInstance;

// Expects 'requiredExtensionNames' as list of strings delimited by a single space.
bool ovrVkInstance_Create(
        ovrVkInstance *instance,
        const char *requiredExtensionNames,
        uint32_t requiredExtensionNamesSize);

void ovrVkInstance_Destroy(ovrVkInstance *instance);

/*
================================================================================================================================

Vulkan Device.

================================================================================================================================
*/

typedef struct {
    ovrVkInstance *instance;
    uint32_t enabledExtensionCount;
    const char *enabledExtensionNames[32];
    uint32_t enabledLayerCount;
    const char *enabledLayerNames[32];
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceFeatures2 physicalDeviceFeatures;
    VkPhysicalDeviceProperties2 physicalDeviceProperties;
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
    uint32_t queueFamilyCount;
    VkQueueFamilyProperties *queueFamilyProperties;
    uint32_t *queueFamilyUsedQueues;
    pthread_mutex_t queueFamilyMutex;
    int workQueueFamilyIndex;
    int presentQueueFamilyIndex;

    // The logical device.
    VkDevice device;

    // Device functions
    PFN_vkDestroyDevice vkDestroyDevice;
    PFN_vkGetDeviceQueue vkGetDeviceQueue;
    PFN_vkQueueSubmit vkQueueSubmit;
    PFN_vkQueueWaitIdle vkQueueWaitIdle;
    PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
    PFN_vkAllocateMemory vkAllocateMemory;
    PFN_vkFreeMemory vkFreeMemory;
    PFN_vkMapMemory vkMapMemory;
    PFN_vkUnmapMemory vkUnmapMemory;
    PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
    PFN_vkBindBufferMemory vkBindBufferMemory;
    PFN_vkBindImageMemory vkBindImageMemory;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
    PFN_vkCreateFence vkCreateFence;
    PFN_vkDestroyFence vkDestroyFence;
    PFN_vkResetFences vkResetFences;
    PFN_vkGetFenceStatus vkGetFenceStatus;
    PFN_vkWaitForFences vkWaitForFences;
    PFN_vkCreateBuffer vkCreateBuffer;
    PFN_vkDestroyBuffer vkDestroyBuffer;
    PFN_vkCreateImage vkCreateImage;
    PFN_vkDestroyImage vkDestroyImage;
    PFN_vkCreateImageView vkCreateImageView;
    PFN_vkDestroyImageView vkDestroyImageView;
    PFN_vkCreateShaderModule vkCreateShaderModule;
    PFN_vkDestroyShaderModule vkDestroyShaderModule;
    PFN_vkCreatePipelineCache vkCreatePipelineCache;
    PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
    PFN_vkDestroyPipeline vkDestroyPipeline;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
    PFN_vkCreateSampler vkCreateSampler;
    PFN_vkDestroySampler vkDestroySampler;
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
    PFN_vkResetDescriptorPool vkResetDescriptorPool;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
    PFN_vkCreateFramebuffer vkCreateFramebuffer;
    PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
    PFN_vkCreateRenderPass vkCreateRenderPass;
    PFN_vkDestroyRenderPass vkDestroyRenderPass;
    PFN_vkCreateCommandPool vkCreateCommandPool;
    PFN_vkDestroyCommandPool vkDestroyCommandPool;
    PFN_vkResetCommandPool vkResetCommandPool;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
    PFN_vkEndCommandBuffer vkEndCommandBuffer;
    PFN_vkResetCommandBuffer vkResetCommandBuffer;
    PFN_vkCmdBindPipeline vkCmdBindPipeline;
    PFN_vkCmdSetViewport vkCmdSetViewport;
    PFN_vkCmdSetScissor vkCmdSetScissor;
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
    PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
    PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
    PFN_vkCmdDraw vkCmdDraw;
    PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
    PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
    PFN_vkCmdResolveImage vkCmdResolveImage;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
    PFN_vkCmdPushConstants vkCmdPushConstants;
    PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
    PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
    PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;

} ovrVkDevice;

// Expects 'requiredExtensionNames' as list of strings delimited by a single space.
bool ovrVkDevice_SelectPhysicalDevice(
        ovrVkDevice *device,
        ovrVkInstance *instance,
        const char *requiredExtensionNames,
        uint32_t requiredExtensionNamesSize);

bool ovrVkDevice_Create(ovrVkDevice *device, ovrVkInstance *instance);

void ovrVkDevice_Destroy(ovrVkDevice *device);

/*
================================================================================================================================

Vulkan context.

A context encapsulates a queue that is used to submit command buffers.
A context can only be used by a single thread.
For optimal performance a context should only be created at load time, not at runtime.

================================================================================================================================
*/

typedef struct {
    ovrVkDevice *device;
    uint32_t queueFamilyIndex;
    uint32_t queueIndex;
    VkQueue queue;
    VkCommandPool commandPool;
    VkCommandBuffer setupCommandBuffer;
} ovrVkContext;

bool ovrVkContext_Create(ovrVkContext *context, ovrVkDevice *device, const int queueIndex);

void ovrVkContext_Destroy(ovrVkContext *context);

void ovrVkContext_WaitIdle(ovrVkContext *context);

/*
================================================================================================================================

Vulkan render pass.

A render pass encapsulates a sequence of graphics commands that can be executed in a single tiling
pass. For optimal performance a render pass should only be created at load time, not at runtime.
Render passes cannot overlap and cannot be nested.

================================================================================================================================
*/

typedef struct {
    VkFormat colorFormat;
    VkSampleCountFlagBits sampleCount;
    VkFormat internalColorFormat;
    VkRenderPass renderPass;
    ovrVector4f clearColor;
} ovrVkRenderPass;

bool ovrVkRenderPass_Create(
        ovrVkContext *context,
        ovrVkRenderPass *renderPass,
        const VkFormat colorFormat,
        const VkSampleCountFlagBits sampleCount,
        const ovrVector4f *clearColor);

void ovrVkRenderPass_Destroy(ovrVkContext *context, ovrVkRenderPass *renderPass);

/*
================================================================================================================================

Vulkan framebuffer.

A framebuffer encapsulates a buffered set of textures.

For optimal performance a framebuffer should only be created at load time, not at runtime.

================================================================================================================================
*/

typedef struct {
    VkImageView *colorImageViews;
    VkFramebuffer *framebuffers;
    ovrVkRenderPass *renderPass;
    int width;
    int height;
    int numLayers;
    int numBuffers;
    int currentBuffer;
    int currentLayer;
} ovrVkFramebuffer;

ovrScreenRect ovrVkFramebuffer_GetRect(const ovrVkFramebuffer *framebuffer);

int ovrVkFramebuffer_GetBufferCount(const ovrVkFramebuffer *framebuffer);


/*
================================================================================================================================

Vulkan program parms and layout.

================================================================================================================================
*/


void VkPipelineLayout_Create(
        ovrVkContext *context,
        VkDescriptorSetLayout *descriptorSetLayout,
        VkPipelineLayout *layout);

void VkPipelineLayout_Destroy(ovrVkContext *context, VkPipelineLayout *layout);

/*
================================================================================================================================

Vulkan graphics program.

A graphics program encapsulates a vertex and fragment program that are used to render geometry.
For optimal performance a graphics program should only be created at load time, not at runtime.


================================================================================================================================
*/

typedef struct {
    VkShaderModule vertexShaderModule;
    VkShaderModule fragmentShaderModule;
    VkPipelineShaderStageCreateInfo pipelineStages[2];
    VkPipelineLayout pipelineLayout;
    int vertexAttribsFlags;
} ovrVkGraphicsProgram;

bool ovrVkGraphicsProgram_Create(
        ovrVkContext *context,
        ovrVkGraphicsProgram *program,
        VkDescriptorSetLayout *descriptorSetLayout,
        const void *vertexSourceData,
        const size_t vertexSourceSize,
        const void *fragmentSourceData,
        const size_t fragmentSourceSize);

void ovrVkGraphicsProgram_Destroy(ovrVkContext *context, ovrVkGraphicsProgram *program);

/*
================================================================================================================================

Vulkan graphics pipeline.

A graphics pipeline encapsulates the geometry, program and ROP state that is used to render.
For optimal performance a graphics pipeline should only be created at load time, not at runtime.
The vertex attribute locations are assigned here, when both the geometry and program are known,
to avoid binding vertex attributes that are not used by the vertex shader, and to avoid binding
to a discontinuous set of vertex attribute locations.

================================================================================================================================
*/

typedef enum {
    OVR_FRONT_FACE_COUNTER_CLOCKWISE = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    OVR_FRONT_FACE_CLOCKWISE = VK_FRONT_FACE_CLOCKWISE
} ovrVkFrontFace;

typedef enum {
    OVR_CULL_MODE_NONE = 0,
    OVR_CULL_MODE_FRONT = VK_CULL_MODE_FRONT_BIT,
    OVR_CULL_MODE_BACK = VK_CULL_MODE_BACK_BIT
} ovrVkCullMode;

typedef enum {
    OVR_COMPARE_OP_NEVER = VK_COMPARE_OP_NEVER,
    OVR_COMPARE_OP_LESS = VK_COMPARE_OP_LESS,
    OVR_COMPARE_OP_EQUAL = VK_COMPARE_OP_EQUAL,
    OVR_COMPARE_OP_LESS_OR_EQUAL = VK_COMPARE_OP_LESS_OR_EQUAL,
    OVR_COMPARE_OP_GREATER = VK_COMPARE_OP_GREATER,
    OVR_COMPARE_OP_NOT_EQUAL = VK_COMPARE_OP_NOT_EQUAL,
    OVR_COMPARE_OP_GREATER_OR_EQUAL = VK_COMPARE_OP_GREATER_OR_EQUAL,
    OVR_COMPARE_OP_ALWAYS = VK_COMPARE_OP_ALWAYS
} ovrVkCompareOp;

typedef enum {
    OVR_BLEND_OP_ADD = VK_BLEND_OP_ADD,
    OVR_BLEND_OP_SUBTRACT = VK_BLEND_OP_SUBTRACT,
    OVR_BLEND_OP_REVERSE_SUBTRACT = VK_BLEND_OP_REVERSE_SUBTRACT,
    OVR_BLEND_OP_MIN = VK_BLEND_OP_MIN,
    OVR_BLEND_OP_MAX = VK_BLEND_OP_MAX
} ovrVkBlendOp;

typedef enum {
    OVR_BLEND_FACTOR_ZERO = VK_BLEND_FACTOR_ZERO,
    OVR_BLEND_FACTOR_ONE = VK_BLEND_FACTOR_ONE,
    OVR_BLEND_FACTOR_SRC_COLOR = VK_BLEND_FACTOR_SRC_COLOR,
    OVR_BLEND_FACTOR_ONE_MINUS_SRC_COLOR = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
    OVR_BLEND_FACTOR_DST_COLOR = VK_BLEND_FACTOR_DST_COLOR,
    OVR_BLEND_FACTOR_ONE_MINUS_DST_COLOR = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
    OVR_BLEND_FACTOR_SRC_ALPHA = VK_BLEND_FACTOR_SRC_ALPHA,
    OVR_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    OVR_BLEND_FACTOR_DST_ALPHA = VK_BLEND_FACTOR_DST_ALPHA,
    OVR_BLEND_FACTOR_ONE_MINUS_DST_ALPHA = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    OVR_BLEND_FACTOR_CONSTANT_COLOR = VK_BLEND_FACTOR_CONSTANT_COLOR,
    OVR_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
    OVR_BLEND_FACTOR_CONSTANT_ALPHA = VK_BLEND_FACTOR_CONSTANT_ALPHA,
    OVR_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
    OVR_BLEND_FACTOR_SRC_ALPHA_SATURATE = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE
} ovrVkBlendFactor;

typedef struct {
    bool blendEnable;
    bool redWriteEnable;
    bool blueWriteEnable;
    bool greenWriteEnable;
    bool alphaWriteEnable;
    ovrVkFrontFace frontFace;
    ovrVkCullMode cullMode;
    ovrVkCompareOp depthCompare;
    ovrVector4f blendColor;
    ovrVkBlendOp blendOpColor;
    ovrVkBlendFactor blendSrcColor;
    ovrVkBlendFactor blendDstColor;
    ovrVkBlendOp blendOpAlpha;
    ovrVkBlendFactor blendSrcAlpha;
    ovrVkBlendFactor blendDstAlpha;
} ovrVkRasterOperations;

typedef struct {
    ovrVkRasterOperations rop;
    const ovrVkRenderPass *renderPass;
    const ovrVkGraphicsProgram *program;
} ovrVkGraphicsPipelineParms;

typedef struct {
    ovrVkRasterOperations rop;
    const ovrVkGraphicsProgram *program;
    VkPipelineVertexInputStateCreateInfo vertexInputState;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
    VkPipeline pipeline;
} ovrVkGraphicsPipeline;

void ovrVkGraphicsPipelineParms_Init(ovrVkGraphicsPipelineParms *parms);

bool ovrVkGraphicsPipeline_Create(
        ovrVkContext *context,
        ovrVkGraphicsPipeline *pipeline,
        const ovrVkGraphicsPipelineParms *parms,
        ovrScreenRect screenRect);

void ovrVkGraphicsPipeline_Destroy(ovrVkContext *context, ovrVkGraphicsPipeline *pipeline);

/*
================================================================================================================================

Vulkan fence.

A fence is used to notify completion of a command buffer.
For optimal performance a fence should only be created at load time, not at runtime.

================================================================================================================================
*/


void VkFence_Create(ovrVkContext *context, VkFence *fence);

void VkFence_Destroy(ovrVkContext *context, VkFence *fence);

/*
================================================================================================================================

Vulkan graphics commands.

A graphics command encapsulates all GPU state associated with a single draw call.
The pointers passed in as parameters are expected to point to unique objects that persist
at least past the submission of the command buffer into which the graphics command is
submitted. Because pointers are maintained as state, DO NOT use pointers to local
variables that will go out of scope before the command buffer is submitted.

================================================================================================================================
*/

typedef struct {
    const ovrVkGraphicsPipeline *pipeline;
} ovrVkGraphicsCommand;

void ovrVkGraphicsCommand_Init(ovrVkGraphicsCommand *command);

void ovrVkGraphicsCommand_SetPipeline(
        ovrVkGraphicsCommand *command,
        const ovrVkGraphicsPipeline *pipeline);

/*
================================================================================================================================

Vulkan command buffer.

A command buffer is used to record graphics and compute commands.
For optimal performance a command buffer should only be created at load time, not at runtime.
When a command is submitted, the state of the command is compared with the currently saved state,
and only the state that has changed translates into graphics API function calls.

================================================================================================================================
*/

typedef enum {
    OVR_BUFFER_UNMAP_TYPE_USE_ALLOCATED, // use the newly allocated (host visible) buffer
    OVR_BUFFER_UNMAP_TYPE_COPY_BACK // copy back to the original buffer
} ovrVkBufferUnmapType;

typedef enum {
    OVR_COMMAND_BUFFER_TYPE_PRIMARY,
    OVR_COMMAND_BUFFER_TYPE_SECONDARY,
    OVR_COMMAND_BUFFER_TYPE_SECONDARY_CONTINUE_RENDER_PASS
} ovrVkCommandBufferType;

typedef struct {
    ovrVkCommandBufferType type;
    int numBuffers;
    int currentBuffer;
    VkCommandBuffer *cmdBuffers;
    ovrVkContext *context;
    VkFence *fences;
    ovrVkGraphicsCommand currentGraphicsState;
    ovrVkFramebuffer *currentFramebuffer;
    ovrVkRenderPass *currentRenderPass;
} ovrVkCommandBuffer;

void ovrVkCommandBuffer_Create(
        ovrVkContext *context,
        ovrVkCommandBuffer *commandBuffer,
        const ovrVkCommandBufferType type,
        const int numBuffers);

void ovrVkCommandBuffer_Destroy(ovrVkContext *context, ovrVkCommandBuffer *commandBuffer);

void ovrVkCommandBuffer_BeginPrimary(ovrVkCommandBuffer *commandBuffer);

void ovrVkCommandBuffer_EndPrimary(ovrVkCommandBuffer *commandBuffer);

VkFence *ovrVkCommandBuffer_SubmitPrimary(ovrVkCommandBuffer *commandBuffer);

void ovrVkCommandBuffer_BeginFramebuffer(
        ovrVkCommandBuffer *commandBuffer,
        ovrVkFramebuffer *framebuffer,
        const int arrayLayer);

void ovrVkCommandBuffer_EndFramebuffer(
        ovrVkCommandBuffer *commandBuffer,
        ovrVkFramebuffer *framebuffer,
        const int arrayLayer);

void ovrVkCommandBuffer_BeginRenderPass(
        ovrVkCommandBuffer *commandBuffer,
        ovrVkRenderPass *renderPass,
        ovrVkFramebuffer *framebuffer,
        const ovrScreenRect *rect);

void ovrVkCommandBuffer_EndRenderPass(
        ovrVkCommandBuffer *commandBuffer,
        ovrVkRenderPass *renderPass);

void ovrVkCommandBuffer_SubmitGraphicsCommand(
        ovrVkCommandBuffer *commandBuffer,
        const ovrBuffer *vertexBuffer,
        const ovrBuffer *indexBuffer,
        const ovrVkGraphicsCommand *command,
        VkPipelineLayout *pipelineLayout,
        const VkDescriptorSet *descriptorSet,
        uint32_t verticesLength,
        uint32_t indicesLength);

/*
================================================================================================================================

Vulkan buffer.

================================================================================================================================
*/

void
ovrBuffer_Create(ovrVkContext *context, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                 VkDeviceSize size, ovrBuffer *buffer);

void
ovrBuffer_Vertex_Create(ovrVkContext *context, const ovrVertex *vertices, uint32_t verticesLength,
                        ovrBuffer *buffer);

void ovrBuffer_Index_Create(ovrVkContext *context, const uint16_t *indices, uint32_t indicesLength,
                            ovrBuffer *buffer);

void ovrBuffer_Uniform_Create(ovrVkContext *context, ovrBuffer *buffer);

void copyBuffer(ovrVkContext *context, ovrBuffer *srcBuffer, ovrBuffer *dstBuffer, VkDeviceSize size);

VkCommandBuffer *beginSingleTimeCommands(ovrVkContext *context);

void endSingleTimeCommands(ovrVkContext *context, VkCommandBuffer *commandBuffer);

int32_t findMemoryType(ovrVkDevice *device, uint32_t typeFilter,
                       VkMemoryPropertyFlags properties);

/*
================================================================================================================================

OvrUniformBufferObject.

================================================================================================================================
*/

typedef struct {
    mat4 model;
    mat4 view;
    mat4 proj;
} ovrUniformBufferObject;

/*
================================================================================================================================

DescriptorSet layout.

================================================================================================================================
*/

void
VkDescriptorSetLayout_Create(ovrVkContext *context, VkDescriptorSetLayout *descriptorSetLayout);


void VkDescriptorPool_Create(ovrVkContext *context, VkDescriptorPool *descriptorPool);

void VkDescriptorSet_Create(ovrVkContext *context, VkDescriptorSetLayout *descriptorSetLayout,
                            VkDescriptorPool descriptorPool, ovrBuffer *uniformBuffers,
                            ovrImage *textureImage,
                            VkDescriptorSet *descriptorSet);

/*
================================================================================================================================

Images, samplers.

================================================================================================================================
*/

void
createTextureImage(ovrVkContext *context, unsigned char *imageHandle, uint32_t texWidth,
                   uint32_t texHeight,
                   ovrImage *textureImage);

void createImage(ovrVkContext *context, uint32_t width, uint32_t height, VkFormat format,
                 VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                 ovrImage *image);

void transitionImageLayout(ovrVkContext *context, VkImage *image,
                           VkImageLayout oldLayout, VkImageLayout newLayout);

void copyBufferToImage(ovrVkContext *context, VkBuffer *buffer, VkImage *image, uint32_t width,
                       uint32_t height);

void createTextureImageView(ovrVkContext *context, ovrImage *textureImage);

void createTextureSampler(ovrVkContext *context, ovrImage *image);
