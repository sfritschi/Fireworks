#include "graphics.h"

#include <string.h>
#include <time.h>  // time()

// Globals
static const char *const VALIDATION_LAYER_NAME = "VK_LAYER_KHRONOS_validation";
static const char *REQ_DEVICE_EXTENSIONS[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// TODO: Maybe move helper functions to different compilation unit
// --- Start Helper functions
// Allocate new string with same contents as src (NULL-terminated)
static char *copyString(const char *src)
{
    const size_t size = strlen(src) + 1;  // +1 for '\0'
    char *dst = malloc(size * sizeof(char));
    strncpy(dst, src, size);
    return dst;
}

// Clamp u32 value to prescribed range [min, max]
static uint32_t clampu32(uint32_t x, uint32_t min, uint32_t max)
{
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

static char *readBinFile(const char *fileName, uint32_t *fileSize)
{
    FILE *fp = fopen(fileName, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open file: '%s'\n", fileName);
        exit(EXIT_FAILURE);
    }
    
    // Move to end of file
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "fseek() error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    *fileSize = (uint32_t) ftell(fp);
    // Go back to beginning
    rewind(fp);
    // Allocate buffer containing file contents
    char *bin = NULL;
    CHK_ALLOC(bin = malloc(*fileSize * sizeof(char)));
    // Read 
    if (fread(bin, sizeof(char), *fileSize, fp) != *fileSize * sizeof(char)) {
        fprintf(stderr, "Failed to read file: '%s'\n", fileName);
        exit(EXIT_FAILURE);
    }
    // Cleanup
    fclose(fp);
    
    return bin;
}
// --- End Helper functions

// Callbacks
static void glfwErrorCallback(int error, const char* description)
{
    (void)error;
    
    fprintf(stderr, "GLFW error: %s\n", description);
    exit(EXIT_FAILURE);  // cannot continue
}

static void glfwKeyCallback(GLFWwindow *window, int key, int scancode, 
    int action, int mods)
{
    (void)scancode;
    (void)mods;
    
    if ((key == GLFW_KEY_Q || key == GLFW_KEY_ESCAPE) && action == GLFW_PRESS) {
        // Close window
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

static void framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
    (void)width;
    (void)height;
    
    Graphics graphics = (Graphics) glfwGetWindowUserPointer(window);
    // Set flag in GraphicsData struct indicating the change
    graphics->framebufferResized = VK_TRUE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
{
    (void) messageSeverity;
    (void) messageType;
    (void) pUserData;
    
    fprintf(stderr, "Validation layer: %s\n", pCallbackData->pMessage);
    return VK_FALSE;
}
    
VkResult createDebugUtilsMessengerEXT(VkInstance instance, 
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
    const VkAllocationCallbacks* pAllocator, 
    VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    
    if (func) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

/*
 *  Try to load destroy function for debug utils messenger extension object
 */
void destroyDebugUtilsMessengerEXT(VkInstance instance, 
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator)
{
    PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    
    if (func) {
        func(instance, debugMessenger, pAllocator);
    }
}

static void initWindow(Graphics graphics)
{
    assert(graphics && "Expected non-NULL graphics handle");
    
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        exit(EXIT_FAILURE);  // cannot continue
    }
    
    glfwSetErrorCallback(glfwErrorCallback);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // no OpenGL context
    // Use minimum required version of 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    // Create window
    graphics->window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
        "Fireworks", NULL, NULL);
    
    if (!graphics->window) {
        fprintf(stderr, "Failed to create window\n");
        exit(EXIT_FAILURE);  // cannot continue
    }
    
    // Store pointer to graphics handle for reuse within GLFW callback
    glfwSetWindowUserPointer(graphics->window, graphics);
    glfwSetFramebufferSizeCallback(graphics->window, framebufferResizeCallback);
    glfwSetKeyCallback(graphics->window, glfwKeyCallback);
}

static VkBool32 checkValidationLayerSupport()
{
    uint32_t layerCount = 0;
    CHK_VK_ERR(vkEnumerateInstanceLayerProperties(&layerCount, NULL),
        "Failed to get layer count\n");
    // Allocate buffer for storing properties
    VkLayerProperties *availableLayers = NULL;
    CHK_ALLOC(availableLayers = malloc(layerCount * sizeof(VkLayerProperties)));
    
    CHK_VK_ERR(vkEnumerateInstanceLayerProperties(&layerCount, 
        availableLayers), "Failed to list available layers\n");
    
    VkBool32 validationLayerFound = VK_FALSE;
    for (uint32_t i = 0; i < layerCount; ++i) {
        if (strncmp(availableLayers[i].layerName, VALIDATION_LAYER_NAME,
                VK_MAX_EXTENSION_NAME_SIZE) == 0) {
            validationLayerFound = VK_TRUE;
            break;
        }
    }
    // Cleanup
    free(availableLayers);
    
    return validationLayerFound;
}

// Initialize Vulkan instance and setup debug messenger callback 
static void initInstance(Graphics graphics)
{    
    assert(graphics && "Expected non-NULL graphics handle");
    
    if (ENABLE_VALIDATION_LAYERS && !checkValidationLayerSupport()) {
        fprintf(stderr, "No support for validation layers. Exiting...\n");
        exit(EXIT_FAILURE);
    }
    
    // Obtain Vulkan API version
    uint32_t apiVersion = 0;
    CHK_VK_ERR(vkEnumerateInstanceVersion(&apiVersion), 
        "Failed to get Vulkan API version\n");
    printf("Vulkan API version: %u.%u.%u\n", VK_API_VERSION_MAJOR(apiVersion),
        VK_API_VERSION_MINOR(apiVersion), VK_API_VERSION_PATCH(apiVersion));
    
    // Create Vulkan instance
    VkApplicationInfo appInfo = {0};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Fireworks";
    // Note: Use version 1.0.0 for now
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";  // no engine used
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    // Use instance-level version of system
    appInfo.apiVersion = VK_API_VERSION_1_3;
    
    VkInstanceCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    
    // Verify first that all required extensions of glfw are supported
    uint32_t vkExtensionCount = 0;
    CHK_VK_ERR(vkEnumerateInstanceExtensionProperties(NULL, 
        &vkExtensionCount, NULL), "Failed to get Vulkan extension count\n");
    
    VkExtensionProperties *vkExtensions = NULL;
    CHK_ALLOC(vkExtensions = malloc(vkExtensionCount * sizeof(VkExtensionProperties)));
    
    CHK_VK_ERR(vkEnumerateInstanceExtensionProperties(NULL,
        &vkExtensionCount, vkExtensions), "Failed to list Vulkan extensions\n");
    
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = NULL;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (!glfwExtensions) {
        fprintf(stderr, "Failed to get GLFW extension count\n");
        exit(EXIT_FAILURE);
    }
    // Add also DEBUG extension
    uint32_t requiredExtensionCount = glfwExtensionCount;
    if (ENABLE_VALIDATION_LAYERS) requiredExtensionCount += 1;
    
    char **requiredExtensions = NULL;
    CHK_ALLOC(requiredExtensions = malloc(requiredExtensionCount * sizeof(char *)));
    for (uint32_t i = 0; i < glfwExtensionCount; ++i)  {
        requiredExtensions[i] = copyString(glfwExtensions[i]);
    }
    
    if (ENABLE_VALIDATION_LAYERS) {
        // Add also DEBUG extension
        requiredExtensions[requiredExtensionCount-1] = 
            copyString(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    VkBool32 isFound = VK_FALSE;
    for (uint32_t i = 0; i < requiredExtensionCount; ++i) {
        // Linear search to check if required GLFW extension is supported
        const char *glfwExtension = requiredExtensions[i];

        isFound = VK_FALSE;
        for (uint32_t j = 0; j < vkExtensionCount; ++j) {
            if (strncmp(glfwExtension, vkExtensions[j].extensionName, 
                    VK_MAX_EXTENSION_NAME_SIZE) == 0) {
                isFound = VK_TRUE;
                break;
            }
        }
        
        if (!isFound) {
            fprintf(stderr, "Required GLFW extension '%s' not supported\n",
                glfwExtension);
            exit(EXIT_FAILURE);
        }
    }
    // Cleanup Vulkan extensions
    free(vkExtensions);
    
    createInfo.enabledExtensionCount = requiredExtensionCount;
    createInfo.ppEnabledExtensionNames = (const char *const *)requiredExtensions;
    
    // Create debug messenger for vkCreate/vkDestroy instance
    VkDebugUtilsMessengerCreateInfoEXT debugInfo = {0};
    if (ENABLE_VALIDATION_LAYERS) {
        // Note: We are only using 1 validation layer
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = &VALIDATION_LAYER_NAME;
        // Populate debug messenger info struct
        debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback = debugCallback;
        debugInfo.pUserData = NULL;  // optional
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = NULL;
    }
    // Finally create Vulkan instance
    CHK_VK_ERR(vkCreateInstance(&createInfo, NULL, &graphics->instance),
        "Failed to create Vulkan instance");
    // Setup debug messenger associated with instance
    if (ENABLE_VALIDATION_LAYERS) {
        CHK_VK_ERR(createDebugUtilsMessengerEXT(graphics->instance,
            &debugInfo, NULL, &graphics->debugMessenger),
            "Failed to setup debug messenger");
    }
    
    // Cleanup required extensions
    for (uint32_t i = 0; i < requiredExtensionCount; ++i) {
        free(requiredExtensions[i]);
    }
    free(requiredExtensions);
}

static void fillSwapChainSupport(Graphics graphics, VkPhysicalDevice device,
    SwapChainSupport *details)
{
    CHK_VK_ERR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, graphics->surface,
        &details->capabilities), "Failed to fetch surface capabilities\n");
    
    details->formatCount = 0;
    CHK_VK_ERR(vkGetPhysicalDeviceSurfaceFormatsKHR(device, graphics->surface,
        &details->formatCount, NULL), "Failed to get count of surface formats\n");
    
    // Allocate formats buffer
    if (details->formatCount > 0) {
        CHK_ALLOC(details->formats = malloc(details->formatCount * sizeof(VkSurfaceFormatKHR)));
        CHK_VK_ERR(vkGetPhysicalDeviceSurfaceFormatsKHR(device, graphics->surface,
            &details->formatCount, details->formats), "Failed to list surface formats\n");
    }
    
    details->presentCount = 0;
    CHK_VK_ERR(vkGetPhysicalDeviceSurfacePresentModesKHR(device, graphics->surface,
        &details->presentCount, NULL), "Failed to get count of present modes\n");
    
    if (details->presentCount > 0) {
        CHK_ALLOC(details->presentModes = malloc(details->presentCount * sizeof(VkPresentModeKHR)));
        CHK_VK_ERR(vkGetPhysicalDeviceSurfacePresentModesKHR(device, graphics->surface,
            &details->presentCount, details->presentModes), "Failed to list present modes\n");
    }
}

static void cleanupSwapChainSupport(SwapChainSupport *details)
{
    FREE_NULL(details->formats);
    details->formatCount = 0;
    FREE_NULL(details->presentModes);
    details->presentCount = 0;
}

// Returns true if Vulkan implementation meets all requirements
static VkBool32 isDeviceSuitable(Graphics graphics, VkPhysicalDevice device,
    QueueFamilyIndices *indices, SwapChainSupport *swapChainSupport)
{    
    VkPhysicalDeviceProperties deviceProps;
    vkGetPhysicalDeviceProperties(device, &deviceProps);
    
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    
    // Find queue families of physical device
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);
    assert(queueFamilyCount > 0);
    
    VkQueueFamilyProperties *queueProps = NULL;
    CHK_ALLOC(queueProps = malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties)));
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueProps);
    
    // Note: Initialize with invalid values
    *swapChainSupport = (SwapChainSupport) {0};
    *indices = (QueueFamilyIndices) {
        .graphicsFamily = queueFamilyCount, 
        .presentFamily = queueFamilyCount
    };
    
    VkBool32 foundGraphicsQueue = VK_FALSE;
    VkBool32 foundPresentQueue = VK_FALSE;
    // Look for a graphics queue and a present queue separately (likely the same)
    // Note: Require graphics queue to also have compute capabilities
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (!foundGraphicsQueue && 
            (queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            (queueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) 
        {
            indices->graphicsFamily = i;
            foundGraphicsQueue = VK_TRUE;
        }
        
        VkBool32 supportsPresent = VK_FALSE;
        CHK_VK_ERR(vkGetPhysicalDeviceSurfaceSupportKHR(device, i,
            graphics->surface, &supportsPresent), 
            "Failed to query presentation support\n");
        
        if (!foundPresentQueue && supportsPresent) {
            indices->presentFamily = i;
            foundPresentQueue = VK_TRUE;
        }
        
        if (foundGraphicsQueue && foundPresentQueue) {
            break;  // done
        }
    }
    // Cleanup
    free(queueProps);
    
    if (!foundGraphicsQueue || !foundPresentQueue) {
        return VK_FALSE;  // early termination
    }
    
    // Check if required device extensions are supported
    uint32_t extensionCount = 0;
    CHK_VK_ERR(vkEnumerateDeviceExtensionProperties(device, NULL,
        &extensionCount, NULL), "Failed to fetch number of device extensions");
    
    assert(extensionCount > 0);
    
    VkExtensionProperties *deviceExtensions = NULL;
    CHK_ALLOC(deviceExtensions = malloc(extensionCount * sizeof(VkExtensionProperties)));
    
    CHK_VK_ERR(vkEnumerateDeviceExtensionProperties(device, NULL,
        &extensionCount, deviceExtensions), 
        "Failed to list available device extensions");
    
    VkBool32 foundReqDevExt = VK_FALSE;
    const uint32_t nReqs = sizeof(REQ_DEVICE_EXTENSIONS) / sizeof(REQ_DEVICE_EXTENSIONS[0]);
    for (uint32_t i = 0; i < nReqs; ++i) {
        const char *required = REQ_DEVICE_EXTENSIONS[i];
        
        foundReqDevExt = VK_FALSE;
        for (uint32_t j = 0; j < extensionCount; ++j) {
            if (strncmp(required, deviceExtensions[j].extensionName,
                    VK_MAX_EXTENSION_NAME_SIZE) == 0) {
                foundReqDevExt = VK_TRUE;
                break;
            }
        }
        
        if (!foundReqDevExt) {
            break;  // at least 1 required extension is NOT supported
        }
    }
    // Cleanup
    free(deviceExtensions);
    
    if (!foundReqDevExt) {
        return VK_FALSE;  // early termination
    }
    
    // Note: Possibly allocates memory for swapChainSupport
    fillSwapChainSupport(graphics, device, swapChainSupport);
     
    // Require at least 1 supported surface format and presentation mode
    if (swapChainSupport->formatCount == 0 ||
        swapChainSupport->presentCount == 0)
    {
        return VK_FALSE;
    }
    
    printf("Using device: %s\n", deviceProps.deviceName);
    
    return VK_TRUE;
}

static void setMsaaSamples(Graphics graphics)
{
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(graphics->physicalDevice, &props);
    
    const VkSampleCountFlagBits sampleCounts[] = {
        VK_SAMPLE_COUNT_64_BIT,
        VK_SAMPLE_COUNT_32_BIT,
        VK_SAMPLE_COUNT_16_BIT,
        VK_SAMPLE_COUNT_8_BIT,
        VK_SAMPLE_COUNT_4_BIT,
        VK_SAMPLE_COUNT_2_BIT,
        VK_SAMPLE_COUNT_1_BIT  // no multisampling
    };
    
    const uint32_t nSampleCounts = sizeof(sampleCounts) / sizeof(sampleCounts[0]);
    // Note: Does not consider depth buffer MSAA support
    const VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts;
    // Pick highest supported sample count
    for (uint32_t i = 0; i < nSampleCounts; ++i) {
        if  (counts & sampleCounts[i]) {
            graphics->msaaSamples = sampleCounts[i];
            return;
        }
    }
    
    assert(VK_FALSE && "Unreachable");
}

static void selectPhysicalDevice(Graphics graphics)
{    
    uint32_t deviceCount = 0;
    CHK_VK_ERR(vkEnumeratePhysicalDevices(graphics->instance, &deviceCount,
        NULL), "Failed to fetch device count\n");
    
    if (deviceCount == 0) {
        fprintf(stderr, "No device with Vulkan support found\n");
        exit(EXIT_FAILURE);  // cannot continue
    }
    
    VkPhysicalDevice *devices = NULL;
    CHK_ALLOC(devices = malloc(deviceCount * sizeof(VkPhysicalDevice)));
    
    CHK_VK_ERR(vkEnumeratePhysicalDevices(graphics->instance, &deviceCount,
        devices), "Failed to list physical devices\n");
    
    // Initialize
    graphics->physicalDevice = VK_NULL_HANDLE;
    
    QueueFamilyIndices indices;
    SwapChainSupport details;
    for (uint32_t i = 0; i < deviceCount; ++i) {
        // Select first device that is suitable
        if (isDeviceSuitable(graphics, devices[i], &indices, &details)) {
            // Set physical device
            graphics->physicalDevice = devices[i];
            graphics->queueFamilies = indices;
            graphics->swapChainSupport = details;  // copies buffer pointers
            // Set #multi samples
            setMsaaSamples(graphics);
            break;
        }
        
        cleanupSwapChainSupport(&details);  // deallocate temporary buffers
    }
    
    // Cleanup
    free(devices);
    
    if (graphics->physicalDevice == VK_NULL_HANDLE) {
        fprintf(stderr, "Failed to find any suitable device (GPU)\n");
        exit(EXIT_FAILURE);
    }
}

static void initLogicalDevice(Graphics graphics)
{
    const uint32_t queueFamilies[] = {
        graphics->queueFamilies.graphicsFamily,
        graphics->queueFamilies.presentFamily
    };
    
    // Check if graphics and presentation queue are the same
    uint32_t uniqueQueueCount = 2;
    if (graphics->queueFamilies.graphicsFamily ==
        graphics->queueFamilies.presentFamily)
    {
        // Only 1 unique queue
        uniqueQueueCount = 1;
    }
    
    // Create device queue(s)
    VkDeviceQueueCreateInfo *queueCreateInfos = NULL;
    CHK_ALLOC(queueCreateInfos = malloc(uniqueQueueCount * sizeof(VkDeviceQueueCreateInfo)));
    
    const float queuePriority = 1.0f;  // maximum priority
    for (uint32_t i = 0; i < uniqueQueueCount; ++i) {
        VkDeviceQueueCreateInfo queueCreateInfo = {0};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilies[i];
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        
        queueCreateInfos[i] = queueCreateInfo;
    }
    
    // Specify which physical device features to use
    VkPhysicalDeviceFeatures deviceFeatures = {0};
    // Note: Ignore for now
    //deviceFeatures.samplerAnisotropy = VK_TRUE;
    
    VkDeviceCreateInfo deviceInfo = {0};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = uniqueQueueCount;
    deviceInfo.pQueueCreateInfos = queueCreateInfos;
    deviceInfo.pEnabledFeatures = &deviceFeatures;
    deviceInfo.enabledExtensionCount = 
        sizeof(REQ_DEVICE_EXTENSIONS) / sizeof(REQ_DEVICE_EXTENSIONS[0]);
    deviceInfo.ppEnabledExtensionNames = REQ_DEVICE_EXTENSIONS;
    
    if (ENABLE_VALIDATION_LAYERS) {
        // For backwards compatibility set also validation layers here
        deviceInfo.enabledLayerCount = 1;
        deviceInfo.ppEnabledLayerNames = &VALIDATION_LAYER_NAME;
    } else {
        deviceInfo.enabledLayerCount = 0;
    }
    
    CHK_VK_ERR(vkCreateDevice(graphics->physicalDevice, &deviceInfo,
        NULL, &graphics->device), "Failed to create logical device\n");
    
    // Cleanup
    free(queueCreateInfos);
    
    // Set device queue handles for graphics & compute / presentation queue
    vkGetDeviceQueue(graphics->device, graphics->queueFamilies.graphicsFamily,
        0, &graphics->graphicsQueue);
    vkGetDeviceQueue(graphics->device, graphics->queueFamilies.graphicsFamily,
        0, &graphics->computeQueue);
    vkGetDeviceQueue(graphics->device, graphics->queueFamilies.presentFamily,
        0, &graphics->presentQueue);
}

static VkImageView createImageView(VkImage image, VkFormat format, 
    VkImageAspectFlags aspectFlags, uint32_t mipLevels, VkDevice device)
{
    VkImageView imageView;
    
    VkImageViewCreateInfo viewInfo = {0};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    CHK_VK_ERR(vkCreateImageView(device, &viewInfo, NULL, 
        &imageView), "Failed to create image view\n");
    
    return imageView;
}

static uint32_t findMemoryType(uint32_t typeFilter, 
    VkMemoryPropertyFlags props, VkPhysicalDevice physicalDevice)
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props)
        {
            return i;
        }
    }
    
    fprintf(stderr, "Failed to find requested memory type\n");
    exit(EXIT_FAILURE);
}

static void createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
    VkSampleCountFlagBits nSamples, VkFormat format, VkImageTiling tiling,
    VkImageUsageFlags usage, VkMemoryPropertyFlags props, 
    VkImage *image, VkDeviceMemory *imageMemory, VkDevice device, 
    VkPhysicalDevice physicalDevice)
{
    VkImageCreateInfo imageInfo = {0};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;  // 3D extent
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = nSamples;  // #samples per pixel (multisampling)
    imageInfo.tiling = tiling;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    CHK_VK_ERR(vkCreateImage(device, &imageInfo, NULL, image),
        "Failed to create image\n");
    
    // Get memory requirements of image
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, *image, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        memRequirements.memoryTypeBits, props, physicalDevice
    );
    
    // Allocate image memory
    CHK_VK_ERR(vkAllocateMemory(device, &allocInfo, NULL, imageMemory),
        "Failed to allocate image memory\n");
    
    // Bind device memory to image
    CHK_VK_ERR(vkBindImageMemory(device, *image, *imageMemory, 0),
        "Failed to bind device memory to image\n");
}

static void createSwapChain(Graphics graphics)
{
    // Choose suitable surface format
    const SwapChainSupport support = graphics->swapChainSupport;
    VkSurfaceFormatKHR surfaceFormat = support.formats[0];  // default
    
    for (uint32_t i = 0; i < support.formatCount; ++i) {
        const VkSurfaceFormatKHR format = support.formats[i];
        // Ideally, want sRGB nonlinear color space and 8 bit wide BGRA channels
        if (format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
            format.format == VK_FORMAT_B8G8R8A8_SRGB)
        {
            surfaceFormat = format;
            break;
        }
    }
    
    // Choose suitable present mode
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;  // default, always available
    
    for (uint32_t i = 0; i < support.presentCount; ++i) {
        const VkPresentModeKHR present = support.presentModes[i];
        // Prefer mailbox/triple buffering presentation mode
        if (present == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = present;
            break;
        }
    }
    
    // Find swap extent
    VkExtent2D swapExtent = support.capabilities.currentExtent;
    // Special case -> surface size determined by extent of swapchain
    if (swapExtent.width == UINT32_MAX || swapExtent.height == UINT32_MAX) {
        int width, height;
        // Query window resolution in pixels
        glfwGetFramebufferSize(graphics->window, &width, &height);
        
        const VkExtent2D minExtent = support.capabilities.minImageExtent;
        const VkExtent2D maxExtent = support.capabilities.maxImageExtent;
        
        // Within min/max range, set swapchain extent to window resolution
        swapExtent.width = clampu32((uint32_t)width, 
            minExtent.width, maxExtent.width);
        swapExtent.height = clampu32((uint32_t)height,
            minExtent.height, maxExtent.height);
    }
    // Number of images in swapchain -> at least minimum number
    uint32_t imageCount = support.capabilities.minImageCount + 1;
    // maxImageCount == 0 -> no max.
    if (support.capabilities.maxImageCount > 0 && 
        imageCount > support.capabilities.maxImageCount)
    {
        imageCount = support.capabilities.maxImageCount;  // exceeded max.
    }
    
    // Create swapchain
    VkSwapchainCreateInfoKHR createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = graphics->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    const uint32_t queueFamilyIndices[] = {
        graphics->queueFamilies.graphicsFamily,
        graphics->queueFamilies.presentFamily
    };
    
    if (queueFamilyIndices[0] == queueFamilyIndices[1]) {
        // Same queue
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // optional, only relevant for VK_SHARING_MODE_CONCURRENT
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = NULL;
    } else {
        // Different queues
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;  // ignore color of pixels obscured by other windows
    createInfo.oldSwapchain = VK_NULL_HANDLE;  // ignore
    
    CHK_VK_ERR(vkCreateSwapchainKHR(graphics->device, &createInfo,
        NULL, &graphics->swapChainData.swapChain), "Failed to create swapchain\n");
        
    // Retrieve handles to swapchain images
    // Fetch final image count
    CHK_VK_ERR(vkGetSwapchainImagesKHR(graphics->device, 
        graphics->swapChainData.swapChain, &graphics->swapChainData.imageCount, NULL),
        "Failed to get swapchain image count\n");
    
    // Allocate swapchain image handles buffer
    CHK_ALLOC(graphics->swapChainData.images = 
        malloc(graphics->swapChainData.imageCount * sizeof(VkImage)));
    CHK_VK_ERR(vkGetSwapchainImagesKHR(graphics->device, 
        graphics->swapChainData.swapChain, &graphics->swapChainData.imageCount, 
        graphics->swapChainData.images), "Failed to fetch swapchain image handles\n");
    
    graphics->swapChainData.format = surfaceFormat.format;
    graphics->swapChainData.extent = swapExtent; 
    
    // Allocate swapchain image views buffer
    CHK_ALLOC(graphics->swapChainData.imageViews = 
        malloc(graphics->swapChainData.imageCount * sizeof(VkImageView)));
    // Initialize swapchain image views (for each swapchain image)
    for (uint32_t i = 0; i < graphics->swapChainData.imageCount; ++i) {
        graphics->swapChainData.imageViews[i] = createImageView(
            graphics->swapChainData.images[i], graphics->swapChainData.format,
            VK_IMAGE_ASPECT_COLOR_BIT, 1, graphics->device
        );
    }
    
    // Create color image for MSAA resolved to swapchain image
    createImage(graphics->swapChainData.extent.width, 
        graphics->swapChainData.extent.height, 1, graphics->msaaSamples, 
        graphics->swapChainData.format, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
        &graphics->swapChainData.colorResource.image, 
        &graphics->swapChainData.colorResource.memory,
        graphics->device, graphics->physicalDevice);
    
    graphics->swapChainData.colorResource.view = createImageView(
        graphics->swapChainData.colorResource.image,
        graphics->swapChainData.format,
        VK_IMAGE_ASPECT_COLOR_BIT, 1, graphics->device);
}

static void cleanupImage(VkDevice device, ImageResource resource)
{
    vkDestroyImageView(device, resource.view, NULL);
    vkDestroyImage(device, resource.image, NULL);
    vkFreeMemory(device, resource.memory, NULL);
}

static void cleanupSwapChain(Graphics graphics)
{
    // Destroy multisampled color image
    cleanupImage(graphics->device, graphics->swapChainData.colorResource);
    
    // Destroy all image views
    for (uint32_t i = 0; i < graphics->swapChainData.imageCount; ++i) {
        // Destroy framebuffers
        vkDestroyFramebuffer(graphics->device, 
            graphics->swapChainData.frameBuffers[i], NULL);
        // Destroy swapchain image views
        vkDestroyImageView(graphics->device, 
            graphics->swapChainData.imageViews[i], NULL);
    }
    // Cleanup swapchain object
    vkDestroySwapchainKHR(graphics->device, graphics->swapChainData.swapChain, NULL);
    // Deallocate buffers detailing swap chain support
    cleanupSwapChainSupport(&graphics->swapChainSupport);
    FREE_NULL(graphics->swapChainData.images);
    FREE_NULL(graphics->swapChainData.imageViews);
    FREE_NULL(graphics->swapChainData.frameBuffers);
    graphics->swapChainData.imageCount = 0;
}

static void createRenderPass(Graphics graphics)
{
    VkAttachmentDescription colorAttachment = {0};
    colorAttachment.format = graphics->swapChainData.format;
    colorAttachment.samples = graphics->msaaSamples;  // multisampled
    // Clear color framebuffer to black before next frame
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // Store results in framebuffer for rendering
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // No stencil buffer
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // Initial and final layout of color framebuffer
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference colorAttachmentRef = {0};
    colorAttachmentRef.attachment = 0;  // index in VkRenderPassCreateInfo.pAttachments
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    // Resolve multisampled color framebuffer to single sample
    VkAttachmentDescription colorAttachmentResolve = {0};
    colorAttachmentResolve.format = graphics->swapChainData.format;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    // Don't care about initial state of this framebuffer
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // Optimal layout for presenting contents to surface
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentReference colorAttachmentResolveRef = {0};
    colorAttachmentResolveRef.attachment = 1;  // index in VkRenderPassCreateInfo.pAttachments
    // Layout during subsequent rendering operations (subpass)
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    // Subsequent rendering operations applied to framebuffer
    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;
    
    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    // Operations to wait on before writing to color framebuffer
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    // Create render pass
    VkAttachmentDescription attachments[] = {
        colorAttachment,
        colorAttachmentResolve
    };
    
    VkRenderPassCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 2;  // see attachments array
    createInfo.pAttachments = attachments;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;
    
    CHK_VK_ERR(vkCreateRenderPass(graphics->device, &createInfo, NULL,
        &graphics->renderPass), "Failed to create render pass");
}

static VkShaderModule createShaderModule(VkDevice device, 
    const char *code, uint32_t codeSize)
{
    VkShaderModuleCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = codeSize;
    createInfo.pCode = (const uint32_t *)code;
    
    VkShaderModule shaderModule;
    CHK_VK_ERR(vkCreateShaderModule(device, &createInfo, NULL, 
        &shaderModule), "Failed to create shader module\n");
        
    return shaderModule;
}

// Depends on renderPass
static void createFramebuffers(Graphics graphics)
{
    // Allocate framebuffer handles
    CHK_ALLOC(graphics->swapChainData.frameBuffers = 
        malloc(graphics->swapChainData.imageCount * sizeof(VkFramebuffer)));
    
    for (uint32_t i = 0; i < graphics->swapChainData.imageCount; ++i) {
        // Note: Order is defined by VkAttachmentReference structs in
        //       createRenderPass()
        const VkImageView attachments[] = {
            graphics->swapChainData.colorResource.view,
            graphics->swapChainData.imageViews[i]
        }; 
        
        VkFramebufferCreateInfo createInfo = {0};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = graphics->renderPass;
        createInfo.attachmentCount = 2;
        createInfo.pAttachments = attachments;
        // Framebuffer dimensions
        createInfo.width = graphics->swapChainData.extent.width;
        createInfo.height = graphics->swapChainData.extent.height;
        createInfo.layers = 1;
        
        CHK_VK_ERR(vkCreateFramebuffer(graphics->device, &createInfo,
            NULL, &graphics->swapChainData.frameBuffers[i]),
            "Failed to create framebuffer(s)\n");
    }
}

static void recreateSwapChain(Graphics graphics)
{
    // Special case: Window is minimized -> width == 0 and height == 0
    int width = 0, height = 0;
    glfwGetFramebufferSize(graphics->window, &width, &height);
    while (width == 0 || height == 0) {
        // Sleep until events are ready to be processed
        glfwWaitEvents();
        glfwGetFramebufferSize(graphics->window, &width, &height);
    }
    vkDeviceWaitIdle(graphics->device);
    
    cleanupSwapChain(graphics);
    
    // Reset swapchain support
    fillSwapChainSupport(graphics, graphics->physicalDevice, 
        &graphics->swapChainSupport);
    createSwapChain(graphics);
    createFramebuffers(graphics);
}

static void createDescriptorResources(Graphics graphics)
{
    // - Create descriptor set layout
    VkDescriptorSetLayoutBinding layoutBindingVertex = {0};
    layoutBindingVertex.binding = 0;  // see binding in vertex shader
    layoutBindingVertex.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindingVertex.descriptorCount = 1;
    layoutBindingVertex.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfoVertex = {0};
    layoutInfoVertex.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfoVertex.bindingCount = 1;
    layoutInfoVertex.pBindings = &layoutBindingVertex;
    
    CHK_VK_ERR(vkCreateDescriptorSetLayout(graphics->device, &layoutInfoVertex,
        NULL, &graphics->vertexDescriptor.layout),
        "Failed to create descriptor set layout\n");
        
    VkDescriptorSetLayoutBinding layoutBindingsCompute[3] = {0};
    layoutBindingsCompute[0].binding = 0;  // see binding in compute shader
    layoutBindingsCompute[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBindingsCompute[0].descriptorCount = 1;
    layoutBindingsCompute[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    layoutBindingsCompute[1].binding = 1;  // see binding in compute shader
    layoutBindingsCompute[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindingsCompute[1].descriptorCount = 1;
    layoutBindingsCompute[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    layoutBindingsCompute[2].binding = 2;  // see binding in compute shader
    layoutBindingsCompute[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    layoutBindingsCompute[2].descriptorCount = 1;
    layoutBindingsCompute[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    VkDescriptorSetLayoutCreateInfo layoutInfoCompute = {0};
    layoutInfoCompute.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfoCompute.bindingCount = 3;
    layoutInfoCompute.pBindings = layoutBindingsCompute;
    
    CHK_VK_ERR(vkCreateDescriptorSetLayout(graphics->device, &layoutInfoCompute,
        NULL, &graphics->computeDescriptor.layout),
        "Failed to create descriptor set layout\n");
        
    // - Create descriptor pool
    VkDescriptorPoolSize poolSizes[2] = {0};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = (uint32_t)MAX_FRAMES_IN_FLIGHT * 2;
    
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = (uint32_t)MAX_FRAMES_IN_FLIGHT * 2;
    
    VkDescriptorPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = (uint32_t)MAX_FRAMES_IN_FLIGHT * 2;
    
    CHK_VK_ERR(vkCreateDescriptorPool(graphics->device, &poolInfo, NULL,
        &graphics->descriptorPool),
        "Failed to create descriptor pool\n");
    
    VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        layouts[i] = graphics->vertexDescriptor.layout;
    }
    // - Allocate descriptor set handles
    VkDescriptorSetAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = graphics->descriptorPool;
    allocInfo.descriptorSetCount = (uint32_t)MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts;
    
    CHK_VK_ERR(vkAllocateDescriptorSets(graphics->device, &allocInfo,
        graphics->vertexDescriptor.sets),
        "Failed to allocate graphics descriptor sets\n");
        
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        layouts[i] = graphics->computeDescriptor.layout;
    }
    VkDescriptorSetAllocateInfo allocInfoCompute = {0};
    allocInfoCompute.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfoCompute.descriptorPool = graphics->descriptorPool;
    allocInfoCompute.descriptorSetCount = (uint32_t)MAX_FRAMES_IN_FLIGHT;
    allocInfoCompute.pSetLayouts = layouts;
    
    CHK_VK_ERR(vkAllocateDescriptorSets(graphics->device, &allocInfoCompute,
        graphics->computeDescriptor.sets),
        "Failed to allocate compute descriptor sets\n");
}

static void cleanupDescriptorResources(Graphics graphics)
{
    // Note: Descriptor sets are automatically deallocated when descriptor
    //       pool is destroyed
    vkDestroyDescriptorPool(graphics->device, 
        graphics->descriptorPool, NULL);
    vkDestroyDescriptorSetLayout(graphics->device, 
        graphics->vertexDescriptor.layout, NULL);
    vkDestroyDescriptorSetLayout(graphics->device,
        graphics->computeDescriptor.layout, NULL);
}

static void createGraphicsPipeline(Graphics graphics)
{
    uint32_t vertShaderSize = 0, compShaderSize = 0, fragShaderSize = 0;
    char *vertShaderSource = readBinFile("shaders/bin/vert.spv", &vertShaderSize);
    char *compShaderSource = readBinFile("shaders/bin/comp.spv", &compShaderSize);
    char *fragShaderSource = readBinFile("shaders/bin/frag.spv", &fragShaderSize);
    
    // - Initialize shader modules
    VkShaderModule vertShaderModule = createShaderModule(
        graphics->device, vertShaderSource, vertShaderSize);
    VkShaderModule compShaderModule = createShaderModule(
        graphics->device, compShaderSource, compShaderSize);
    VkShaderModule fragShaderModule = createShaderModule(
        graphics->device, fragShaderSource, fragShaderSize);
    
    // - Assign shader modules to respective graphics pipeline stages
    VkPipelineShaderStageCreateInfo vertShaderInfo = {0};
    vertShaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderInfo.module = vertShaderModule;
    vertShaderInfo.pName = "main";  // entry point of shader code
    
    VkPipelineShaderStageCreateInfo compShaderInfo = {0};
    compShaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compShaderInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compShaderInfo.module = compShaderModule;
    compShaderInfo.pName = "main";  // entry point of shader code
    
    VkPipelineShaderStageCreateInfo fragShaderInfo = {0};
    fragShaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderInfo.module = fragShaderModule;
    fragShaderInfo.pName = "main";  // entry point of shader code
    
    const VkPipelineShaderStageCreateInfo shaderInfos[] = {
        vertShaderInfo,
        fragShaderInfo
    };
    
    // - Initialize vertex input binding and attribute descriptions
    VkVertexInputBindingDescription bindingDescriptions[2] = {0};
    bindingDescriptions[0].binding = 0;  // index (only 1 binding)
    bindingDescriptions[0].stride = sizeof(Vertex);
    // Attribute addressing using vertex index (as opposed to instance index)
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    bindingDescriptions[1].binding = 1;
    bindingDescriptions[1].stride = sizeof(Particle);
    // Attribute addressing using vertex index (as opposed to instance index)
    bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    
    // See Vertex and Particle structure in graphics.h 
    VkVertexInputAttributeDescription attributeDescriptions[4] = {0};
    // Vertex.pos (vec2 -> r32g32)
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);
    // Particle.col (vec3 -> r32g32b32)
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].binding = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Particle, color);
    // Particle.pos (vec2 -> r32g32)
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].binding = 1;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Particle, position);
    // Particle.orientation (float)
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].binding = 1;
    attributeDescriptions[3].format = VK_FORMAT_R32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Particle, orientation);
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {0};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 2;
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions;
    vertexInputInfo.vertexAttributeDescriptionCount = 4;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;
    
    VkPipelineInputAssemblyStateCreateInfo pipelineAssemblyInfo = {0};
    pipelineAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    // Geometry drawn from list of triangles
    pipelineAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Only applies to _STRIP topologies
    pipelineAssemblyInfo.primitiveRestartEnable = VK_FALSE;
    
    // - Dynamic viewport/scissors
    VkPipelineViewportStateCreateInfo viewportInfo = {0};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = 1;
    viewportInfo.scissorCount = 1;
    
    const VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    
    VkPipelineDynamicStateCreateInfo dynamicInfo = {0};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.dynamicStateCount = 2;
    dynamicInfo.pDynamicStates = dynamicStates;
    
    // - Rasterizer for converting vertex geometry into (polygonal) fragments
    VkPipelineRasterizationStateCreateInfo rasterizationInfo = {0};
    rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationInfo.depthClampEnable = VK_FALSE;
    rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    // Fill out interior of polygons
    rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationInfo.lineWidth = 1.0f;  // width of line segments
    rasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;  // Back face culling
    rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    
    // - Multisampling for anti-aliasing
    VkPipelineMultisampleStateCreateInfo multisampleInfo = {0};
    multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleInfo.sampleShadingEnable = VK_FALSE;
    multisampleInfo.rasterizationSamples = graphics->msaaSamples;
    
    // - Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
    // Color/alpha channels which are written to
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                          VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT |
                                          VK_COLOR_COMPONENT_A_BIT;
    // Overwrite framebuffer instead of blending it with previous result
    colorBlendAttachment.blendEnable = VK_TRUE;
    // C = \alpha * src + (1 - \alpha) * dst
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    
    VkPipelineColorBlendStateCreateInfo colorBlendInfo = {0};
    colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendInfo.logicOpEnable = VK_FALSE;  // bit-wise blending (disabled)
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &colorBlendAttachment;
    // (Optional if logicOp is disabled)
    colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendInfo.blendConstants[0] = 0.0f;
    colorBlendInfo.blendConstants[1] = 0.0f;
    colorBlendInfo.blendConstants[2] = 0.0f;
    colorBlendInfo.blendConstants[3] = 0.0f;
    
    // - Create depth/stencil state
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {0};
    depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilInfo.depthTestEnable = VK_FALSE;  // Note: disable for now
    depthStencilInfo.depthWriteEnable = VK_FALSE;  // Note: disable for now
    depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilInfo.minDepthBounds = 0.0f;
    depthStencilInfo.maxDepthBounds = 1.0f;
    // No stencil buffer
    
    // - Layout of pipeline
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &graphics->vertexDescriptor.layout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = NULL;
    
    CHK_VK_ERR(vkCreatePipelineLayout(graphics->device, 
        &pipelineLayoutInfo, NULL, &graphics->pipelineLayout), 
        "Failed to create pipeline layout\n");
    
    // - Finally, create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {0};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;  // fragment + vertex shader stages
    pipelineInfo.pStages = shaderInfos;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &pipelineAssemblyInfo;
    pipelineInfo.pViewportState = &viewportInfo;
    pipelineInfo.pRasterizationState = &rasterizationInfo;
    pipelineInfo.pMultisampleState = &multisampleInfo;
    pipelineInfo.pDepthStencilState = &depthStencilInfo;
    pipelineInfo.pColorBlendState = &colorBlendInfo;
    pipelineInfo.pDynamicState = &dynamicInfo;
    pipelineInfo.layout = graphics->pipelineLayout;
    pipelineInfo.renderPass = graphics->renderPass;
    pipelineInfo.subpass = 0;  // index
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE,
    pipelineInfo.basePipelineIndex = -1;
    
    // Create only 1 pipeline with pipeline caching disabled
    CHK_VK_ERR(vkCreateGraphicsPipelines(graphics->device, VK_NULL_HANDLE,
        1, &pipelineInfo, NULL, &graphics->graphicsPipeline), 
        "Failed to create graphics pipeline\n");
    
    // Create compute pipeline/layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfoCompute = {0};
    pipelineLayoutInfoCompute.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfoCompute.setLayoutCount = 1;
    pipelineLayoutInfoCompute.pSetLayouts = &graphics->computeDescriptor.layout;
    
    CHK_VK_ERR(vkCreatePipelineLayout(graphics->device, &pipelineLayoutInfoCompute,
        NULL, &graphics->computePipelineLayout),
        "Failed to create compute pipeline layout\n");
    
    VkComputePipelineCreateInfo pipelineInfoCompute = {0};
    pipelineInfoCompute.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfoCompute.layout = graphics->computePipelineLayout;
    pipelineInfoCompute.stage = compShaderInfo;
    
    CHK_VK_ERR(vkCreateComputePipelines(graphics->device, VK_NULL_HANDLE, 1,
        &pipelineInfoCompute, NULL, &graphics->computePipeline), "Failed to create compute pipeline\n");
    
    // - Cleanup
    vkDestroyShaderModule(graphics->device, vertShaderModule, NULL);
    vkDestroyShaderModule(graphics->device, compShaderModule, NULL);
    vkDestroyShaderModule(graphics->device, fragShaderModule, NULL);
    
    free(vertShaderSource);
    free(compShaderSource);
    free(fragShaderSource);
}

// Initialize command pool and buffers
static void createCommandResources(Graphics graphics)
{
    VkCommandPoolCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // Allow resetting command buffers allocated from this pool
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = graphics->queueFamilies.graphicsFamily;
    
    CHK_VK_ERR(vkCreateCommandPool(graphics->device, &createInfo,
        NULL, &graphics->commandPool), "Failed to create command pool\n");
    
    VkCommandBufferAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = graphics->commandPool;
    // Can be submitted to (graphics) queue for execution but not callable
    // from other command buffers
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)MAX_FRAMES_IN_FLIGHT;
    
    // Allocate command buffers for both graphics and compute queues
    // Note: Command buffers are freed when their command pool is destroyed
    CHK_VK_ERR(vkAllocateCommandBuffers(graphics->device, &allocInfo,
        graphics->commandBuffers), "Failed to allocate command buffers\n");
    
    CHK_VK_ERR(vkAllocateCommandBuffers(graphics->device, &allocInfo,
        graphics->computeCommandBuffers), 
        "Failed to allocate compute command buffers\n");
}

static void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice,
    VkDeviceSize size, VkBufferUsageFlags usage, 
    VkMemoryPropertyFlags properties, VkBuffer *buffer, 
    VkDeviceMemory *bufferMemory)
{
    VkBufferCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
    createInfo.usage = usage;
    // Only accessed by single queue family
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;  // optional
    createInfo.pQueueFamilyIndices = NULL;  // optional
    
    CHK_VK_ERR(vkCreateBuffer(device, &createInfo, NULL, buffer),
        "Failed to create buffer\n");
    
    // Query memory requirements of this buffer
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, *buffer, &memReq);
    
    VkMemoryAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits,
        properties, physicalDevice);
    
    // Allocate required memory for buffer
    CHK_VK_ERR(vkAllocateMemory(device, &allocInfo, NULL, bufferMemory),
        "Failed to allocate buffer memory\n");
    
    // Bind buffer memory to buffer object
    CHK_VK_ERR(vkBindBufferMemory(device, *buffer, *bufferMemory, 0),
        "Failed to bind memory to buffer\n");
}

static VkCommandBuffer beginSingleUseCommands(Graphics graphics)
{
    VkCommandBufferAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = graphics->commandPool;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer;
    CHK_VK_ERR(vkAllocateCommandBuffers(graphics->device, &allocInfo,
        &commandBuffer), "Failed to allocate single use command buffer\n");
    
    // Begin recording commands to single time command buffer
    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // Commands of command buffer are only submitted once 
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    CHK_VK_ERR(vkBeginCommandBuffer(commandBuffer, &beginInfo),
        "Failed to begin recording single use command buffer\n");
    
    return commandBuffer;
}

static void endSingleUseCommands(Graphics graphics, 
    VkCommandBuffer commandBuffer)
{
    // End recording commands
    CHK_VK_ERR(vkEndCommandBuffer(commandBuffer), 
        "Failed to end recording single use command buffer\n");
    
    VkSubmitInfo submitInfo = {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    // Note: Wait on fence instead of queue to be idle
    // Submit command buffer to graphics queue
    VkFenceCreateInfo fenceInfo = {0};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    
    VkFence fence;
    CHK_VK_ERR(vkCreateFence(graphics->device, &fenceInfo, NULL, &fence),
        "Failed to create fence for single use command buffer\n");
    
    CHK_VK_ERR(vkQueueSubmit(graphics->graphicsQueue, 1, &submitInfo,
        fence), "Failed to submit single use command buffer to graphics queue\n");
    // Wait until commands recorded to command buffer are executed
    // (timeout is maximum possible value)
    CHK_VK_ERR(vkWaitForFences(graphics->device, 1, &fence, VK_TRUE,
        UINT64_MAX), "Failed to wait for single use command buffer completion\n");
    
    // Cleanup
    vkDestroyFence(graphics->device, fence, NULL);
    vkFreeCommandBuffers(graphics->device, graphics->commandPool,
        1, &commandBuffer);
}

static void copyBuffer(Graphics graphics, VkBuffer src, VkBuffer dst, 
    VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = beginSingleUseCommands(graphics);
    
    VkBufferCopy copyRegion = {0};
    // No offsets
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    
    // Record command to copy buffer data
    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);
    
    // Cleanup
    endSingleUseCommands(graphics, commandBuffer);
}

static void createVertexBuffer(Graphics graphics, const Vertex *vertices,
    uint32_t nVertices)
{
    const VkDeviceSize bufferSize = nVertices * sizeof(vertices[0]);
    
    // Temporary staging buffer to move data from host (CPU) to device (GPU)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(graphics->device, graphics->physicalDevice, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer, &stagingBufferMemory);
    
    // Fill staging buffer with vertex data
    void *data = NULL;
    vkMapMemory(graphics->device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices, (size_t)bufferSize);
    vkUnmapMemory(graphics->device, stagingBufferMemory);
    
    // Create vertex buffer
    createBuffer(graphics->device, graphics->physicalDevice, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &graphics->vertexData.buffer, &graphics->vertexData.memory);
    
    // Copy contents of staging buffer
    copyBuffer(graphics, stagingBuffer, graphics->vertexData.buffer, bufferSize);
    
    // Cleanup staging buffer
    vkDestroyBuffer(graphics->device, stagingBuffer, NULL);
    vkFreeMemory(graphics->device, stagingBufferMemory, NULL);
}

static void createIndexBuffer(Graphics graphics, const uint16_t *indices,
    uint32_t nIndices)
{
    const VkDeviceSize bufferSize = nIndices * sizeof(indices[0]);
    
    // Temporary staging buffer to move data from host (CPU) to device (GPU)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(graphics->device, graphics->physicalDevice, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer, &stagingBufferMemory);
    
    // Fill staging buffer with vertex data
    void *data = NULL;
    vkMapMemory(graphics->device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices, (size_t)bufferSize);
    vkUnmapMemory(graphics->device, stagingBufferMemory);
    
    // Create vertex buffer
    createBuffer(graphics->device, graphics->physicalDevice, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &graphics->indexData.buffer, &graphics->indexData.memory);
    
    // Copy contents of staging buffer
    copyBuffer(graphics, stagingBuffer, graphics->indexData.buffer, bufferSize);
    
    // Cleanup staging buffer
    vkDestroyBuffer(graphics->device, stagingBuffer, NULL);
    vkFreeMemory(graphics->device, stagingBufferMemory, NULL);
}

static void createFlightBuffer(Graphics graphics, 
    FlightBufferResource *bufferResource, const DescriptorData *descriptor,
    VkDeviceSize bufferSize, uint32_t binding)
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        createBuffer(graphics->device, graphics->physicalDevice, bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &bufferResource->buffers[i], &bufferResource->memories[i]);
            
        // Obtain pointer to mapped memory range
        CHK_VK_ERR(vkMapMemory(graphics->device, bufferResource->memories[i],
            0, bufferSize, 0, &bufferResource->mapped[i]),
            "Failed to map uniform buffer memory\n");
        
        // Update descriptor sets to use uniform buffers
        VkDescriptorBufferInfo bufferInfo = {0};
        bufferInfo.buffer = bufferResource->buffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = bufferSize;  // VK_WHOLE_SIZE
        
        VkWriteDescriptorSet descriptorWrite = {0};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptor->sets[i];
        descriptorWrite.dstBinding = binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = NULL;  // unused
        descriptorWrite.pTexelBufferView = NULL;  // unused
        
        vkUpdateDescriptorSets(graphics->device, 1, &descriptorWrite, 0, NULL);
    }
}

static void createUniformBuffers(Graphics graphics)
{    
    VkDeviceSize bufferSize;
    // Create buffer for MVP matrices (binding 0 in vertex shader)
    bufferSize = sizeof(UniformBufferObject);
    createFlightBuffer(graphics, &graphics->mvpUniform, 
        &graphics->vertexDescriptor, bufferSize, 0);
    // Create buffer for deltaTime uniform (binding 0 in compute shader)
    bufferSize = sizeof(ParameterBufferObject);
    createFlightBuffer(graphics, &graphics->deltaTimeUniform, 
        &graphics->computeDescriptor, bufferSize, 0);
}

static void randomizeParticles(Particle particles[N_PARTICLES])
{
    // Note: Equi-area sampling (uniform)
    const float r = 0.5f * sqrtf((float)rand() / (float)RAND_MAX);
    const float phi = ((float)rand() / (float)RAND_MAX) * 2.0f * GLM_PI;
    const float randomCenterX = r * cosf(phi);
    const float randomCenterY = r * sinf(phi);
    
    for (uint32_t i = 0; i < N_PARTICLES; ++i) {
        // Random initial position inside concentric circle for ALL particles
        particles[i].position[0] = randomCenterX;
        particles[i].position[1] = randomCenterY;
        
        particles[i].orientation = ((float)rand() / (float)RAND_MAX) * 2.0f * GLM_PI;
        
        // Random direction
        const float theta = ((float)rand() / (float)RAND_MAX) * 2.0f * GLM_PI;
        // Random speed
        const float minSpeed = 1e-1f;
        const float maxSpeed = 1.0f;
        const float xi = ((float)rand() / (float)RAND_MAX);
        const float speed = (maxSpeed - minSpeed) * xi + minSpeed;
        particles[i].velocity[0] = speed * cosf(theta);
        particles[i].velocity[1] = speed * sinf(theta);
        
        particles[i].color[0] = (float)rand() / (float)RAND_MAX;
        particles[i].color[1] = (float)rand() / (float)RAND_MAX;
        particles[i].color[2] = (float)rand() / (float)RAND_MAX;
    }
}

static void createShaderStorage(Graphics graphics)
{
    // Initialize particle data
    Particle particles[N_PARTICLES] = {0};
    randomizeParticles(particles);
    
    const VkDeviceSize bufferSize = sizeof(particles);
    
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        createBuffer(graphics->device, graphics->physicalDevice, bufferSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &graphics->shaderStorage.buffers[i], &graphics->shaderStorage.memories[i]);
        
        // Map particle data to buffer
        vkMapMemory(graphics->device, graphics->shaderStorage.memories[i],
            0, bufferSize, 0, &graphics->shaderStorage.mapped[i]);
        // Copy particle data
        memcpy(graphics->shaderStorage.mapped[i], particles, (size_t)bufferSize);
    }
    
    // Update descriptor sets accordingly
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkWriteDescriptorSet descriptorWrites[2] = {0};
        
        VkDescriptorBufferInfo storageBufferInfoLastFrame = {0};
        storageBufferInfoLastFrame.buffer = 
            graphics->shaderStorage.buffers[(i + 1) % MAX_FRAMES_IN_FLIGHT];
        storageBufferInfoLastFrame.offset = 0;
        storageBufferInfoLastFrame.range = bufferSize;
        
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = graphics->computeDescriptor.sets[i];
        descriptorWrites[0].dstBinding = 1;  // see InParticleSSBO in comp shader
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &storageBufferInfoLastFrame;
        
        VkDescriptorBufferInfo storageBufferInfoCurrentFrame = {0};
        storageBufferInfoCurrentFrame.buffer = graphics->shaderStorage.buffers[i];
        storageBufferInfoCurrentFrame.offset = 0;
        storageBufferInfoCurrentFrame.range = bufferSize;
        
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = graphics->computeDescriptor.sets[i];
        descriptorWrites[1].dstBinding = 2;  // see OutParticleSSBO in comp shader
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &storageBufferInfoCurrentFrame;
        
        vkUpdateDescriptorSets(graphics->device, 2, descriptorWrites, 0, NULL);
    }
}

static void createSyncObjects(Graphics graphics)
{
    VkSemaphoreCreateInfo semaphoreInfo = {0};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo = {0};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // Initialize fence in signalled state for drawing first frame
    // (no previous frame to wait on)
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        CHK_VK_ERR(vkCreateSemaphore(graphics->device, &semaphoreInfo,
            NULL, &graphics->sync.imageAvailableSemaphores[i]),
            "Failed to create imageAvailableSemaphores\n");
        
        CHK_VK_ERR(vkCreateSemaphore(graphics->device, &semaphoreInfo,
            NULL, &graphics->sync.renderFinishedSemaphores[i]),
            "Failed to create renderFinishedSemaphores\n");
        
        CHK_VK_ERR(vkCreateSemaphore(graphics->device, &semaphoreInfo,
            NULL, &graphics->sync.computeFinishedSemaphores[i]),
            "Failed to create computeFinishedSemaphores\n");
            
        CHK_VK_ERR(vkCreateFence(graphics->device, &fenceInfo, NULL,
            &graphics->sync.inFlightFences[i]), 
            "Failed to create inFlightFences\n");
        
        CHK_VK_ERR(vkCreateFence(graphics->device, &fenceInfo, NULL,
            &graphics->sync.computeInFlightFences[i]),
            "Failed to create computeInFlightFences\n");
    }
}

static void cleanupSyncObjects(Graphics graphics)
{
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(graphics->device, 
            graphics->sync.imageAvailableSemaphores[i], NULL);
        vkDestroySemaphore(graphics->device, 
            graphics->sync.renderFinishedSemaphores[i], NULL);
        vkDestroySemaphore(graphics->device, 
            graphics->sync.computeFinishedSemaphores[i], NULL);
        vkDestroyFence(graphics->device, 
            graphics->sync.inFlightFences[i], NULL);
        vkDestroyFence(graphics->device, 
            graphics->sync.computeInFlightFences[i], NULL);
    }
}

static void recordCommandBuffer(Graphics graphics, 
    VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;  // optional
    beginInfo.pInheritanceInfo = NULL;  // optional
    
    CHK_VK_ERR(vkBeginCommandBuffer(commandBuffer, &beginInfo),
        "Failed to begin recording command buffer\n");
    
    // Start render pass
    VkRenderPassBeginInfo renderPassInfo = {0};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = graphics->renderPass;
    // Bind framebuffer associated with acquired swapchain image to draw to it
    renderPassInfo.framebuffer = graphics->swapChainData.frameBuffers[imageIndex];
    renderPassInfo.renderArea.offset = (VkOffset2D) {0, 0};
    renderPassInfo.renderArea.extent = graphics->swapChainData.extent;
    
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};  // black
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, 
        VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        graphics->graphicsPipeline);
    
    // Bind vertex buffers (star vertices + particle data)
    const VkBuffer vertexBuffers[] = {
        graphics->vertexData.buffer,
        graphics->shaderStorage.buffers[graphics->currentFrame]
    };
    const VkDeviceSize offsets[] = {0, 0};
    
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);
    
    // Bind index buffer
    vkCmdBindIndexBuffer(commandBuffer, graphics->indexData.buffer, 0, 
        VK_INDEX_TYPE_UINT16);
    
    // Dynamically set viewport and scissor state
    VkViewport viewport = {0};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) graphics->swapChainData.extent.width;
    viewport.height = (float) graphics->swapChainData.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    
    VkRect2D scissor = {0};
    scissor.offset = (VkOffset2D) {0, 0};
    scissor.extent = graphics->swapChainData.extent;
    
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    // Finally, issue draw command
    // TODO: Pass N_INDICES_STAR as function argument
    const uint32_t indexCount = N_INDICES_STAR;
    
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        graphics->pipelineLayout, 0, 1, 
        &graphics->vertexDescriptor.sets[graphics->currentFrame], 0, NULL);
    vkCmdDrawIndexed(commandBuffer, indexCount, N_PARTICLES, 0, 0, 0);
    
    vkCmdEndRenderPass(commandBuffer);
    
    // Done recording commands
    CHK_VK_ERR(vkEndCommandBuffer(commandBuffer),
        "Failed to end recording command buffer\n");
}

static void recordComputeCommandBuffer(Graphics graphics, 
    VkCommandBuffer commandBuffer)
{
    VkCommandBufferBeginInfo beginInfo = {0};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    CHK_VK_ERR(vkBeginCommandBuffer(commandBuffer, &beginInfo),
        "Failed to begin recording compute command buffer\n");
    
    // Bind the compute pipeline
    vkCmdBindPipeline(commandBuffer, 
        VK_PIPELINE_BIND_POINT_COMPUTE, graphics->computePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        graphics->computePipelineLayout, 0, 1, 
        &graphics->computeDescriptor.sets[graphics->currentFrame], 0, NULL);
    // Dispatch compute shader
    // Note: Using 256 threads per work group in x dimension 
    vkCmdDispatch(commandBuffer, N_PARTICLES / 256, 1, 1);
    
    CHK_VK_ERR(vkEndCommandBuffer(commandBuffer),
        "Failed to end recording compute command buffer\n");
}

static void updateUniformBuffer(Graphics graphics)
{   
    // Compute elapsed time since last frame
    const double now = glfwGetTime();
    const double deltaTime = now - graphics->lastFrameTime;
    if (now >= ANIMATION_RESET_TIME) {
        printf("Now: %.3f s\n", now);
        // Reset GLFW timer
        glfwSetTime(0.0);
        graphics->lastFrameTime = glfwGetTime();
        // Re-randomize particles
        Particle particles[N_PARTICLES] = {0};
        randomizeParticles(particles);
        // Copy data to storage buffer of currentFrame
        memcpy(graphics->shaderStorage.mapped[graphics->currentFrame],
            particles, sizeof(particles));
        
    } else {
        graphics->lastFrameTime = now;
    }
    
    ParameterBufferObject pbo = {0};
    pbo.deltaTime = (float)deltaTime;
    
    // Copy deltaTime to uniform entry
    memcpy(graphics->deltaTimeUniform.mapped[graphics->currentFrame], 
        &pbo, sizeof(pbo));
        
    UniformBufferObject ubo = {0};
    glm_mat4_identity(ubo.model);
    
    glm_lookat((vec3){0.0f, 0.0f, -2.0f}, (vec3){0.0f, 0.0f, 0.0f}, 
        (vec3){0.0f, -1.0f, 0.0f}, ubo.view);
    //glm_rotate_make(ubo.model, deltaTime * CGLM_PI / 4.0f, (vec3) {0.0f, 0.0f, 1.0f});
    //glm_ortho(0.0f, (float)graphics->swapChainData.extent.width, 0.0f,
    //    (float)graphics->swapChainData.extent.height, -1.0f, 1.0f, ubo.proj);
    glm_perspective(GLM_PI / 4.0f, (float)graphics->swapChainData.extent.width /
        (float)graphics->swapChainData.extent.height, -1.0f, 1.0f, ubo.proj);
    // Flip sign for consistency
    ubo.proj[1][1] *= -1.0f;
    
    // Copy ubo to mapped range in memory
    memcpy(graphics->mvpUniform.mapped[graphics->currentFrame], &ubo, sizeof(ubo));
}

static void initVulkan(Graphics graphics)
{    
    initInstance(graphics);
    // Initialize surface handle
    CHK_VK_ERR(glfwCreateWindowSurface(graphics->instance, graphics->window,
        NULL, &graphics->surface), "Failed to create GLFW window surface\n");
    // Select suitable physical device (GPU):
    // This initializes associated queue family indices and 
    // swap chain support details, as well as the #MSAA samples to use
    selectPhysicalDevice(graphics);
    // Initializes device, graphicsQueue and presentQueue
    initLogicalDevice(graphics);
    // Fills most of swapChainData struct
    createSwapChain(graphics);
    // Initialize render pass
    createRenderPass(graphics);
    // Initialize framebuffers contained in swapChainData
    createFramebuffers(graphics);
    // Initialize descriptorData
    createDescriptorResources(graphics);
    // Create graphicsPipeline along with its pipelineLayout
    createGraphicsPipeline(graphics);
    // Initialize command pool and command buffer objects
    createCommandResources(graphics);
    // Create a star
    const Star star = geomMakeStar(0.0f, 0.0f, 0.05f, 0.8f, 0.1f, 0.0f);
    // Initialize vertexData
    createVertexBuffer(graphics, star.vertices, N_VERTICES_STAR);
    // Initialize indexData
    createIndexBuffer(graphics, star.indices, N_INDICES_STAR);
    // Initialize uniform buffers
    createUniformBuffers(graphics);
    // Initialize shaderStorage
    createShaderStorage(graphics);
    // Initialize sync
    createSyncObjects(graphics);
}

Graphics initGraphics()
{
    Graphics graphics = NULL;
    // Allocate graphics handle and initialize to 0
    CHK_ALLOC(graphics = (Graphics) calloc(1, sizeof(GraphicsData)));
    
    // Initialize start time
    graphics->lastFrameTime = glfwGetTime();
    
    // Seed random engine using starting time
    srand(time(NULL));
    
    initWindow(graphics);
    initVulkan(graphics);
    
    return graphics;
}

static void draw(Graphics graphics)
{
    // - Compute submission
    CHK_VK_ERR(vkWaitForFences(graphics->device, 1,
        &graphics->sync.computeInFlightFences[graphics->currentFrame],
        VK_TRUE, UINT64_MAX),
        "Failed to wait for computeInFlightFence of current frame\n");
    
    // Update uniform buffers ahead of shader stages
    updateUniformBuffer(graphics);
    
    // Reset fence to unsignalled state
    vkResetFences(graphics->device, 1, 
        &graphics->sync.computeInFlightFences[graphics->currentFrame]);
    // Make sure command buffer is able to be recorded
    vkResetCommandBuffer(graphics->computeCommandBuffers[graphics->currentFrame], 0);
    // Record compute commands to computeComandBuffer
    recordComputeCommandBuffer(graphics, 
        graphics->computeCommandBuffers[graphics->currentFrame]);
    
    // Submit recorded command buffer to queue
    VkSubmitInfo submitInfo = {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = 
        &graphics->computeCommandBuffers[graphics->currentFrame];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = 
        &graphics->sync.computeFinishedSemaphores[graphics->currentFrame];
    
    CHK_VK_ERR(vkQueueSubmit(graphics->computeQueue, 1, &submitInfo,
        graphics->sync.computeInFlightFences[graphics->currentFrame]),
        "Failed to submit compute command buffer\n");
    
    // Note: currentFrame is initialized to 0 in initGraphics()
    // Wait for previous frame to finish
    CHK_VK_ERR(vkWaitForFences(graphics->device, 1, 
        &graphics->sync.inFlightFences[graphics->currentFrame], VK_TRUE,
        UINT64_MAX), "Failed to wait for inFlightFence of current frame\n");
    
    // Obtain index to next image in swapchain, as it becomes presentable
    uint32_t imageIndex = 0;
    VkResult result;
    result = vkAcquireNextImageKHR(graphics->device, 
        graphics->swapChainData.swapChain, UINT64_MAX, 
        graphics->sync.imageAvailableSemaphores[graphics->currentFrame],
        VK_NULL_HANDLE, &imageIndex);
    
    // Note: VK_SUBOPTIMAL_KHR means swapchain no longer matches surface
    //       properties, but CAN still be used to present image to surface
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Current swapchain is no longer adequate (e.g. due to resize)
        recreateSwapChain(graphics);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        fprintf(stderr, "Failed to acquire swapchain image\n");
        exit(EXIT_FAILURE);
    }
    
    // - Graphics submission
    // Reset fence to unsignalled state
    vkResetFences(graphics->device, 1, 
        &graphics->sync.inFlightFences[graphics->currentFrame]);
    // Make sure command buffer is able to be recorded
    vkResetCommandBuffer(graphics->commandBuffers[graphics->currentFrame], 0);
    // Record rendering commands to commandBuffer
    recordCommandBuffer(graphics, graphics->commandBuffers[graphics->currentFrame], imageIndex);
    
    // Wait on imageAvailable semaphore during COLOR_ATTACHMENT_OUTPUT_BIT
    // pipeline stage
    const VkSemaphore waitSemaphores[] = {
        graphics->sync.computeFinishedSemaphores[graphics->currentFrame],
        graphics->sync.imageAvailableSemaphores[graphics->currentFrame]
    };
    const VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    submitInfo = (VkSubmitInfo) {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    submitInfo.waitSemaphoreCount = 2;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &graphics->commandBuffers[graphics->currentFrame];
    
    // Signal renderFinished semaphore once commandBuffer has finished
    // execution
    const VkSemaphore signalSemaphores[] = {
        graphics->sync.renderFinishedSemaphores[graphics->currentFrame]
    }; 
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    // After MAX_FRAMES_IN_FLIGHT, CPU waits for command buffer to finish execution
    // due to inFlightFence
    CHK_VK_ERR(vkQueueSubmit(graphics->graphicsQueue, 1, &submitInfo,
        graphics->sync.inFlightFences[graphics->currentFrame]),
        "Failed to submit draw command buffer\n");
    
    // Presentation of image to surface
    VkPresentInfoKHR presentInfo = {0};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    
    // Specify swapchain(s) to present images to
    const VkSwapchainKHR swapchains[] = {
        graphics->swapChainData.swapChain
    };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = NULL;  // optional error handling for individual swapchains
    
    result = vkQueuePresentKHR(graphics->presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || 
        result == VK_SUBOPTIMAL_KHR ||
        graphics->framebufferResized)
    {
        graphics->framebufferResized = VK_FALSE;
        recreateSwapChain(graphics);
    } else if (result != VK_SUCCESS) {
        fprintf(stderr, "Failed to present swapchain image\n");
        exit(EXIT_FAILURE);
    }
    // Move to next frame
    graphics->currentFrame = (graphics->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// Main rendering loop
void renderLoop(Graphics graphics)
{
    assert(graphics && "Expected non-NULL graphics handle");
    
    while (!glfwWindowShouldClose(graphics->window)) {
        glfwPollEvents();
        draw(graphics);  // draw next frame to surface
    }
    // Wait for device to finish all operations before exiting (cleanup)
    vkDeviceWaitIdle(graphics->device);
}

void cleanupGraphics(Graphics graphics)
{
    assert(graphics && "Expected non-NULL graphics handle");
    
    // Cleanup swapchain
    cleanupSwapChain(graphics);
    
    // Cleanup vertex buffer
    vkDestroyBuffer(graphics->device, graphics->vertexData.buffer, NULL);
    vkFreeMemory(graphics->device, graphics->vertexData.memory, NULL);
    // Cleanup index buffer
    vkDestroyBuffer(graphics->device, graphics->indexData.buffer, NULL);
    vkFreeMemory(graphics->device, graphics->indexData.memory, NULL);
    // Cleanup uniform buffers & shader storage buffers
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroyBuffer(graphics->device, graphics->mvpUniform.buffers[i], NULL);
        vkFreeMemory(graphics->device, graphics->mvpUniform.memories[i], NULL);
        
        vkDestroyBuffer(graphics->device, graphics->deltaTimeUniform.buffers[i], NULL);
        vkFreeMemory(graphics->device, graphics->deltaTimeUniform.memories[i], NULL);
        
        vkDestroyBuffer(graphics->device, graphics->shaderStorage.buffers[i], NULL);
        vkFreeMemory(graphics->device, graphics->shaderStorage.memories[i], NULL);
    }
    // Cleanup synchronization objects
    cleanupSyncObjects(graphics);
    
    // Cleanup command pool
    vkDestroyCommandPool(graphics->device, graphics->commandPool, NULL);
    
    // Destroy graphics pipeline
    vkDestroyPipeline(graphics->device, graphics->graphicsPipeline, NULL);
    vkDestroyPipelineLayout(graphics->device, graphics->pipelineLayout, NULL);
    // Destroy compute pipeline
    vkDestroyPipeline(graphics->device, graphics->computePipeline, NULL);
    vkDestroyPipelineLayout(graphics->device, graphics->computePipelineLayout, NULL);
    
    // Cleanup descriptor set resources
    cleanupDescriptorResources(graphics);
    
    // Destroy render pass object
    vkDestroyRenderPass(graphics->device, graphics->renderPass, NULL);
    // Destroy logical device (wait until device is idle first)
    vkDestroyDevice(graphics->device, NULL);
    
    if (ENABLE_VALIDATION_LAYERS) {
        destroyDebugUtilsMessengerEXT(graphics->instance, 
            graphics->debugMessenger, NULL);
    }
    vkDestroySurfaceKHR(graphics->instance, graphics->surface, NULL);
    vkDestroyInstance(graphics->instance, NULL);
    
    glfwDestroyWindow(graphics->window);
    glfwTerminate();
    
    free(graphics);
}
