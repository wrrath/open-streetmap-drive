#include "graphics/VulkanRenderer.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <vector>

namespace osm_drive::graphics {
namespace {
constexpr std::array<const char*, 1> DeviceExtensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats.front();
}

VkExtent2D chooseExtent(GLFWwindow* window, const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    return {
        std::clamp(static_cast<std::uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp(static_cast<std::uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
    };
}
} // namespace

VulkanRenderer::VulkanRenderer(int width, int height, const char* title) {
    if (glfwInit() == GLFW_FALSE) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window_ == nullptr) {
        throw std::runtime_error("Failed to create window");
    }

    createInstance();
    createSurface();
    pickPhysicalDevice();
    createDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
}

VulkanRenderer::~VulkanRenderer() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        if (inFlightFence_ != VK_NULL_HANDLE) vkDestroyFence(device_, inFlightFence_, nullptr);
        if (renderFinishedSemaphore_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, renderFinishedSemaphore_, nullptr);
        if (imageAvailableSemaphore_ != VK_NULL_HANDLE) vkDestroySemaphore(device_, imageAvailableSemaphore_, nullptr);
        cleanupSwapchain();
        if (commandPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, commandPool_, nullptr);
        vkDestroyDevice(device_, nullptr);
    }
    if (surface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
    if (window_ != nullptr) glfwDestroyWindow(window_);
    glfwTerminate();
}

bool VulkanRenderer::shouldClose() const { return glfwWindowShouldClose(window_); }
void VulkanRenderer::pollEvents() const { glfwPollEvents(); }

void VulkanRenderer::drawFrame() {
    vkWaitForFences(device_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &inFlightFence_);

    std::uint32_t imageIndex = 0;
    const VkResult acquireResult = vkAcquireNextImageKHR(
        device_, swapchain_, UINT64_MAX, imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailableSemaphore_;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinishedSemaphore_;

    if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFence_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit Vulkan draw command buffer");
    }

    VkPresentInfoKHR presentInfo {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore_;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex;
    vkQueuePresentKHR(graphicsQueue_, &presentInfo);
}

void VulkanRenderer::createInstance() {
    VkApplicationInfo appInfo {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Open StreetMap Drive";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "OSM Drive Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    std::uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    VkInstanceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

void VulkanRenderer::createSurface() {
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan surface");
    }
}

void VulkanRenderer::pickPhysicalDevice() {
    std::uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan-capable GPU found");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());
    physicalDevice_ = devices.front();
}

std::uint32_t VulkanRenderer::findGraphicsQueueFamily() const {
    std::uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &familyCount, families.data());

    for (std::uint32_t i = 0; i < familyCount; ++i) {
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, i, surface_, &presentSupport);
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U && presentSupport == VK_TRUE) {
            return i;
        }
    }
    throw std::runtime_error("No graphics+present queue family found");
}

void VulkanRenderer::createDevice() {
    graphicsQueueFamily_ = findGraphicsQueueFamily();

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo {};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = graphicsQueueFamily_;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkDeviceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(DeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = DeviceExtensions.data();

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }
    vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
}

void VulkanRenderer::createSwapchain() {
    VkSurfaceCapabilitiesKHR capabilities {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &capabilities);

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
    if (formatCount == 0) throw std::runtime_error("No Vulkan surface formats available");
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());

    const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(formats);
    swapchainImageFormat_ = surfaceFormat.format;
    swapchainExtent_ = chooseExtent(window_, capabilities);

    std::uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) imageCount = std::min(imageCount, capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = swapchainImageFormat_;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapchainExtent_;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan swapchain");
    }

    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());
}

void VulkanRenderer::createImageViews() {
    swapchainImageViews_.resize(swapchainImages_.size());
    for (std::size_t i = 0; i < swapchainImages_.size(); ++i) {
        VkImageViewCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchainImages_[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainImageFormat_;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &createInfo, nullptr, &swapchainImageViews_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swapchain image view");
        }
    }
}

void VulkanRenderer::createRenderPass() {
    VkAttachmentDescription colorAttachment {};
    colorAttachment.format = swapchainImageFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorReference {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;

    VkRenderPassCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(device_, &createInfo, nullptr, &renderPass_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
}

void VulkanRenderer::createFramebuffers() {
    framebuffers_.resize(swapchainImageViews_.size());
    for (std::size_t i = 0; i < swapchainImageViews_.size(); ++i) {
        VkImageView attachments[] = {swapchainImageViews_[i]};
        VkFramebufferCreateInfo createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = renderPass_;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = attachments;
        createInfo.width = swapchainExtent_.width;
        createInfo.height = swapchainExtent_.height;
        createInfo.layers = 1;
        if (vkCreateFramebuffer(device_, &createInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void VulkanRenderer::createCommandPool() {
    VkCommandPoolCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.queueFamilyIndex = graphicsQueueFamily_;
    if (vkCreateCommandPool(device_, &createInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
}

void VulkanRenderer::createCommandBuffers() {
    commandBuffers_.resize(framebuffers_.size());
    VkCommandBufferAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = static_cast<std::uint32_t>(commandBuffers_.size());
    if (vkAllocateCommandBuffers(device_, &allocateInfo, commandBuffers_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }

    for (std::size_t i = 0; i < commandBuffers_.size(); ++i) {
        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(commandBuffers_[i], &beginInfo);

        VkClearValue clearColor {{{0.03f, 0.12f, 0.08f, 1.0f}}};
        VkRenderPassBeginInfo renderPassInfo {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass_;
        renderPassInfo.framebuffer = framebuffers_[i];
        renderPassInfo.renderArea.extent = swapchainExtent_;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        vkCmdBeginRenderPass(commandBuffers_[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(commandBuffers_[i]);

        if (vkEndCommandBuffer(commandBuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer");
        }
    }
}

void VulkanRenderer::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphore_) != VK_SUCCESS ||
        vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphore_) != VK_SUCCESS ||
        vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFence_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan sync objects");
    }
}

void VulkanRenderer::cleanupSwapchain() {
    for (VkFramebuffer framebuffer : framebuffers_) vkDestroyFramebuffer(device_, framebuffer, nullptr);
    framebuffers_.clear();
    if (renderPass_ != VK_NULL_HANDLE) vkDestroyRenderPass(device_, renderPass_, nullptr);
    renderPass_ = VK_NULL_HANDLE;
    for (VkImageView imageView : swapchainImageViews_) vkDestroyImageView(device_, imageView, nullptr);
    swapchainImageViews_.clear();
    if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
}

} // namespace osm_drive::graphics
