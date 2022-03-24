/************************************************************************************

Filename	:	Framework_Vulkan.c
Content		:	Vulkan Framework
Created		:	October, 2017
Authors		:	J.M.P. van Waveren

Copyright	:	Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "Framework_Vulkan.h"
#include <sys/system_properties.h>

static void ParseExtensionString(
        char *extensionNames,
        uint32_t *numExtensions,
        const char *extensionArrayPtr[],
        const uint32_t arrayCount) {
    uint32_t extensionCount = 0;
    char *nextExtensionName = extensionNames;

    while (*nextExtensionName && (extensionCount < arrayCount)) {
        extensionArrayPtr[extensionCount++] = nextExtensionName;

        // Skip to a space or null
        while (*(++nextExtensionName)) {
            if (*nextExtensionName == ' ') {
                // Null-terminate and break out of the loop
                *nextExtensionName++ = '\0';
                break;
            }
        }
    }

    *numExtensions = extensionCount;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
        VkDebugReportFlagsEXT msgFlags,
        VkDebugReportObjectTypeEXT objType,
        uint64_t srcObject,
        size_t location,
        int32_t msgCode,
        const char *pLayerPrefix,
        const char *pMsg,
        void *pUserData) {
    const char *reportType = "Unknown";
    if ((msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0) {
        reportType = "Error";
    } else if ((msgFlags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0) {
        reportType = "Warning";
    } else if ((msgFlags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) != 0) {
        reportType = "Performance Warning";
    } else if ((msgFlags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) != 0) {
        reportType = "Information";
    } else if ((msgFlags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) != 0) {
        reportType = "Debug";
    }

    ALOGE("%s: [%s] Code %d : %s", reportType, pLayerPrefix, msgCode, pMsg);
    return VK_FALSE;
}

/*
================================================================================================================================

Vulkan Instance

================================================================================================================================
*/

#define GET_INSTANCE_PROC_ADDR_EXP(function)                                              \
    instance->function =                                                                  \
        (PFN_##function)(instance->vkGetInstanceProcAddr(instance->instance, #function)); \
    assert(instance->function != NULL);
#define GET_INSTANCE_PROC_ADDR(function) GET_INSTANCE_PROC_ADDR_EXP(function)

bool ovrVkInstance_Create(
        ovrVkInstance *instance,
        const char *requiredExtensionNames,
        uint32_t requiredExtensionNamesSize) {
    memset(instance, 0, sizeof(ovrVkInstance));

#if defined(_DEBUG)
    instance->validate = VK_TRUE;
#else
    instance->validate = VK_FALSE;
#endif

    instance->loader = dlopen(VULKAN_LOADER, RTLD_NOW | RTLD_LOCAL);
    if (instance->loader == NULL) {
        ALOGE("%s not available: %s", VULKAN_LOADER, dlerror());
        return false;
    }
    instance->vkGetInstanceProcAddr =
            (PFN_vkGetInstanceProcAddr) dlsym(instance->loader, "vkGetInstanceProcAddr");
    instance->vkEnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties) dlsym(
            instance->loader, "vkEnumerateInstanceLayerProperties");
    instance->vkEnumerateInstanceExtensionProperties =
            (PFN_vkEnumerateInstanceExtensionProperties) dlsym(
                    instance->loader, "vkEnumerateInstanceExtensionProperties");
    instance->vkCreateInstance = (PFN_vkCreateInstance) dlsym(instance->loader, "vkCreateInstance");

    char *extensionNames = (char *) requiredExtensionNames;

    // Add any additional instance extensions.
    // TODO: Differentiate between required and validation/debug.
    if (instance->validate) {
        strcat(extensionNames, " " VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }
    strcat(extensionNames, " " VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    const char *enabledExtensionNames[32] = {0};
    uint32_t enabledExtensionCount = 0;

    ParseExtensionString(extensionNames, &enabledExtensionCount, enabledExtensionNames, 32);
#if defined(_DEBUG)
    ALOGV("Enabled Extensions: ");
    for (uint32_t i = 0; i < enabledExtensionCount; i++) {
        ALOGV("\t(%d):%s", i, enabledExtensionNames[i]);
    }
#endif

    // TODO: Differentiate between required and validation/debug.
    static const char *requestedLayers[] = {"VK_LAYER_KHRONOS_validation"};
    const uint32_t requestedCount = sizeof(requestedLayers) / sizeof(requestedLayers[0]);

    // Request debug layers
    const char *enabledLayerNames[32] = {0};
    uint32_t enabledLayerCount = 0;
    if (instance->validate) {
        uint32_t availableLayerCount = 0;
        VK(instance->vkEnumerateInstanceLayerProperties(&availableLayerCount, NULL));

        VkLayerProperties *availableLayers =
                malloc(availableLayerCount * sizeof(VkLayerProperties));
        VK(instance->vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers));

        for (uint32_t i = 0; i < requestedCount; i++) {
            for (uint32_t j = 0; j < availableLayerCount; j++) {
                if (strcmp(requestedLayers[i], availableLayers[j].layerName) == 0) {
                    enabledLayerNames[enabledLayerCount++] = requestedLayers[i];
                    break;
                }
            }
        }

        free(availableLayers);
    }

#if defined(_DEBUG)
        ALOGV("Enabled Layers ");
        for (uint32_t i = 0; i < enabledLayerCount; i++) {
            ALOGV("\t(%d):%s", i, enabledLayerNames[i]);
        }
#endif

    ALOGV("--------------------------------\n");

    const uint32_t apiMajor = VK_VERSION_MAJOR(VK_API_VERSION_1_0);
    const uint32_t apiMinor = VK_VERSION_MINOR(VK_API_VERSION_1_0);
    const uint32_t apiPatch = VK_VERSION_PATCH(VK_API_VERSION_1_0);
    ALOGV("Instance API version : %d.%d.%d\n", apiMajor, apiMinor, apiPatch);

    ALOGV("--------------------------------\n");

    // Create the instance.

    VkDebugReportCallbackCreateInfoEXT debugReportCallbackCreateInfo;
    debugReportCallbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    debugReportCallbackCreateInfo.pNext = NULL;
    debugReportCallbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
                                          VK_DEBUG_REPORT_WARNING_BIT_EXT |
                                          VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    debugReportCallbackCreateInfo.pfnCallback = DebugReportCallback;
    debugReportCallbackCreateInfo.pUserData = NULL;

    VkApplicationInfo app;
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pNext = NULL;
    app.pApplicationName = "WalleVrController2";
    app.applicationVersion = 1;
    app.pEngineName = "Oculus Mobile SDK";
    app.engineVersion = 1;
    app.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceCreateInfo;
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    // if validation is enabled, pass the VkDebugReportCallbackCreateInfoEXT through
    // to vkCreateInstance so validation is enabled for the vkCreateInstance call.
    instanceCreateInfo.pNext = (instance->validate) ? &debugReportCallbackCreateInfo : NULL;
    instanceCreateInfo.flags = 0;
    instanceCreateInfo.pApplicationInfo = &app;
    instanceCreateInfo.enabledLayerCount = enabledLayerCount;
    instanceCreateInfo.ppEnabledLayerNames =
            (const char *const *) (enabledLayerCount != 0 ? enabledLayerNames : NULL);
    instanceCreateInfo.enabledExtensionCount = enabledExtensionCount;
    instanceCreateInfo.ppEnabledExtensionNames =
            (const char *const *) (enabledExtensionCount != 0 ? enabledExtensionNames : NULL);

    VK(instance->vkCreateInstance(&instanceCreateInfo, VK_ALLOCATOR, &instance->instance));

    // Get the instance functions
    GET_INSTANCE_PROC_ADDR(vkDestroyInstance);
    GET_INSTANCE_PROC_ADDR(vkEnumeratePhysicalDevices);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceFeatures);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceFeatures2KHR);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceProperties);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceProperties2KHR);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceMemoryProperties);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceQueueFamilyProperties);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceFormatProperties);
    GET_INSTANCE_PROC_ADDR(vkGetPhysicalDeviceImageFormatProperties);
    GET_INSTANCE_PROC_ADDR(vkEnumerateDeviceExtensionProperties);
    GET_INSTANCE_PROC_ADDR(vkEnumerateDeviceLayerProperties);
    GET_INSTANCE_PROC_ADDR(vkCreateDevice);
    GET_INSTANCE_PROC_ADDR(vkGetDeviceProcAddr);

    if (instance->validate) {
        GET_INSTANCE_PROC_ADDR(vkCreateDebugReportCallbackEXT);
        GET_INSTANCE_PROC_ADDR(vkDestroyDebugReportCallbackEXT);
        if (instance->vkCreateDebugReportCallbackEXT != NULL) {
            VK(instance->vkCreateDebugReportCallbackEXT(
                    instance->instance,
                    &debugReportCallbackCreateInfo,
                    VK_ALLOCATOR,
                    &instance->debugReportCallback));
        }
    }

    return true;
}

void ovrVkInstance_Destroy(ovrVkInstance *instance) {
    if (instance->validate && instance->vkDestroyDebugReportCallbackEXT != NULL) {
        VC(instance->vkDestroyDebugReportCallbackEXT(
                instance->instance, instance->debugReportCallback, VK_ALLOCATOR));
    }

    VC(instance->vkDestroyInstance(instance->instance, VK_ALLOCATOR));

    if (instance->loader != NULL) {
        dlclose(instance->loader);
        instance->loader = NULL;
    }
}

static const VkQueueFlags requiredQueueFlags = VK_QUEUE_GRAPHICS_BIT;
static const int queueCount = 1;

#define GET_DEVICE_PROC_ADDR_EXP(function)                                                  \
    device->function =                                                                      \
        (PFN_##function)(device->instance->vkGetDeviceProcAddr(device->device, #function)); \
    assert(device->function != NULL);
#define GET_DEVICE_PROC_ADDR(function) GET_DEVICE_PROC_ADDR_EXP(function)

bool ovrVkDevice_SelectPhysicalDevice(
        ovrVkDevice *device,
        ovrVkInstance *instance,
        const char *requiredExtensionNames,
        uint32_t requiredExtensionNamesSize) {
    memset(device, 0, sizeof(ovrVkDevice));

    char value[128];

    device->instance = instance;

    char *extensionNames = (char *) requiredExtensionNames;

    ParseExtensionString(
            extensionNames, &device->enabledExtensionCount, device->enabledExtensionNames, 32);
#if defined(_DEBUG)
    ALOGV("Requested Extensions: ");
    for (uint32_t i = 0; i < device->enabledExtensionCount; i++) {
        ALOGV("\t(%d):%s", i, device->enabledExtensionNames[i]);
    }
#endif

    // TODO: Differentiate between required and validation/debug.
    static const char *requestedLayers[] = {"VK_LAYER_KHRONOS_validation"};
    const int requestedCount = sizeof(requestedLayers) / sizeof(requestedLayers[0]);

    device->enabledLayerCount = 0;

    uint32_t physicalDeviceCount = 0;
    VK(instance->vkEnumeratePhysicalDevices(instance->instance, &physicalDeviceCount, NULL));

    VkPhysicalDevice *physicalDevices =
            (VkPhysicalDevice *) malloc(physicalDeviceCount * sizeof(VkPhysicalDevice));
    VK(instance->vkEnumeratePhysicalDevices(
            instance->instance, &physicalDeviceCount, physicalDevices));

    for (uint32_t physicalDeviceIndex = 0; physicalDeviceIndex < physicalDeviceCount;
         physicalDeviceIndex++) {
        // Get the device properties.
        VkPhysicalDeviceProperties physicalDeviceProperties;
        VC(instance->vkGetPhysicalDeviceProperties(
                physicalDevices[physicalDeviceIndex], &physicalDeviceProperties));

        const uint32_t driverMajor = VK_VERSION_MAJOR(physicalDeviceProperties.driverVersion);
        const uint32_t driverMinor = VK_VERSION_MINOR(physicalDeviceProperties.driverVersion);
        const uint32_t driverPatch = VK_VERSION_PATCH(physicalDeviceProperties.driverVersion);

        const uint32_t apiMajor = VK_VERSION_MAJOR(physicalDeviceProperties.apiVersion);
        const uint32_t apiMinor = VK_VERSION_MINOR(physicalDeviceProperties.apiVersion);
        const uint32_t apiPatch = VK_VERSION_PATCH(physicalDeviceProperties.apiVersion);

        ALOGV("--------------------------------\n");
        ALOGV("Device Name          : %s\n", physicalDeviceProperties.deviceName);
        ALOGV(
                "Device Type          : %s\n",
                ((physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                 ? "integrated GPU"
                 : ((physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                    ? "discrete GPU"
                    : ((physicalDeviceProperties.deviceType ==
                        VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
                       ? "virtual GPU"
                       : ((physicalDeviceProperties.deviceType ==
                           VK_PHYSICAL_DEVICE_TYPE_CPU)
                          ? "CPU"
                          : "unknown")))));
        ALOGV(
                "Vendor ID            : 0x%04X\n",
                physicalDeviceProperties.vendorID); // http://pcidatabase.com
        ALOGV("Device ID            : 0x%04X\n", physicalDeviceProperties.deviceID);
        ALOGV("Driver Version       : %d.%d.%d\n", driverMajor, driverMinor, driverPatch);
        ALOGV("API Version          : %d.%d.%d\n", apiMajor, apiMinor, apiPatch);

        // Get the queue families.
        uint32_t queueFamilyCount = 0;
        VC(instance->vkGetPhysicalDeviceQueueFamilyProperties(
                physicalDevices[physicalDeviceIndex], &queueFamilyCount, NULL));

        VkQueueFamilyProperties *queueFamilyProperties =
                (VkQueueFamilyProperties *) malloc(
                        queueFamilyCount * sizeof(VkQueueFamilyProperties));
        VC(instance->vkGetPhysicalDeviceQueueFamilyProperties(
                physicalDevices[physicalDeviceIndex], &queueFamilyCount, queueFamilyProperties));

        for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount;
             queueFamilyIndex++) {
            const VkQueueFlags queueFlags = queueFamilyProperties[queueFamilyIndex].queueFlags;
            ALOGV(
                    "%-21s%c %d =%s%s (%d queues, %d priorities)\n",
                    (queueFamilyIndex == 0 ? "Queue Families" : ""),
                    (queueFamilyIndex == 0 ? ':' : ' '),
                    queueFamilyIndex,
                    (queueFlags & VK_QUEUE_GRAPHICS_BIT) ? " graphics" : "",
                    (queueFlags & VK_QUEUE_TRANSFER_BIT) ? " transfer" : "",
                    queueFamilyProperties[queueFamilyIndex].queueCount,
                    physicalDeviceProperties.limits.discreteQueuePriorities);
        }

        // Check if this physical device supports the required queue families.
        int workQueueFamilyIndex = -1;
        int presentQueueFamilyIndex = -1;
        for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount;
             queueFamilyIndex++) {
            if ((queueFamilyProperties[queueFamilyIndex].queueFlags & requiredQueueFlags) ==
                requiredQueueFlags) {
                if ((int) queueFamilyProperties[queueFamilyIndex].queueCount >= queueCount) {
                    workQueueFamilyIndex = queueFamilyIndex;
                }
            }
            if (workQueueFamilyIndex != -1 && (presentQueueFamilyIndex != -1)) {
                break;
            }
        }

        // On Android all devices must be able to present to the system compositor, and all queue
        // families must support the necessary image layout transitions and synchronization
        // operations.
        presentQueueFamilyIndex = workQueueFamilyIndex;

        if (workQueueFamilyIndex == -1) {
            ALOGV("Required work queue family not supported.\n");
            free(queueFamilyProperties);
            continue;
        }

        ALOGV("Work Queue Family    : %d\n", workQueueFamilyIndex);
        ALOGV("Present Queue Family : %d\n", presentQueueFamilyIndex);

        // Check the device extensions.
        bool requiredExtensionsAvailable = true;

        {
            uint32_t availableExtensionCount = 0;
            VK(instance->vkEnumerateDeviceExtensionProperties(
                    physicalDevices[physicalDeviceIndex], NULL, &availableExtensionCount, NULL));

            VkExtensionProperties *availableExtensions = (VkExtensionProperties *) malloc(
                    availableExtensionCount * sizeof(VkExtensionProperties));
            VK(instance->vkEnumerateDeviceExtensionProperties(
                    physicalDevices[physicalDeviceIndex],
                    NULL,
                    &availableExtensionCount,
                    availableExtensions));

            free(availableExtensions);
        }

        if (!requiredExtensionsAvailable) {
            ALOGV("Required device extensions not supported.\n");
            free(queueFamilyProperties);
            continue;
        }

        // Enable requested device layers, if available.
        if (instance->validate) {
            uint32_t availableLayerCount = 0;
            VK(instance->vkEnumerateDeviceLayerProperties(
                    physicalDevices[physicalDeviceIndex], &availableLayerCount, NULL));

            VkLayerProperties *availableLayers =
                    malloc(availableLayerCount * sizeof(VkLayerProperties));
            VK(instance->vkEnumerateDeviceLayerProperties(
                    physicalDevices[physicalDeviceIndex], &availableLayerCount, availableLayers));

            for (uint32_t i = 0; i < requestedCount; i++) {
                for (uint32_t j = 0; j < availableLayerCount; j++) {
                    if (strcmp(requestedLayers[i], availableLayers[j].layerName) == 0) {
                        device->enabledLayerNames[device->enabledLayerCount++] = requestedLayers[i];
                        break;
                    }
                }
            }

            free(availableLayers);

#if defined(_DEBUG)
            ALOGV("Enabled Layers ");
            for (uint32_t i = 0; i < device->enabledLayerCount; i++) {
                ALOGV("\t(%d):%s", i, device->enabledLayerNames[i]);
            }
#endif
        }

        device->physicalDevice = physicalDevices[physicalDeviceIndex];
        device->queueFamilyCount = queueFamilyCount;
        device->queueFamilyProperties = queueFamilyProperties;
        device->workQueueFamilyIndex = workQueueFamilyIndex;
        device->presentQueueFamilyIndex = presentQueueFamilyIndex;
        device->physicalDeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        device->physicalDeviceFeatures.pNext = NULL;
        device->physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        device->physicalDeviceProperties.pNext = NULL;

        VC(instance->vkGetPhysicalDeviceFeatures2KHR(
                physicalDevices[physicalDeviceIndex], &device->physicalDeviceFeatures));
        VC(instance->vkGetPhysicalDeviceProperties2KHR(
                physicalDevices[physicalDeviceIndex], &device->physicalDeviceProperties));
        VC(instance->vkGetPhysicalDeviceMemoryProperties(
                physicalDevices[physicalDeviceIndex], &device->physicalDeviceMemoryProperties));

        break;
    }

    ALOGV("--------------------------------\n");

    if (device->physicalDevice == VK_NULL_HANDLE) {
        ALOGE("No capable Vulkan physical device found.");
        return false;
    }

    // Allocate a bit mask for the available queues per family.
    device->queueFamilyUsedQueues = (uint32_t *) malloc(
            device->queueFamilyCount * sizeof(uint32_t));
    for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < device->queueFamilyCount;
         queueFamilyIndex++) {
        device->queueFamilyUsedQueues[queueFamilyIndex] = 0xFFFFFFFF
                << device->queueFamilyProperties[queueFamilyIndex].queueCount;
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&device->queueFamilyMutex, &attr);

    return true;
}

bool ovrVkDevice_Create(ovrVkDevice *device, ovrVkInstance *instance) {
    //
    // Create the logical device
    //
    const uint32_t discreteQueuePriorities =
            device->physicalDeviceProperties.properties.limits.discreteQueuePriorities;
    // One queue, at mid priority
    const float queuePriority = (discreteQueuePriorities <= 2) ? 0.0f : 0.5f;

    // Create the device.
    VkDeviceQueueCreateInfo deviceQueueCreateInfo[2];
    deviceQueueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo[0].pNext = NULL;
    deviceQueueCreateInfo[0].flags = 0;
    deviceQueueCreateInfo[0].queueFamilyIndex = device->workQueueFamilyIndex;
    deviceQueueCreateInfo[0].queueCount = queueCount;
    deviceQueueCreateInfo[0].pQueuePriorities = &queuePriority;

    deviceQueueCreateInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo[1].pNext = NULL;
    deviceQueueCreateInfo[1].flags = 0;
    deviceQueueCreateInfo[1].queueFamilyIndex = device->presentQueueFamilyIndex;
    deviceQueueCreateInfo[1].queueCount = 1;
    deviceQueueCreateInfo[1].pQueuePriorities = NULL;

    VkDeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = NULL;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = 1 +
                                            (device->presentQueueFamilyIndex != -1 &&
                                             device->presentQueueFamilyIndex !=
                                             device->workQueueFamilyIndex);
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfo;
    deviceCreateInfo.enabledLayerCount = device->enabledLayerCount;
    deviceCreateInfo.ppEnabledLayerNames =
            (const char *const *) (device->enabledLayerCount != 0 ? device->enabledLayerNames
                                                                  : NULL);
    deviceCreateInfo.enabledExtensionCount = device->enabledExtensionCount;
    deviceCreateInfo.ppEnabledExtensionNames =
            (const char *const *) (device->enabledExtensionCount != 0
                                   ? device->enabledExtensionNames : NULL);
    deviceCreateInfo.pEnabledFeatures = NULL;

    VK(instance->vkCreateDevice(
            device->physicalDevice, &deviceCreateInfo, VK_ALLOCATOR, &device->device));

    // Get the device functions.
    GET_DEVICE_PROC_ADDR(vkDestroyDevice);
    GET_DEVICE_PROC_ADDR(vkGetDeviceQueue);
    GET_DEVICE_PROC_ADDR(vkQueueSubmit);
    GET_DEVICE_PROC_ADDR(vkQueueWaitIdle);
    GET_DEVICE_PROC_ADDR(vkDeviceWaitIdle);
    GET_DEVICE_PROC_ADDR(vkAllocateMemory);
    GET_DEVICE_PROC_ADDR(vkFreeMemory);
    GET_DEVICE_PROC_ADDR(vkMapMemory);
    GET_DEVICE_PROC_ADDR(vkUnmapMemory);
    GET_DEVICE_PROC_ADDR(vkFlushMappedMemoryRanges);
    GET_DEVICE_PROC_ADDR(vkBindBufferMemory);
    GET_DEVICE_PROC_ADDR(vkBindImageMemory);
    GET_DEVICE_PROC_ADDR(vkGetBufferMemoryRequirements);
    GET_DEVICE_PROC_ADDR(vkGetImageMemoryRequirements);
    GET_DEVICE_PROC_ADDR(vkCreateFence);
    GET_DEVICE_PROC_ADDR(vkDestroyFence);
    GET_DEVICE_PROC_ADDR(vkResetFences);
    GET_DEVICE_PROC_ADDR(vkGetFenceStatus);
    GET_DEVICE_PROC_ADDR(vkWaitForFences);
    GET_DEVICE_PROC_ADDR(vkCreateBuffer);
    GET_DEVICE_PROC_ADDR(vkDestroyBuffer);
    GET_DEVICE_PROC_ADDR(vkCreateImage);
    GET_DEVICE_PROC_ADDR(vkDestroyImage);
    GET_DEVICE_PROC_ADDR(vkCreateImageView);
    GET_DEVICE_PROC_ADDR(vkDestroyImageView);
    GET_DEVICE_PROC_ADDR(vkCreateShaderModule);
    GET_DEVICE_PROC_ADDR(vkDestroyShaderModule);
    GET_DEVICE_PROC_ADDR(vkCreatePipelineCache);
    GET_DEVICE_PROC_ADDR(vkDestroyPipelineCache);
    GET_DEVICE_PROC_ADDR(vkCreateGraphicsPipelines);
    GET_DEVICE_PROC_ADDR(vkDestroyPipeline);

    GET_DEVICE_PROC_ADDR(vkCreatePipelineLayout);
    GET_DEVICE_PROC_ADDR(vkDestroyPipelineLayout);
    GET_DEVICE_PROC_ADDR(vkCreateSampler);
    GET_DEVICE_PROC_ADDR(vkDestroySampler);
    GET_DEVICE_PROC_ADDR(vkCreateDescriptorSetLayout);
    GET_DEVICE_PROC_ADDR(vkDestroyDescriptorSetLayout);
    GET_DEVICE_PROC_ADDR(vkCreateDescriptorPool);
    GET_DEVICE_PROC_ADDR(vkDestroyDescriptorPool);
    GET_DEVICE_PROC_ADDR(vkResetDescriptorPool);
    GET_DEVICE_PROC_ADDR(vkAllocateDescriptorSets);
    GET_DEVICE_PROC_ADDR(vkFreeDescriptorSets);
    GET_DEVICE_PROC_ADDR(vkUpdateDescriptorSets);
    GET_DEVICE_PROC_ADDR(vkCreateFramebuffer);
    GET_DEVICE_PROC_ADDR(vkDestroyFramebuffer);
    GET_DEVICE_PROC_ADDR(vkCreateRenderPass);
    GET_DEVICE_PROC_ADDR(vkDestroyRenderPass);

    GET_DEVICE_PROC_ADDR(vkCreateCommandPool);
    GET_DEVICE_PROC_ADDR(vkDestroyCommandPool);
    GET_DEVICE_PROC_ADDR(vkResetCommandPool);
    GET_DEVICE_PROC_ADDR(vkAllocateCommandBuffers);
    GET_DEVICE_PROC_ADDR(vkFreeCommandBuffers);

    GET_DEVICE_PROC_ADDR(vkBeginCommandBuffer);
    GET_DEVICE_PROC_ADDR(vkEndCommandBuffer);
    GET_DEVICE_PROC_ADDR(vkResetCommandBuffer);

    GET_DEVICE_PROC_ADDR(vkCmdBindPipeline);
    GET_DEVICE_PROC_ADDR(vkCmdSetViewport);
    GET_DEVICE_PROC_ADDR(vkCmdSetScissor);

    GET_DEVICE_PROC_ADDR(vkCmdBindDescriptorSets);
    GET_DEVICE_PROC_ADDR(vkCmdBindIndexBuffer);
    GET_DEVICE_PROC_ADDR(vkCmdBindVertexBuffers);

    GET_DEVICE_PROC_ADDR(vkCmdDraw);

    GET_DEVICE_PROC_ADDR(vkCmdCopyBuffer);
    GET_DEVICE_PROC_ADDR(vkCmdResolveImage);
    GET_DEVICE_PROC_ADDR(vkCmdPipelineBarrier);

    GET_DEVICE_PROC_ADDR(vkCmdPushConstants);

    GET_DEVICE_PROC_ADDR(vkCmdBeginRenderPass);

    GET_DEVICE_PROC_ADDR(vkCmdEndRenderPass);

    return true;
}

void ovrVkDevice_Destroy(ovrVkDevice *device) {
    VK(device->vkDeviceWaitIdle(device->device));

    free(device->queueFamilyProperties);
    free(device->queueFamilyUsedQueues);

    pthread_mutex_destroy(&device->queueFamilyMutex);

    VC(device->vkDestroyDevice(device->device, VK_ALLOCATOR));
}

void ovrGpuDevice_CreateShader(
        ovrVkDevice *device,
        VkShaderModule *shaderModule,
        const VkShaderStageFlagBits stage,
        const void *code,
        size_t codeSize) {
    VkShaderModuleCreateInfo moduleCreateInfo;
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.pNext = NULL;
    moduleCreateInfo.flags = 0;
    moduleCreateInfo.codeSize = codeSize;
    moduleCreateInfo.pCode = code;

    if (*(uint32_t *) code == ICD_SPV_MAGIC) {
        moduleCreateInfo.codeSize = codeSize;
        moduleCreateInfo.pCode = (uint32_t *) code;

        VK(device->vkCreateShaderModule(
                device->device, &moduleCreateInfo, VK_ALLOCATOR, shaderModule));
    } else {
        // Create fake SPV structure to feed GLSL to the driver "under the covers".
        size_t tempCodeSize = 3 * sizeof(uint32_t) + codeSize + 1;
        uint32_t *tempCode = (uint32_t *) malloc(tempCodeSize);
        tempCode[0] = ICD_SPV_MAGIC;
        tempCode[1] = 0;
        tempCode[2] = stage;
        memcpy(tempCode + 3, code, codeSize + 1);

        moduleCreateInfo.codeSize = tempCodeSize;
        moduleCreateInfo.pCode = tempCode;

        VK(device->vkCreateShaderModule(
                device->device, &moduleCreateInfo, VK_ALLOCATOR, shaderModule));

        free(tempCode);
    }
}

/*
================================================================================================================================

GPU context.

A context encapsulates a queue that is used to submit command buffers.
A context can only be used by a single thread.
For optimal performance a context should only be created at load time, not at runtime.

================================================================================================================================
*/

bool ovrVkContext_Create(ovrVkContext *context, ovrVkDevice *device, const int queueIndex) {
    memset(context, 0, sizeof(ovrVkContext));

    if (pthread_mutex_trylock(&device->queueFamilyMutex) == EBUSY) {
        pthread_mutex_lock(&device->queueFamilyMutex);
    }
    assert((device->queueFamilyUsedQueues[device->workQueueFamilyIndex] & (1 << queueIndex)) == 0);
    device->queueFamilyUsedQueues[device->workQueueFamilyIndex] |= (1 << queueIndex);
    pthread_mutex_unlock(&device->queueFamilyMutex);

    context->device = device;
    context->queueFamilyIndex = device->workQueueFamilyIndex;
    context->queueIndex = queueIndex;

    VC(device->vkGetDeviceQueue(
            device->device, context->queueFamilyIndex, context->queueIndex, &context->queue));

    VkCommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = NULL;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = context->queueFamilyIndex;

    VK(device->vkCreateCommandPool(
            device->device, &commandPoolCreateInfo, VK_ALLOCATOR, &context->commandPool));

    return true;
}

void ovrVkContext_Destroy(ovrVkContext *context) {
    if (context->device == NULL) {
        return;
    }

    // Mark the queue as no longer in use.
    if (pthread_mutex_trylock(&context->device->queueFamilyMutex) == EBUSY) {
        pthread_mutex_lock(&context->device->queueFamilyMutex);
    }
    assert(
            (context->device->queueFamilyUsedQueues[context->queueFamilyIndex] &
             (1 << context->queueIndex)) != 0);
    context->device->queueFamilyUsedQueues[context->queueFamilyIndex] &=
            ~(1 << context->queueIndex);
    pthread_mutex_unlock(&context->device->queueFamilyMutex);

    if (context->setupCommandBuffer) {
        VC(context->device->vkFreeCommandBuffers(
                context->device->device, context->commandPool, 1, &context->setupCommandBuffer));
    }
    VC(context->device->vkDestroyCommandPool(
            context->device->device, context->commandPool, VK_ALLOCATOR));
}

void ovrVkContext_WaitIdle(ovrVkContext *context) {
    VK(context->device->vkQueueWaitIdle(context->queue));
}

/*
================================================================================================================================

Vulkan render pass.

A render pass encapsulates a sequence of graphics commands that can be executed in a single tiling
pass. For optimal performance a render pass should only be created at load time, not at runtime.
Render passes cannot overlap and cannot be nested.

================================================================================================================================
*/

static VkFormat ovrGpuColorBuffer_InternalSurfaceColorFormat(
        const ovrSurfaceColorFormat colorFormat) {
    return (
            (colorFormat == OVR_SURFACE_COLOR_FORMAT_R8G8B8A8)
            ? VK_FORMAT_R8G8B8A8_UNORM
            : ((colorFormat == OVR_SURFACE_COLOR_FORMAT_B8G8R8A8)
               ? VK_FORMAT_B8G8R8A8_UNORM
               : ((colorFormat == OVR_SURFACE_COLOR_FORMAT_R5G6B5)
                  ? VK_FORMAT_R5G6B5_UNORM_PACK16
                  : ((colorFormat == OVR_SURFACE_COLOR_FORMAT_B5G6R5)
                     ? VK_FORMAT_B5G6R5_UNORM_PACK16
                     : ((VK_FORMAT_UNDEFINED))))));
}

bool ovrVkRenderPass_Create(
        ovrVkContext *context,
        ovrVkRenderPass *renderPass,
        const ovrSurfaceColorFormat colorFormat,
        const ovrSampleCount sampleCount,
        const ovrVkRenderPassType type,
        const int flags,
        const ovrVector4f *clearColor) {
    assert(
            (context->device->physicalDeviceProperties.properties.limits.framebufferColorSampleCounts &
             (VkSampleCountFlags) sampleCount) != 0);
    assert(
            (context->device->physicalDeviceProperties.properties.limits.framebufferDepthSampleCounts &
             (VkSampleCountFlags) sampleCount) != 0);

    renderPass->type = type;
    renderPass->flags = flags;
    renderPass->colorFormat = colorFormat;
    renderPass->sampleCount = sampleCount;
    renderPass->internalColorFormat = ovrGpuColorBuffer_InternalSurfaceColorFormat(colorFormat);
    renderPass->clearColor = *clearColor;

    VkAttachmentDescription attachment;
    attachment.flags = 0;
    attachment.format = renderPass->internalColorFormat;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; //TODO: VK_IMAGE_LAYOUT_PRESENT_SRC_KHR ??? https://vulkan-tutorial.com/en/Drawing_a_triangle/Graphics_pipeline_basics/Render_passes

    VkAttachmentReference colorAttachmentReference;
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription;
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = NULL;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    subpassDescription.pResolveAttachments = NULL;
    subpassDescription.pDepthStencilAttachment = NULL;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = NULL;

    VkRenderPassCreateInfo renderPassCreateInfo;
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = NULL;
    renderPassCreateInfo.flags = 0;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &attachment;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.dependencyCount = 0;
    renderPassCreateInfo.pDependencies = NULL;

    VK(context->device->vkCreateRenderPass(
            context->device->device, &renderPassCreateInfo, VK_ALLOCATOR, &renderPass->renderPass));

    return true;
}

void ovrVkRenderPass_Destroy(ovrVkContext *context, ovrVkRenderPass *renderPass) {
    VC(context->device->vkDestroyRenderPass(
            context->device->device, renderPass->renderPass, VK_ALLOCATOR));
}

/*
================================================================================================================================

Vulkan framebuffer.

A framebuffer encapsulates a buffered set of textures.
For optimal performance a framebuffer should only be created at load time, not at runtime.

================================================================================================================================
*/

ovrScreenRect ovrVkFramebuffer_GetRect(const ovrVkFramebuffer *framebuffer) {
    ovrScreenRect rect;
    rect.x = 0;
    rect.y = 0;
    rect.width = framebuffer->width;
    rect.height = framebuffer->height;
    return rect;
}

int ovrVkFramebuffer_GetBufferCount(const ovrVkFramebuffer *framebuffer) {
    return framebuffer->numBuffers;
}

void VkPipelineLayout_Create(
        ovrVkContext *context,
        VkPipelineLayout *layout) {
    memset(layout, 0, sizeof(VkPipelineLayout));


    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = NULL;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = NULL;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = NULL;

    VK(context->device->vkCreatePipelineLayout(
            context->device->device,
            &pipelineLayoutCreateInfo,
            VK_ALLOCATOR,
            layout))
}

void VkPipelineLayout_Destroy(ovrVkContext *context, VkPipelineLayout *layout) {
    VC(context->device->vkDestroyPipelineLayout(
            context->device->device, *layout, VK_ALLOCATOR));
}

/*
================================================================================================================================

Vulkan graphics program.

A graphics program encapsulates a vertex and fragment program that are used to render geometry.
For optimal performance a graphics program should only be created at load time, not at runtime.

================================================================================================================================
*/

bool ovrVkGraphicsProgram_Create(
        ovrVkContext *context,
        ovrVkGraphicsProgram *program,
        const void *vertexSourceData,
        const size_t vertexSourceSize,
        const void *fragmentSourceData,
        const size_t fragmentSourceSize) {

    ovrGpuDevice_CreateShader(
            context->device,
            &program->vertexShaderModule,
            VK_SHADER_STAGE_VERTEX_BIT,
            vertexSourceData,
            vertexSourceSize);
    ovrGpuDevice_CreateShader(
            context->device,
            &program->fragmentShaderModule,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            fragmentSourceData,
            fragmentSourceSize);

    program->pipelineStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    program->pipelineStages[0].pNext = NULL;
    program->pipelineStages[0].flags = 0;
    program->pipelineStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    program->pipelineStages[0].module = program->vertexShaderModule;
    program->pipelineStages[0].pName = "main";
    program->pipelineStages[0].pSpecializationInfo = NULL;

    program->pipelineStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    program->pipelineStages[1].pNext = NULL;
    program->pipelineStages[1].flags = 0;
    program->pipelineStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    program->pipelineStages[1].module = program->fragmentShaderModule;
    program->pipelineStages[1].pName = "main";
    program->pipelineStages[1].pSpecializationInfo = NULL;

    VkPipelineLayout_Create(context, &program->pipelineLayout);

    return true;
}

void ovrVkGraphicsProgram_Destroy(ovrVkContext *context, ovrVkGraphicsProgram *program) {
    VkPipelineLayout_Destroy(context, &program->pipelineLayout);

    VC(context->device->vkDestroyShaderModule(
            context->device->device, program->vertexShaderModule, VK_ALLOCATOR));
    VC(context->device->vkDestroyShaderModule(
            context->device->device, program->fragmentShaderModule, VK_ALLOCATOR));
}

/*
================================================================================================================================

ovrVertex

================================================================================================================================
*/

VkVertexInputAttributeDescription *getVertexInputAttributeDescriptions() {
    VkVertexInputAttributeDescription *descriptions = malloc(
            sizeof(VkVertexInputAttributeDescription) * 2);

    descriptions[0].binding = 0;
    descriptions[0].location = 0;
    descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    descriptions[0].offset = offsetof(ovrVertex, pos);

    descriptions[1].binding = 0;
    descriptions[1].location = 1;
    descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    descriptions[1].offset = offsetof(ovrVertex, color);

    return descriptions;
}

VkVertexInputBindingDescription *getVertexInputBindingDescription() {
    VkVertexInputBindingDescription *bindingDescription = malloc(
            sizeof(VkVertexInputBindingDescription));

    bindingDescription[0].binding = 0;
    bindingDescription[0].stride = sizeof(ovrVertex);
    bindingDescription[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

void
ovrBuffer_Create(ovrVkContext *context, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                 VkDeviceSize size,
                 ovrBuffer *buffer) {
    VkBufferCreateInfo bufferInfo;
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = NULL;
    bufferInfo.flags = 0;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.queueFamilyIndexCount = 0;
    bufferInfo.pQueueFamilyIndices = NULL;

    VK(context->device->vkCreateBuffer(context->device->device, &bufferInfo, VK_ALLOCATOR,
                                       &buffer->buffer));

    VkMemoryRequirements memRequirements;
    VC(context->device->vkGetBufferMemoryRequirements(context->device->device,
                                                      buffer->buffer,
                                                      &memRequirements));

    int32_t memoryType = findMemoryType(context->device,
                                         memRequirements.memoryTypeBits,
                                         properties);
    if(memoryType < 0) {
        ALOGE("Couldn't find proper memory type!");
    }

    VkMemoryAllocateInfo allocInfo;
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = NULL;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryType;

    VK(context->device->vkAllocateMemory(context->device->device, &allocInfo, VK_ALLOCATOR,
                                         &buffer->bufferMemory))
    VK(context->device->vkBindBufferMemory(context->device->device, buffer->buffer,
                                           buffer->bufferMemory, 0));
}

void
ovrBuffer_Vertex_Create(ovrVkContext *context, const ovrVertex *vertices, uint32_t verticesLength,
                        ovrBuffer *buffer) {

    VkDeviceSize size = sizeof(ovrVertex) * verticesLength;
    ovrBuffer *stagingBuffer = malloc(sizeof(ovrBuffer));

    ovrBuffer_Create(context, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     size, stagingBuffer);

    void *data;
    VK(context->device->vkMapMemory(context->device->device, stagingBuffer->bufferMemory, 0,
                                    size, 0, &data));
    memcpy(data, vertices, (size_t) size);
    VC(context->device->vkUnmapMemory(context->device->device, stagingBuffer->bufferMemory));

    ovrBuffer_Create(context, VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     size, buffer);

    copyBuffer(context, *stagingBuffer, *buffer, size);

    VC(context->device->vkDestroyBuffer(context->device->device, stagingBuffer->buffer,
                                        VK_ALLOCATOR));
    VC(context->device->vkFreeMemory(context->device->device, stagingBuffer->bufferMemory,
                                     VK_ALLOCATOR));
}

void
copyBuffer(ovrVkContext *context, ovrBuffer srcBuffer, ovrBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo;
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.pNext = NULL;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = context->commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    VK(context->device->vkAllocateCommandBuffers(context->device->device, &allocInfo,
                                                 &commandBuffer));

    VkCommandBufferBeginInfo beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pNext = NULL;
    beginInfo.pInheritanceInfo = NULL;
    VK(context->device->vkBeginCommandBuffer(commandBuffer, &beginInfo));

    VkBufferCopy copyRegion;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    VC(context->device->vkCmdCopyBuffer(commandBuffer, srcBuffer.buffer, dstBuffer.buffer, 1,
                                        &copyRegion));
    VK(context->device->vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = NULL;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = NULL;
    submitInfo.pWaitDstStageMask = NULL;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = NULL;

    VK(context->device->vkQueueSubmit(context->queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK(context->device->vkQueueWaitIdle(context->queue));

    VC(context->device->vkFreeCommandBuffers(context->device->device, context->commandPool, 1,
                                             &commandBuffer));
}

int32_t findMemoryType(ovrVkDevice *device, uint32_t typeFilter,
                        VkMemoryPropertyFlags properties) {


    for (uint32_t i = 0; i < device->physicalDeviceMemoryProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (device->physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) ==
            properties) {
            return i;
        }
    }

    return -1;
}

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

void ovrVkGraphicsPipelineParms_Init(ovrVkGraphicsPipelineParms *parms) {
    parms->rop.blendEnable = false;
    parms->rop.redWriteEnable = true;
    parms->rop.blueWriteEnable = true;
    parms->rop.greenWriteEnable = true;
    parms->rop.alphaWriteEnable = false;
    parms->rop.frontFace = OVR_FRONT_FACE_COUNTER_CLOCKWISE;
    parms->rop.cullMode = OVR_CULL_MODE_BACK;
    parms->rop.depthCompare = OVR_COMPARE_OP_LESS_OR_EQUAL;
    parms->rop.blendColor.x = 0.0f;
    parms->rop.blendColor.y = 0.0f;
    parms->rop.blendColor.z = 0.0f;
    parms->rop.blendColor.w = 0.0f;
    parms->rop.blendOpColor = OVR_BLEND_OP_ADD;
    parms->rop.blendSrcColor = OVR_BLEND_FACTOR_ONE;
    parms->rop.blendDstColor = OVR_BLEND_FACTOR_ZERO;
    parms->rop.blendOpAlpha = OVR_BLEND_OP_ADD;
    parms->rop.blendSrcAlpha = OVR_BLEND_FACTOR_ONE;
    parms->rop.blendDstAlpha = OVR_BLEND_FACTOR_ZERO;
    parms->renderPass = NULL;
    parms->program = NULL;
}

bool ovrVkGraphicsPipeline_Create(
        ovrVkContext *context,
        ovrVkGraphicsPipeline *pipeline,
        const ovrVkGraphicsPipelineParms *parms,
        ovrScreenRect screenRect) {

    VkVertexInputBindingDescription *bindingDescription = getVertexInputBindingDescription();
    VkVertexInputAttributeDescription *attributeDescriptions = getVertexInputAttributeDescriptions();

    pipeline->rop = parms->rop;
    pipeline->program = parms->program;

    pipeline->vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipeline->vertexInputState.pNext = NULL;
    pipeline->vertexInputState.flags = 0;
    pipeline->vertexInputState.vertexBindingDescriptionCount = 1;
    pipeline->vertexInputState.pVertexBindingDescriptions = bindingDescription;
    pipeline->vertexInputState.vertexAttributeDescriptionCount = 2;
    pipeline->vertexInputState.pVertexAttributeDescriptions = attributeDescriptions;

    pipeline->inputAssemblyState.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipeline->inputAssemblyState.pNext = NULL;
    pipeline->inputAssemblyState.flags = 0;
    pipeline->inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipeline->inputAssemblyState.primitiveRestartEnable = VK_FALSE;

    VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo;
    tessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellationStateCreateInfo.pNext = NULL;
    tessellationStateCreateInfo.flags = 0;
    tessellationStateCreateInfo.patchControlPoints = 0;

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) screenRect.width;
    viewport.height = (float) screenRect.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkOffset2D offset;
    offset.x = 0;
    offset.y = 0;
    VkExtent2D extent;
    extent.width = screenRect.width;
    extent.height = screenRect.height;
    VkRect2D scissor;
    scissor.offset = offset;
    scissor.extent = extent;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo;
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.pNext = NULL;
    viewportStateCreateInfo.flags = 0;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.pNext = NULL;
    rasterizationStateCreateInfo.flags = 0;
    // NOTE: If the depth clamping feature is not enabled, depthClampEnable must be VK_FALSE.
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.cullMode = (VkCullModeFlags) parms->rop.cullMode;
    rasterizationStateCreateInfo.frontFace = (VkFrontFace) parms->rop.frontFace;
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
    rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
    rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
    rasterizationStateCreateInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo;
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.pNext = NULL;
    multisampleStateCreateInfo.flags = 0;
    multisampleStateCreateInfo.rasterizationSamples =
            (VkSampleCountFlagBits) parms->renderPass->sampleCount;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleStateCreateInfo.minSampleShading = 1.0f;
    multisampleStateCreateInfo.pSampleMask = NULL;
    multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
    multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachementState[1];
    colorBlendAttachementState[0].blendEnable = parms->rop.blendEnable ? VK_TRUE : VK_FALSE;
    colorBlendAttachementState[0].srcColorBlendFactor = (VkBlendFactor) parms->rop.blendSrcColor;
    colorBlendAttachementState[0].dstColorBlendFactor = (VkBlendFactor) parms->rop.blendDstColor;
    colorBlendAttachementState[0].colorBlendOp = (VkBlendOp) parms->rop.blendOpColor;
    colorBlendAttachementState[0].srcAlphaBlendFactor = (VkBlendFactor) parms->rop.blendSrcAlpha;
    colorBlendAttachementState[0].dstAlphaBlendFactor = (VkBlendFactor) parms->rop.blendDstAlpha;
    colorBlendAttachementState[0].alphaBlendOp = (VkBlendOp) parms->rop.blendOpAlpha;
    colorBlendAttachementState[0].colorWriteMask =
            (parms->rop.redWriteEnable ? VK_COLOR_COMPONENT_R_BIT : 0) |
            (parms->rop.blueWriteEnable ? VK_COLOR_COMPONENT_G_BIT : 0) |
            (parms->rop.greenWriteEnable ? VK_COLOR_COMPONENT_B_BIT : 0) |
            (parms->rop.alphaWriteEnable ? VK_COLOR_COMPONENT_A_BIT : 0);

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.pNext = NULL;
    colorBlendStateCreateInfo.flags = 0;
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_CLEAR;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = colorBlendAttachementState;
    colorBlendStateCreateInfo.blendConstants[0] = parms->rop.blendColor.x;
    colorBlendStateCreateInfo.blendConstants[1] = parms->rop.blendColor.y;
    colorBlendStateCreateInfo.blendConstants[2] = parms->rop.blendColor.z;
    colorBlendStateCreateInfo.blendConstants[3] = parms->rop.blendColor.w;

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo;
    graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphicsPipelineCreateInfo.pNext = NULL;
    graphicsPipelineCreateInfo.flags = 0;
    graphicsPipelineCreateInfo.stageCount = 2;
    graphicsPipelineCreateInfo.pStages = parms->program->pipelineStages;
    graphicsPipelineCreateInfo.pVertexInputState = &pipeline->vertexInputState;
    graphicsPipelineCreateInfo.pInputAssemblyState = &pipeline->inputAssemblyState;
    graphicsPipelineCreateInfo.pTessellationState = &tessellationStateCreateInfo;
    graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    graphicsPipelineCreateInfo.pDepthStencilState = NULL;
    graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    graphicsPipelineCreateInfo.pDynamicState = NULL;
    graphicsPipelineCreateInfo.layout = parms->program->pipelineLayout;
    graphicsPipelineCreateInfo.renderPass = parms->renderPass->renderPass;
    graphicsPipelineCreateInfo.subpass = 0;
    graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    graphicsPipelineCreateInfo.basePipelineIndex = 0;

    VK(context->device->vkCreateGraphicsPipelines(
            context->device->device,
            VK_NULL_HANDLE,
            1,
            &graphicsPipelineCreateInfo,
            VK_ALLOCATOR,
            &pipeline->pipeline));

    return true;
}

void ovrVkGraphicsPipeline_Destroy(ovrVkContext *context, ovrVkGraphicsPipeline *pipeline) {
    VC(context->device->vkDestroyPipeline(
            context->device->device, pipeline->pipeline, VK_ALLOCATOR));

    memset(pipeline, 0, sizeof(ovrVkGraphicsPipeline));
}

/*
================================================================================================================================

Vulkan fence.

A fence is used to notify completion of a command buffer.
For optimal performance a fence should only be created at load time, not at runtime.

================================================================================================================================
*/

void VkFence_Create(ovrVkContext *context, VkFence *fence) {
    VkFenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = NULL;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK(context->device->vkCreateFence(
            context->device->device, &fenceCreateInfo, VK_ALLOCATOR, fence));

}

void VkFence_Destroy(ovrVkContext *context, VkFence *fence) {
    VC(context->device->vkDestroyFence(context->device->device, *fence, VK_ALLOCATOR));
}

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

void ovrVkGraphicsCommand_Init(ovrVkGraphicsCommand *command) {
    command->pipeline = NULL;
}

void ovrVkGraphicsCommand_SetPipeline(
        ovrVkGraphicsCommand *command,
        const ovrVkGraphicsPipeline *pipeline) {
    command->pipeline = pipeline;
}

/*
================================================================================================================================

Vulkan command buffer.

A command buffer is used to record graphics and compute commands.
For optimal performance a command buffer should only be created at load time, not at runtime.
When a command is submitted, the state of the command is compared with the currently saved state,
and only the state that has changed translates into graphics API function calls.

================================================================================================================================
*/

void ovrVkCommandBuffer_Create(
        ovrVkContext *context,
        ovrVkCommandBuffer *commandBuffer,
        const ovrVkCommandBufferType type,
        const int numBuffers) {
    memset(commandBuffer, 0, sizeof(ovrVkCommandBuffer));

    commandBuffer->type = type;
    commandBuffer->numBuffers = numBuffers;
    commandBuffer->currentBuffer = 0;
    commandBuffer->context = context;
    commandBuffer->cmdBuffers = (VkCommandBuffer *) malloc(numBuffers * sizeof(VkCommandBuffer));
    commandBuffer->fences = (VkFence *) malloc(numBuffers * sizeof(VkFence));

    for (int i = 0; i < numBuffers; i++) {
        VkCommandBufferAllocateInfo commandBufferAllocateInfo;
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.pNext = NULL;
        commandBufferAllocateInfo.commandPool = context->commandPool;
        commandBufferAllocateInfo.level = (type == OVR_COMMAND_BUFFER_TYPE_PRIMARY)
                                          ? VK_COMMAND_BUFFER_LEVEL_PRIMARY
                                          : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        commandBufferAllocateInfo.commandBufferCount = 1;

        VK(context->device->vkAllocateCommandBuffers(
                context->device->device, &commandBufferAllocateInfo,
                &commandBuffer->cmdBuffers[i]));

        VkFence_Create(context, &commandBuffer->fences[i]);
    }
}

void ovrVkCommandBuffer_Destroy(ovrVkContext *context, ovrVkCommandBuffer *commandBuffer) {
    assert(context == commandBuffer->context);

    for (int i = 0; i < commandBuffer->numBuffers; i++) {
        VC(context->device->vkFreeCommandBuffers(
                context->device->device, context->commandPool, 1, &commandBuffer->cmdBuffers[i]));

        VkFence_Destroy(context, &commandBuffer->fences[i]);
    }

    free(commandBuffer->fences);
    free(commandBuffer->cmdBuffers);

    memset(commandBuffer, 0, sizeof(ovrVkCommandBuffer));
}

void ovrVkCommandBuffer_BeginPrimary(ovrVkCommandBuffer *commandBuffer) {
    assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
    assert(commandBuffer->currentFramebuffer == NULL);
    assert(commandBuffer->currentRenderPass == NULL);

    ovrVkDevice *device = commandBuffer->context->device;

    commandBuffer->currentBuffer = (commandBuffer->currentBuffer + 1) % commandBuffer->numBuffers;

    VkFence *fence = &commandBuffer->fences[commandBuffer->currentBuffer];

    VK(device->vkWaitForFences(
            device->device, 1, fence, VK_TRUE, UINT64_MAX));
    VK(device->vkResetFences(device->device, 1, fence));

    ovrVkGraphicsCommand_Init(&commandBuffer->currentGraphicsState);

    VK(device->vkResetCommandBuffer(commandBuffer->cmdBuffers[commandBuffer->currentBuffer], 0));

    VkCommandBufferBeginInfo commandBufferBeginInfo;
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = NULL;
    commandBufferBeginInfo.flags = 0;
    commandBufferBeginInfo.pInheritanceInfo = NULL;

    VK(device->vkBeginCommandBuffer(
            commandBuffer->cmdBuffers[commandBuffer->currentBuffer], &commandBufferBeginInfo));
}

void ovrVkCommandBuffer_EndPrimary(ovrVkCommandBuffer *commandBuffer) {
    assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
    assert(commandBuffer->currentFramebuffer == NULL);
    assert(commandBuffer->currentRenderPass == NULL);

    ovrVkDevice *device = commandBuffer->context->device;
    VK(device->vkEndCommandBuffer(commandBuffer->cmdBuffers[commandBuffer->currentBuffer]));
}

VkFence *ovrVkCommandBuffer_SubmitPrimary(ovrVkCommandBuffer *commandBuffer) {
    assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
    assert(commandBuffer->currentFramebuffer == NULL);
    assert(commandBuffer->currentRenderPass == NULL);

    ovrVkDevice *device = commandBuffer->context->device;

    const VkPipelineStageFlags stageFlags[1] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = NULL;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = NULL;
    submitInfo.pWaitDstStageMask = NULL;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer->cmdBuffers[commandBuffer->currentBuffer];
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = NULL;

    VkFence *fence = &commandBuffer->fences[commandBuffer->currentBuffer];
    VK(device->vkQueueSubmit(commandBuffer->context->queue, 1, &submitInfo, *fence));

    return fence;
}

void ovrVkCommandBuffer_BeginFramebuffer(
        ovrVkCommandBuffer *commandBuffer,
        ovrVkFramebuffer *framebuffer,
        const int arrayLayer) {
    assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
    assert(commandBuffer->currentFramebuffer == NULL);
    assert(commandBuffer->currentRenderPass == NULL);
    assert(arrayLayer >= 0 && arrayLayer < framebuffer->numLayers);

    // Only advance when rendering to the first layer.
    if (arrayLayer == 0) {
        framebuffer->currentBuffer = (framebuffer->currentBuffer + 1) % framebuffer->numBuffers;
    }
    framebuffer->currentLayer = arrayLayer;

    commandBuffer->currentFramebuffer = framebuffer;
}

void ovrVkCommandBuffer_EndFramebuffer(
        ovrVkCommandBuffer *commandBuffer,
        ovrVkFramebuffer *framebuffer,
        const int arrayLayer) {
    assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
    assert(commandBuffer->currentFramebuffer == framebuffer);
    assert(commandBuffer->currentRenderPass == NULL);
    assert(arrayLayer >= 0 && arrayLayer < framebuffer->numLayers);

    commandBuffer->currentFramebuffer = NULL;
}

void ovrVkCommandBuffer_BeginRenderPass(
        ovrVkCommandBuffer *commandBuffer,
        ovrVkRenderPass *renderPass,
        ovrVkFramebuffer *framebuffer,
        const ovrScreenRect *rect) {
    assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
    assert(commandBuffer->currentRenderPass == NULL);
    assert(commandBuffer->currentFramebuffer == framebuffer);

    ovrVkDevice *device = commandBuffer->context->device;

    VkCommandBuffer cmdBuffer = commandBuffer->cmdBuffers[commandBuffer->currentBuffer];

    VkClearValue clearValues[1];
    memset(clearValues, 0, sizeof(clearValues));

    clearValues[0].color.float32[0] = renderPass->clearColor.x;
    clearValues[0].color.float32[1] = renderPass->clearColor.y;
    clearValues[0].color.float32[2] = renderPass->clearColor.z;
    clearValues[0].color.float32[3] = renderPass->clearColor.w;

    VkRenderPassBeginInfo renderPassBeginInfo;
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = NULL;
    renderPassBeginInfo.renderPass = renderPass->renderPass;
    renderPassBeginInfo.framebuffer =
            framebuffer->framebuffers[framebuffer->currentBuffer + framebuffer->currentLayer];
    renderPassBeginInfo.renderArea.offset.x = rect->x;
    renderPassBeginInfo.renderArea.offset.y = rect->y;
    renderPassBeginInfo.renderArea.extent.width = rect->width;
    renderPassBeginInfo.renderArea.extent.height = rect->height;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = clearValues;

    VC(device->vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE));

    commandBuffer->currentRenderPass = renderPass;
}

void ovrVkCommandBuffer_EndRenderPass(
        ovrVkCommandBuffer *commandBuffer,
        ovrVkRenderPass *renderPass) {
    assert(commandBuffer->type == OVR_COMMAND_BUFFER_TYPE_PRIMARY);
    assert(commandBuffer->currentRenderPass == renderPass);

    ovrVkDevice *device = commandBuffer->context->device;

    VkCommandBuffer cmdBuffer = commandBuffer->cmdBuffers[commandBuffer->currentBuffer];

    VC(device->vkCmdEndRenderPass(cmdBuffer));

    commandBuffer->currentRenderPass = NULL;
}

void ovrVkCommandBuffer_SubmitGraphicsCommand(
        ovrVkCommandBuffer *commandBuffer,
        const ovrBuffer *vertexBuffer,
        const ovrVkGraphicsCommand *command,
        uint32_t verticesLength) {
    assert(commandBuffer->currentRenderPass != NULL);

    ovrVkDevice *device = commandBuffer->context->device;

    VkCommandBuffer cmdBuffer = commandBuffer->cmdBuffers[commandBuffer->currentBuffer];
    const ovrVkGraphicsCommand *state = &commandBuffer->currentGraphicsState;

    // If the pipeline has changed.
    if (command->pipeline != state->pipeline) {
        VC(device->vkCmdBindPipeline(
                cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, command->pipeline->pipeline));
    }

    VkBuffer vertexBuffers[] = {vertexBuffer->buffer};
    VkDeviceSize offsets[] = {0};
    VC(device->vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets));

    VC(device->vkCmdDraw(cmdBuffer, verticesLength, 1, 0, 0));

    commandBuffer->currentGraphicsState = *command;
}
