#include "../include/VulkanRenderer3D.hpp"
#include "../include/Window.hpp"

#ifdef __linux__
#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib.h>
#if JWAROL_HAS_WAYLAND
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan_wayland.h>
#endif
#endif

#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
#include <algorithm>
#include <unordered_map>

#include "spirv_data.h"

struct UBO {
    float model[16];
    float view[16];
    float proj[16];
    float lightDir[4];
    float color[4];
};

struct VulkanMeshBuffers {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;
    uint32_t indexCount = 0;
};

struct VulkanRenderer3DInternal {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t graphicsFamily = 0;
    uint32_t presentFamily = 0;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapExtent = {};
    std::vector<VkImage> swapImages;
    std::vector<VkImageView> swapImageViews;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;

    VkDescriptorSetLayout descLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorPool descPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descSets;

    VkCommandPool cmdPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> cmdBuffers;

    std::vector<VkSemaphore> imageAvailable;
    std::vector<VkSemaphore> renderFinished;
    std::vector<VkFence> fences;
    uint32_t currentFrame = 0;
    uint32_t currentImageIndex = 0;
    static const int MAX_FRAMES = 2;

    VkBuffer uboBuffer = VK_NULL_HANDLE;
    VkDeviceMemory uboMemory = VK_NULL_HANDLE;
    void* uboMapped = nullptr;

    float screenW = 800, screenH = 600;
    bool initialized = false;

    Vec3 lightDir = {0.5f, -1.0f, 0.3f};
    float ambient = 0.15f;

    AuraWindow* window = nullptr;

    std::unordered_map<const Mesh3D*, VulkanMeshBuffers> meshCache;
};

static uint32_t findMemType(VkPhysicalDevice pd, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(pd, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return 0;
}

static void createBuffer(VulkanRenderer3DInternal& v, VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags props, VkBuffer& buf, VkDeviceMemory& mem) {
    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = size;
    bufInfo.usage = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(v.device, &bufInfo, nullptr, &buf);

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(v.device, buf, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemType(v.physicalDevice, memReq.memoryTypeBits, props);

    vkAllocateMemory(v.device, &allocInfo, nullptr, &mem);
    vkBindBufferMemory(v.device, buf, mem, 0);
}

static void copyBuffer(VulkanRenderer3DInternal& v, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = v.cmdPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(v.device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferCopy copyRegion = {};
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &copyRegion);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(v.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(v.graphicsQueue);

    vkFreeCommandBuffers(v.device, v.cmdPool, 1, &cmd);
}

static bool createInstance(VulkanRenderer3DInternal& v) {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "jwarol2";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "jwarol2";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
    };

#if JWAROL_HAS_WAYLAND
    if (v.window->isWayland()) {
        extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
    } else {
        extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
    }
#else
    extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = (uint32_t)extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = 0;

    return vkCreateInstance(&createInfo, nullptr, &v.instance) == VK_SUCCESS;
}

static bool createSurface(VulkanRenderer3DInternal& v) {
#if JWAROL_HAS_WAYLAND
    if (v.window->isWayland()) {
        VkWaylandSurfaceCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        createInfo.display = v.window->getWlDisplay();
        createInfo.surface = v.window->getWlSurface();
        return vkCreateWaylandSurfaceKHR(v.instance, &createInfo, nullptr, &v.surface) == VK_SUCCESS;
    }
#endif
    VkXlibSurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    createInfo.dpy = v.window->getDisplay();
    createInfo.window = v.window->getX11Window();
    return vkCreateXlibSurfaceKHR(v.instance, &createInfo, nullptr, &v.surface) == VK_SUCCESS;
}

static bool pickPhysicalDevice(VulkanRenderer3DInternal& v) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(v.instance, &count, nullptr);
    if (count == 0) return false;

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(v.instance, &count, devices.data());

    for (auto& pd : devices) {
        uint32_t qfCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfCount, nullptr);
        std::vector<VkQueueFamilyProperties> qfProps(qfCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfCount, qfProps.data());

        int gfxIdx = -1, presIdx = -1;
        for (uint32_t i = 0; i < qfCount; i++) {
            if (qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) gfxIdx = i;
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, v.surface, &presentSupport);
            if (presentSupport) presIdx = i;
            if (gfxIdx >= 0 && presIdx >= 0) break;
        }

        if (gfxIdx >= 0 && presIdx >= 0) {
            v.physicalDevice = pd;
            v.graphicsFamily = gfxIdx;
            v.presentFamily = presIdx;
            return true;
        }
    }
    return false;
}

static bool createLogicalDevice(VulkanRenderer3DInternal& v) {
    uint32_t uniqueFamilies[] = { v.graphicsFamily, v.presentFamily };
    uint32_t uniqueCount = (v.graphicsFamily != v.presentFamily) ? 2 : 1;

    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos(uniqueCount);
    for (uint32_t i = 0; i < uniqueCount; i++) {
        queueInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[i].queueFamilyIndex = uniqueFamilies[i];
        queueInfos[i].queueCount = 1;
        queueInfos[i].pQueuePriorities = &queuePriority;
    }

    const char* extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkPhysicalDeviceFeatures features = {};

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = uniqueCount;
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.pEnabledFeatures = &features;

    if (vkCreateDevice(v.physicalDevice, &createInfo, nullptr, &v.device) != VK_SUCCESS)
        return false;

    vkGetDeviceQueue(v.device, v.graphicsFamily, 0, &v.graphicsQueue);
    vkGetDeviceQueue(v.device, v.presentFamily, 0, &v.presentQueue);
    return true;
}

static bool createSwapchain(VulkanRenderer3DInternal& v) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(v.physicalDevice, v.surface, &caps);

    uint32_t fmtCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(v.physicalDevice, v.surface, &fmtCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(v.physicalDevice, v.surface, &fmtCount, formats.data());

    VkSurfaceFormatKHR chosenFmt = formats[0];
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFmt = f;
            break;
        }
    }

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == 0xFFFFFFFF) {
        extent.width = (uint32_t)v.screenW;
        extent.height = (uint32_t)v.screenH;
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = v.surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = chosenFmt.format;
    createInfo.imageColorSpace = chosenFmt.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = { v.graphicsFamily, v.presentFamily };
    if (v.graphicsFamily != v.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(v.device, &createInfo, nullptr, &v.swapchain) != VK_SUCCESS)
        return false;

    v.swapFormat = chosenFmt.format;
    v.swapExtent = extent;

    uint32_t scImgCount;
    vkGetSwapchainImagesKHR(v.device, v.swapchain, &scImgCount, nullptr);
    v.swapImages.resize(scImgCount);
    vkGetSwapchainImagesKHR(v.device, v.swapchain, &scImgCount, v.swapImages.data());

    v.swapImageViews.resize(scImgCount);
    for (uint32_t i = 0; i < scImgCount; i++) {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = v.swapImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = v.swapFormat;
        viewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(v.device, &viewInfo, nullptr, &v.swapImageViews[i]);
    }

    return true;
}

static bool createRenderPass(VulkanRenderer3DInternal& v) {
    VkAttachmentDescription colorAttach = {};
    colorAttach.format = v.swapFormat;
    colorAttach.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttach.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttach.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dep = {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &colorAttach;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dep;

    return vkCreateRenderPass(v.device, &rpInfo, nullptr, &v.renderPass) == VK_SUCCESS;
}

static bool createFramebuffers(VulkanRenderer3DInternal& v) {
    v.framebuffers.resize(v.swapImageViews.size());
    for (size_t i = 0; i < v.swapImageViews.size(); i++) {
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = v.renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &v.swapImageViews[i];
        fbInfo.width = v.swapExtent.width;
        fbInfo.height = v.swapExtent.height;
        fbInfo.layers = 1;
        vkCreateFramebuffer(v.device, &fbInfo, nullptr, &v.framebuffers[i]);
    }
    return true;
}

static VkShaderModule createShaderModule(VulkanRenderer3DInternal& v, const uint32_t* code, size_t size) {
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = code;
    VkShaderModule module;
    vkCreateShaderModule(v.device, &createInfo, nullptr, &module);
    return module;
}

static bool createGraphicsPipeline(VulkanRenderer3DInternal& v) {
    VkShaderModule vertModule = createShaderModule(v, vertSPIRV, vertSPIRV_len);
    VkShaderModule fragModule = createShaderModule(v, fragSPIRV, fragSPIRV_len);

    VkPipelineShaderStageCreateInfo vertStage = {};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage = {};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

    VkVertexInputBindingDescription bindingDesc = {};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDescs[3] = {};
    attrDescs[0].binding = 0;
    attrDescs[0].location = 0;
    attrDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[0].offset = offsetof(Vertex, position);
    attrDescs[1].binding = 0;
    attrDescs[1].location = 1;
    attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[1].offset = offsetof(Vertex, normal);
    attrDescs[2].binding = 0;
    attrDescs[2].location = 2;
    attrDescs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[2].offset = offsetof(Vertex, uv);

    VkPipelineVertexInputStateCreateInfo vertInput = {};
    vertInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertInput.vertexBindingDescriptionCount = 1;
    vertInput.pVertexBindingDescriptions = &bindingDesc;
    vertInput.vertexAttributeDescriptionCount = 3;
    vertInput.pVertexAttributeDescriptions = attrDescs;

    VkPipelineInputAssemblyStateCreateInfo inputAsm = {};
    inputAsm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {};
    viewport.x = 0; viewport.y = 0;
    viewport.width = (float)v.swapExtent.width;
    viewport.height = (float)v.swapExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = v.swapExtent;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttach = {};
    colorBlendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                       VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttach.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttach;

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &v.descLayout;

    vkCreatePipelineLayout(v.device, &layoutInfo, nullptr, &v.pipelineLayout);

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertInput;
    pipelineInfo.pInputAssemblyState = &inputAsm;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = v.pipelineLayout;
    pipelineInfo.renderPass = v.renderPass;
    pipelineInfo.subpass = 0;

    VkResult result = vkCreateGraphicsPipelines(v.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &v.pipeline);

    vkDestroyShaderModule(v.device, vertModule, nullptr);
    vkDestroyShaderModule(v.device, fragModule, nullptr);

    return result == VK_SUCCESS;
}

static bool createDescriptorResources(VulkanRenderer3DInternal& v) {
    VkDescriptorSetLayoutBinding uboBinding = {};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboBinding;

    if (vkCreateDescriptorSetLayout(v.device, &layoutInfo, nullptr, &v.descLayout) != VK_SUCCESS)
        return false;

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = (uint32_t)v.swapImages.size();

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = (uint32_t)v.swapImages.size();

    if (vkCreateDescriptorPool(v.device, &poolInfo, nullptr, &v.descPool) != VK_SUCCESS)
        return false;

    VkDeviceSize uboSize = sizeof(UBO);
    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = uboSize;
    bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(v.device, &bufInfo, nullptr, &v.uboBuffer);

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(v.device, v.uboBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemType(v.physicalDevice, memReq.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(v.device, &allocInfo, nullptr, &v.uboMemory);
    vkBindBufferMemory(v.device, v.uboBuffer, v.uboMemory, 0);
    vkMapMemory(v.device, v.uboMemory, 0, uboSize, 0, &v.uboMapped);

    std::vector<VkDescriptorSetLayout> layouts(v.swapImages.size(), v.descLayout);
    VkDescriptorSetAllocateInfo dsInfo = {};
    dsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsInfo.descriptorPool = v.descPool;
    dsInfo.descriptorSetCount = (uint32_t)v.swapImages.size();
    dsInfo.pSetLayouts = layouts.data();

    v.descSets.resize(v.swapImages.size());
    vkAllocateDescriptorSets(v.device, &dsInfo, v.descSets.data());

    for (size_t i = 0; i < v.swapImages.size(); i++) {
        VkDescriptorBufferInfo bufInfo = {};
        bufInfo.buffer = v.uboBuffer;
        bufInfo.offset = 0;
        bufInfo.range = sizeof(UBO);

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = v.descSets[i];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &bufInfo;

        vkUpdateDescriptorSets(v.device, 1, &write, 0, nullptr);
    }

    return true;
}

static bool createCommandPoolAndBuffers(VulkanRenderer3DInternal& v) {
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = v.graphicsFamily;

    if (vkCreateCommandPool(v.device, &poolInfo, nullptr, &v.cmdPool) != VK_SUCCESS)
        return false;

    v.cmdBuffers.resize(v.swapImages.size());
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = v.cmdPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)v.cmdBuffers.size();

    return vkAllocateCommandBuffers(v.device, &allocInfo, v.cmdBuffers.data()) == VK_SUCCESS;
}

static bool createSyncObjects(VulkanRenderer3DInternal& v) {
    v.imageAvailable.resize(VulkanRenderer3DInternal::MAX_FRAMES);
    v.renderFinished.resize(VulkanRenderer3DInternal::MAX_FRAMES);
    v.fences.resize(VulkanRenderer3DInternal::MAX_FRAMES);

    VkSemaphoreCreateInfo semInfo = {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < VulkanRenderer3DInternal::MAX_FRAMES; i++) {
        if (vkCreateSemaphore(v.device, &semInfo, nullptr, &v.imageAvailable[i]) != VK_SUCCESS) return false;
        if (vkCreateSemaphore(v.device, &semInfo, nullptr, &v.renderFinished[i]) != VK_SUCCESS) return false;
        if (vkCreateFence(v.device, &fenceInfo, nullptr, &v.fences[i]) != VK_SUCCESS) return false;
    }
    return true;
}

static void cleanupSwapchain(VulkanRenderer3DInternal& v) {
    for (auto fb : v.framebuffers) vkDestroyFramebuffer(v.device, fb, nullptr);
    for (auto iv : v.swapImageViews) vkDestroyImageView(v.device, iv, nullptr);
    if (v.swapchain) vkDestroySwapchainKHR(v.device, v.swapchain, nullptr);
    v.framebuffers.clear();
    v.swapImageViews.clear();
    v.swapImages.clear();
    v.swapchain = VK_NULL_HANDLE;
}

static bool recreateSwapchain(VulkanRenderer3DInternal& v) {
    vkDeviceWaitIdle(v.device);
    cleanupSwapchain(v);
    return createSwapchain(v) && createFramebuffers(v);
}

// ============ Public API ============

VulkanRenderer3D::VulkanRenderer3D(float width, float height, AuraWindow& win) {
    impl = new VulkanRenderer3DInternal();
    impl->screenW = width;
    impl->screenH = height;
    impl->window = &win;

    if (!createInstance(*impl)) { fprintf(stderr, "Vulkan: failed to create instance\n"); return; }
    if (!createSurface(*impl)) { fprintf(stderr, "Vulkan: failed to create surface\n"); return; }
    if (!pickPhysicalDevice(*impl)) { fprintf(stderr, "Vulkan: no suitable GPU\n"); return; }
    if (!createLogicalDevice(*impl)) { fprintf(stderr, "Vulkan: failed to create device\n"); return; }
    if (!createSwapchain(*impl)) { fprintf(stderr, "Vulkan: failed to create swapchain\n"); return; }
    if (!createRenderPass(*impl)) { fprintf(stderr, "Vulkan: failed to create render pass\n"); return; }
    if (!createFramebuffers(*impl)) { fprintf(stderr, "Vulkan: failed to create framebuffers\n"); return; }
    if (!createDescriptorResources(*impl)) { fprintf(stderr, "Vulkan: failed to create descriptors\n"); return; }
    if (!createGraphicsPipeline(*impl)) { fprintf(stderr, "Vulkan: failed to create pipeline\n"); return; }
    if (!createCommandPoolAndBuffers(*impl)) { fprintf(stderr, "Vulkan: failed to create command buffers\n"); return; }
    if (!createSyncObjects(*impl)) { fprintf(stderr, "Vulkan: failed to create sync objects\n"); return; }

    impl->initialized = true;
    fprintf(stderr, "VulkanRenderer3D: initialized successfully\n");
}

VulkanRenderer3D::~VulkanRenderer3D() {
    if (!impl) return;
    vkDeviceWaitIdle(impl->device);

    for (auto& [meshPtr, mb] : impl->meshCache) {
        if (mb.vertexBuffer) vkDestroyBuffer(impl->device, mb.vertexBuffer, nullptr);
        if (mb.vertexMemory) vkFreeMemory(impl->device, mb.vertexMemory, nullptr);
        if (mb.indexBuffer) vkDestroyBuffer(impl->device, mb.indexBuffer, nullptr);
        if (mb.indexMemory) vkFreeMemory(impl->device, mb.indexMemory, nullptr);
    }
    impl->meshCache.clear();

    if (impl->uboMapped) { vkUnmapMemory(impl->device, impl->uboMemory); impl->uboMapped = nullptr; }
    if (impl->uboBuffer) vkDestroyBuffer(impl->device, impl->uboBuffer, nullptr);
    if (impl->uboMemory) vkFreeMemory(impl->device, impl->uboMemory, nullptr);

    for (int i = 0; i < VulkanRenderer3DInternal::MAX_FRAMES; i++) {
        vkDestroySemaphore(impl->device, impl->imageAvailable[i], nullptr);
        vkDestroySemaphore(impl->device, impl->renderFinished[i], nullptr);
        vkDestroyFence(impl->device, impl->fences[i], nullptr);
    }

    if (impl->cmdPool) vkDestroyCommandPool(impl->device, impl->cmdPool, nullptr);
    if (impl->pipeline) vkDestroyPipeline(impl->device, impl->pipeline, nullptr);
    if (impl->pipelineLayout) vkDestroyPipelineLayout(impl->device, impl->pipelineLayout, nullptr);
    if (impl->descPool) vkDestroyDescriptorPool(impl->device, impl->descPool, nullptr);
    if (impl->descLayout) vkDestroyDescriptorSetLayout(impl->device, impl->descLayout, nullptr);
    if (impl->renderPass) vkDestroyRenderPass(impl->device, impl->renderPass, nullptr);

    cleanupSwapchain(*impl);

    if (impl->device) vkDestroyDevice(impl->device, nullptr);
    if (impl->surface) vkDestroySurfaceKHR(impl->instance, impl->surface, nullptr);
    if (impl->instance) vkDestroyInstance(impl->instance, nullptr);

    delete impl;
}

void VulkanRenderer3D::beginFrame(Camera3D& camera) {
    if (!impl || !impl->initialized) return;

    vkWaitForFences(impl->device, 1, &impl->fences[impl->currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(impl->device, impl->swapchain, UINT64_MAX,
                                             impl->imageAvailable[impl->currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain(*impl);
        return;
    }

    impl->currentImageIndex = imageIndex;
    vkResetFences(impl->device, 1, &impl->fences[impl->currentFrame]);

    Mat4 view = camera.getViewMatrix();
    Mat4 proj = camera.getProjectionMatrix();

    UBO ubo;
    std::memcpy(ubo.model, Mat4::identity().m, 16 * sizeof(float));
    std::memcpy(ubo.view, view.m, 16 * sizeof(float));
    std::memcpy(ubo.proj, proj.m, 16 * sizeof(float));

    Vec3 ld = impl->lightDir.normalized();
    ubo.lightDir[0] = ld.x; ubo.lightDir[1] = ld.y; ubo.lightDir[2] = ld.z; ubo.lightDir[3] = 1.0f;
    ubo.color[0] = 1.0f; ubo.color[1] = 1.0f; ubo.color[2] = 1.0f; ubo.color[3] = impl->ambient;

    std::memcpy(impl->uboMapped, &ubo, sizeof(UBO));

    VkCommandBuffer cmd = impl->cmdBuffers[imageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkClearValue clearColor = { {{0.1f, 0.1f, 0.15f, 1.0f}} };
    VkRenderPassBeginInfo rpBegin = {};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = impl->renderPass;
    rpBegin.framebuffer = impl->framebuffers[imageIndex];
    rpBegin.renderArea.offset = {0, 0};
    rpBegin.renderArea.extent = impl->swapExtent;
    rpBegin.clearValueCount = 1;
    rpBegin.pClearValues = &clearColor;

    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, impl->pipeline);

    VkViewport viewport = {};
    viewport.x = 0; viewport.y = 0;
    viewport.width = (float)impl->swapExtent.width;
    viewport.height = (float)impl->swapExtent.height;
    viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = impl->swapExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VulkanRenderer3D::endFrame() {
    if (!impl || !impl->initialized) return;

    VkCommandBuffer cmd = impl->cmdBuffers[impl->currentImageIndex];
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { impl->imageAvailable[impl->currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkSemaphore signalSemaphores[] = { impl->renderFinished[impl->currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(impl->graphicsQueue, 1, &submitInfo, impl->fences[impl->currentFrame]);

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = { impl->swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &impl->currentImageIndex;

    vkQueuePresentKHR(impl->presentQueue, &presentInfo);

    impl->currentFrame = (impl->currentFrame + 1) % VulkanRenderer3DInternal::MAX_FRAMES;
}

void VulkanRenderer3D::drawMesh(Mesh3D& mesh, const Mat4& model,
                                float r, float g, float b, float a) {
    drawMeshLit(mesh, model, r, g, b, impl->lightDir, 1.0f, impl->ambient);
}

void VulkanRenderer3D::drawMeshLit(Mesh3D& mesh, const Mat4& model,
                                    float r, float g, float b,
                                    const Vec3& lightDir, float lightIntensity,
                                    float ambient) {
    if (!impl || !impl->initialized) return;
    if (impl->cmdBuffers.empty()) return;

    if (mesh.cpuVertices.empty() || mesh.cpuIndices.empty()) return;

    VkCommandBuffer cmd = impl->cmdBuffers[impl->currentImageIndex];

    VulkanMeshBuffers* mb = nullptr;
    auto it = impl->meshCache.find(&mesh);
    if (it != impl->meshCache.end()) {
        mb = &it->second;
    } else {
        VulkanMeshBuffers newMb;
        newMb.indexCount = (uint32_t)mesh.cpuIndices.size();

        VkDeviceSize vbSize = mesh.cpuVertices.size() * sizeof(Vertex);
        VkBuffer stagingBuf;
        VkDeviceMemory stagingMem;
        createBuffer(*impl, vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuf, stagingMem);

        void* data;
        vkMapMemory(impl->device, stagingMem, 0, vbSize, 0, &data);
        memcpy(data, mesh.cpuVertices.data(), vbSize);
        vkUnmapMemory(impl->device, stagingMem);

        createBuffer(*impl, vbSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     newMb.vertexBuffer, newMb.vertexMemory);
        copyBuffer(*impl, stagingBuf, newMb.vertexBuffer, vbSize);

        vkDestroyBuffer(impl->device, stagingBuf, nullptr);
        vkFreeMemory(impl->device, stagingMem, nullptr);

        VkDeviceSize ibSize = mesh.cpuIndices.size() * sizeof(unsigned int);
        createBuffer(*impl, ibSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuf, stagingMem);

        vkMapMemory(impl->device, stagingMem, 0, ibSize, 0, &data);
        memcpy(data, mesh.cpuIndices.data(), ibSize);
        vkUnmapMemory(impl->device, stagingMem);

        createBuffer(*impl, ibSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     newMb.indexBuffer, newMb.indexMemory);
        copyBuffer(*impl, stagingBuf, newMb.indexBuffer, ibSize);

        vkDestroyBuffer(impl->device, stagingBuf, nullptr);
        vkFreeMemory(impl->device, stagingMem, nullptr);

        impl->meshCache[&mesh] = newMb;
        mb = &impl->meshCache[&mesh];
    }

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mb->vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, mb->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, mb->indexCount, 1, 0, 0, 0);
}

void VulkanRenderer3D::setLightDirection(float x, float y, float z) {
    if (impl) impl->lightDir = {x, y, z};
}

void VulkanRenderer3D::setAmbient(float a) {
    if (impl) impl->ambient = a;
}

bool VulkanRenderer3D::isInitialized() const { return impl && impl->initialized; }
float VulkanRenderer3D::getScreenWidth() const { return impl ? impl->screenW : 0; }
float VulkanRenderer3D::getScreenHeight() const { return impl ? impl->screenH : 0; }

void VulkanRenderer3D::onResize(float w, float h) {
    if (!impl) return;
    impl->screenW = w;
    impl->screenH = h;
    if (impl->initialized) recreateSwapchain(*impl);
}
