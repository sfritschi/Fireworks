#ifndef GRAPHICS_H
#define GRAPHICS_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <sys/time.h>  // struct timeval, gettimeofday
#include <errno.h>

#include "geometry.h"

#define WINDOW_WIDTH 1400
#define WINDOW_HEIGHT 1000
#define MAX_FRAMES_IN_FLIGHT 2
#define ANIMATION_RESET_TIME 10.0  // 10 seconds

#ifdef NDEBUG
#define ENABLE_VALIDATION_LAYERS VK_FALSE
#else
#define ENABLE_VALIDATION_LAYERS VK_TRUE
#endif

#define CHK_VK_ERR(vk, msg) do {\
    if ((vk) != VK_SUCCESS) {\
        fprintf(stderr, (msg));\
        exit(EXIT_FAILURE);\
    }\
} while(0)

#define CHK_ALLOC(ptr) do {\
    if (!(ptr)) {\
        fprintf(stderr, "Out of memory\n");\
        exit(EXIT_FAILURE);\
    }\
} while(0)

#define FREE_NULL(ptr) do {\
    if ((ptr)) {\
        free(ptr);\
        ptr = NULL;\
    }\
} while(0)

typedef struct QueueFamilyIndices {
    uint32_t graphicsFamily;  // queue family index of graphics queue
    uint32_t presentFamily;   // queue family index of present queue
} QueueFamilyIndices;

typedef struct SwapChainSupport {
    VkSurfaceCapabilitiesKHR capabilities;  // min/max #images and dimensions
    VkSurfaceFormatKHR *formats;            // pixel format, color spaces
    uint32_t formatCount;                   // #elements in formats
    VkPresentModeKHR *presentModes;         // supported presentation modes
    uint32_t presentCount;                  // #elements in presentModes
} SwapChainSupport;

typedef struct ImageResource {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
} ImageResource;

typedef struct SwapChainData {
    VkSwapchainKHR swapChain;
    VkImage *images;              // imageCount many image handles
    VkImageView *imageViews;      // imageCount many image views
    VkFramebuffer *frameBuffers;  // imageCount many framebuffers
    uint32_t imageCount;
    VkFormat format;
    VkExtent2D extent;
    // Multisampling color buffer resolved to swapchain image
    ImageResource colorResource;
} SwapChainData;

typedef struct DescriptorData {
    VkDescriptorSet sets[MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSetLayout layout;
} DescriptorData;

typedef struct BufferResource {
    VkBuffer buffer;        // handle to buffer object
    VkDeviceMemory memory;  // handle of device memory associated with buffer
} BufferResource;

// Buffer resource for every frame in flight
typedef struct FlightBufferResource {
    VkBuffer buffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory memories[MAX_FRAMES_IN_FLIGHT];
    void *mapped[MAX_FRAMES_IN_FLIGHT];  // mapped memory regions    
} FlightBufferResource;

typedef struct SyncObjects {
    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore computeFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];
    VkFence computeInFlightFences[MAX_FRAMES_IN_FLIGHT];
} SyncObjects;

typedef struct GraphicsData {
    GLFWwindow *window;     // window handle
    VkInstance instance;    // instance storing application state
    VkPhysicalDevice physicalDevice;      // implementation of Vulkan
    VkDevice device;        // logical device (including state information)
    VkSurfaceKHR surface;   // surface to render graphics to
    VkQueue graphicsQueue;  // graphics queue handle
    VkQueue computeQueue;   // compute queue handle
    VkQueue presentQueue;   // presentation queue handle
    VkRenderPass renderPass;  // rendering operations
    VkPipeline graphicsPipeline;
    VkPipeline computePipeline;
    VkPipelineLayout pipelineLayout;
    VkPipelineLayout computePipelineLayout;
    VkCommandPool commandPool;  // pool for allocating command buffers
    VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];
    VkCommandBuffer computeCommandBuffers[MAX_FRAMES_IN_FLIGHT];
    VkSampleCountFlagBits msaaSamples;  // #multisampling sample count
    uint32_t currentFrame;  // index of current frame being drawn
    VkBool32 framebufferResized;
    QueueFamilyIndices queueFamilies;
    SwapChainSupport swapChainSupport;
    SwapChainData swapChainData;
    BufferResource vertexData;
    BufferResource indexData;
    VkDescriptorPool descriptorPool;
    DescriptorData vertexDescriptor;
    DescriptorData computeDescriptor;
    FlightBufferResource mvpUniform;
    FlightBufferResource deltaTimeUniform;
    FlightBufferResource shaderStorage; 
    SyncObjects sync;
    double lastFrameTime;  // Elapsed time in seconds since last frame
    VkDebugUtilsMessengerEXT debugMessenger;
} GraphicsData;

typedef GraphicsData * Graphics;

Graphics initGraphics();
void renderLoop(Graphics graphics);
void cleanupGraphics(Graphics graphics);

#endif /* GRAPHICS_H */
