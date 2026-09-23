#include "graphics/VulkanRenderer.hpp"
#include "core/Log.hpp"
#include "map/RoadNetwork.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace osm_drive::graphics {
namespace {
constexpr std::array<const char*, 1> DeviceExtensions {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

struct PushConstants { glm::mat4 mvp; };
struct HudVertex { glm::vec2 position; glm::vec3 color; };
constexpr VkDeviceSize HudBufferBytes = sizeof(HudVertex) * 12000;

// Tiny 5x7 bitmap alphabet: no textures, descriptors, or font dependency.
// Each lit cell becomes two triangles in clip space.
std::array<const char*, 7> glyph(char c) {
    switch (c) {
    case '0': return {"11111","10001","10011","10101","11001","10001","11111"};
    case '1': return {"00100","01100","00100","00100","00100","00100","01110"};
    case '2': return {"11110","00001","00001","11110","10000","10000","11111"};
    case '3': return {"11110","00001","00001","01110","00001","00001","11110"};
    case '4': return {"10010","10010","10010","11111","00010","00010","00010"};
    case '5': return {"11111","10000","10000","11110","00001","00001","11110"};
    case '6': return {"01111","10000","10000","11110","10001","10001","01110"};
    case '7': return {"11111","00001","00010","00100","01000","01000","01000"};
    case '8': return {"01110","10001","10001","01110","10001","10001","01110"};
    case '9': return {"01110","10001","10001","01111","00001","00001","11110"};
    case 'A': return {"01110","10001","10001","11111","10001","10001","10001"};
    case 'D': return {"11110","10001","10001","10001","10001","10001","11110"};
    case 'E': return {"11111","10000","10000","11110","10000","10000","11111"};
    case 'G': return {"01111","10000","10000","10111","10001","10001","01110"};
    case 'H': return {"10001","10001","10001","11111","10001","10001","10001"};
    case 'K': return {"10001","10010","10100","11000","10100","10010","10001"};
    case 'M': return {"10001","11011","10101","10101","10001","10001","10001"};
    case 'P': return {"11110","10001","10001","11110","10000","10000","10000"};
    case 'R': return {"11110","10001","10001","11110","10100","10010","10001"};
    case 'S': return {"01111","10000","10000","01110","00001","00001","11110"};
    case 'V': return {"10001","10001","10001","10001","10001","01010","00100"};
    default: return {"00000","00000","00000","00000","00000","00000","00000"};
    }
}

void addHudRect(std::vector<HudVertex>& vertices, float x, float y, float width, float height,
                glm::vec3 color, VkExtent2D extent) {
    const float sx = 2.0f / static_cast<float>(extent.width);
    const float sy = 2.0f / static_cast<float>(extent.height);
    const float left = -1.0f + x * sx;
    const float right = -1.0f + (x + width) * sx;
    const float top = 1.0f - y * sy;
    const float bottom = 1.0f - (y + height) * sy;
    vertices.insert(vertices.end(), {{{left, top}, color}, {{left, bottom}, color}, {{right, bottom}, color},
                                     {{left, top}, color}, {{right, bottom}, color}, {{right, top}, color}});
}

void addHudText(std::vector<HudVertex>& vertices, const std::string& text, float x, float y,
                float pixel, glm::vec3 color, VkExtent2D extent) {
    const float sx = 2.0f / static_cast<float>(extent.width);
    const float sy = 2.0f / static_cast<float>(extent.height);
    for (char c : text) {
        const auto rows = glyph(c);
        for (int row = 0; row < 7; ++row) for (int col = 0; col < 5; ++col) {
            if (rows[static_cast<std::size_t>(row)][col] != '1') continue;
            const float left = -1.0f + (x + static_cast<float>(col) * pixel) * sx;
            const float right = -1.0f + (x + static_cast<float>(col + 1) * pixel - 1.0f) * sx;
            const float top = 1.0f - (y + static_cast<float>(row) * pixel) * sy;
            const float bottom = 1.0f - (y + static_cast<float>(row + 1) * pixel - 1.0f) * sy;
            vertices.insert(vertices.end(), {{{left, top}, color}, {{left, bottom}, color}, {{right, bottom}, color},
                                             {{left, top}, color}, {{right, bottom}, color}, {{right, top}, color}});
        }
        x += pixel * 6.0f;
    }
}

struct CarVertex {
    glm::vec3 position;
    glm::vec3 color;
};

// Two deliberately simple prisms form a chunky late-90s sedan silhouette.
// Keeping this mesh procedural makes the coordinate convention (+Z forward,
// +Y up) obvious and avoids introducing an asset loader for one small object.
const std::array<CarVertex, 16> CarVertices {{
    {{-1.0f, 0.0f, -2.0f}, {0.72f, 0.05f, 0.04f}},
    {{ 1.0f, 0.0f, -2.0f}, {0.72f, 0.05f, 0.04f}},
    {{ 1.0f, 0.0f,  2.0f}, {0.72f, 0.05f, 0.04f}},
    {{-1.0f, 0.0f,  2.0f}, {0.72f, 0.05f, 0.04f}},
    {{-1.0f, 0.65f, -1.8f}, {0.90f, 0.10f, 0.06f}},
    {{ 1.0f, 0.65f, -1.8f}, {0.90f, 0.10f, 0.06f}},
    {{ 1.0f, 0.65f,  1.7f}, {0.90f, 0.10f, 0.06f}},
    {{-1.0f, 0.65f,  1.7f}, {0.90f, 0.10f, 0.06f}},
    {{-0.78f, 0.65f, -0.85f}, {0.06f, 0.12f, 0.17f}},
    {{ 0.78f, 0.65f, -0.85f}, {0.06f, 0.12f, 0.17f}},
    {{ 0.78f, 0.65f,  1.05f}, {0.06f, 0.12f, 0.17f}},
    {{-0.78f, 0.65f,  1.05f}, {0.06f, 0.12f, 0.17f}},
    {{-0.58f, 1.42f, -0.55f}, {0.10f, 0.20f, 0.27f}},
    {{ 0.58f, 1.42f, -0.55f}, {0.10f, 0.20f, 0.27f}},
    {{ 0.58f, 1.42f,  0.68f}, {0.10f, 0.20f, 0.27f}},
    {{-0.58f, 1.42f,  0.68f}, {0.10f, 0.20f, 0.27f}},
}};

constexpr std::array<std::uint32_t, 72> CarIndices {{
    0,1,5, 0,5,4, 1,2,6, 1,6,5, 2,3,7, 2,7,6,
    3,0,4, 3,4,7, 4,5,6, 4,6,7, 3,2,1, 3,1,0,
    8,9,13, 8,13,12, 9,10,14, 9,14,13, 10,11,15, 10,15,14,
    11,8,12, 11,12,15, 12,13,14, 12,14,15, 11,10,9, 11,9,8
}};

std::vector<char> readBinaryFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) throw std::runtime_error("Missing shader: " + path.string());
    const auto size = file.tellg();
    std::vector<char> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(bytes.data(), size);
    return bytes;
}

VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return format;
    }
    return formats.front();
}

VkExtent2D chooseExtent(GLFWwindow* window, const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) return capabilities.currentExtent;
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    return {std::clamp(static_cast<std::uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp(static_cast<std::uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
}
} // namespace

VulkanRenderer::VulkanRenderer(int width, int height, const char* title) {
    if (glfwInit() == GLFW_FALSE) throw std::runtime_error("Failed to initialize GLFW");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window_ == nullptr) throw std::runtime_error("Failed to create window");

    createInstance();
    createSurface();
    pickPhysicalDevice();
    createDevice();
    createSwapchain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createCarMeshBuffers();
    createBuffer(HudBufferBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 hudVertexBuffer_, hudVertexMemory_);
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
        if (terrainIndexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, terrainIndexBuffer_, nullptr);
        if (terrainIndexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, terrainIndexMemory_, nullptr);
        if (terrainVertexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, terrainVertexBuffer_, nullptr);
        if (terrainVertexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, terrainVertexMemory_, nullptr);
        if (carIndexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, carIndexBuffer_, nullptr);
        if (carIndexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, carIndexMemory_, nullptr);
        if (carVertexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, carVertexBuffer_, nullptr);
        if (carVertexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, carVertexMemory_, nullptr);
        if (hudVertexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, hudVertexBuffer_, nullptr);
        if (hudVertexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, hudVertexMemory_, nullptr);
        if (roadIndexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, roadIndexBuffer_, nullptr);
        if (roadIndexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, roadIndexMemory_, nullptr);
        if (roadVertexBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(device_, roadVertexBuffer_, nullptr);
        if (roadVertexMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, roadVertexMemory_, nullptr);
        cleanupSwapchain();
        if (terrainPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, terrainPipeline_, nullptr);
        if (carPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, carPipeline_, nullptr);
        if (hudPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, hudPipeline_, nullptr);
        if (hudPipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, hudPipelineLayout_, nullptr);
        if (roadPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, roadPipeline_, nullptr);
        if (pipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
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

void VulkanRenderer::setRoadMesh(const map::RoadMesh& mesh) {
    roadIndexCount_ = static_cast<std::uint32_t>(mesh.indices.size());
    if (mesh.vertices.empty() || mesh.indices.empty()) return;

    const VkDeviceSize vertexBytes = sizeof(map::RoadVertex) * mesh.vertices.size();
    const VkDeviceSize indexBytes = sizeof(std::uint32_t) * mesh.indices.size();

    createBuffer(vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 roadVertexBuffer_, roadVertexMemory_);
    void* data = nullptr;
    vkMapMemory(device_, roadVertexMemory_, 0, vertexBytes, 0, &data);
    std::memcpy(data, mesh.vertices.data(), static_cast<std::size_t>(vertexBytes));
    vkUnmapMemory(device_, roadVertexMemory_);

    createBuffer(indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 roadIndexBuffer_, roadIndexMemory_);
    vkMapMemory(device_, roadIndexMemory_, 0, indexBytes, 0, &data);
    std::memcpy(data, mesh.indices.data(), static_cast<std::size_t>(indexBytes));
    vkUnmapMemory(device_, roadIndexMemory_);

    core::log(core::LogLevel::Info, "Uploaded road mesh to Vulkan vertex/index buffers");
}

void VulkanRenderer::setTerrainMesh(const map::RoadMesh& mesh) {
    terrainIndexCount_ = static_cast<std::uint32_t>(mesh.indices.size());
    if (mesh.vertices.empty() || mesh.indices.empty()) return;

    const VkDeviceSize vertexBytes = sizeof(map::RoadVertex) * mesh.vertices.size();
    const VkDeviceSize indexBytes = sizeof(std::uint32_t) * mesh.indices.size();
    createBuffer(vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 terrainVertexBuffer_, terrainVertexMemory_);
    void* data = nullptr;
    vkMapMemory(device_, terrainVertexMemory_, 0, vertexBytes, 0, &data);
    std::memcpy(data, mesh.vertices.data(), static_cast<std::size_t>(vertexBytes));
    vkUnmapMemory(device_, terrainVertexMemory_);
    createBuffer(indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 terrainIndexBuffer_, terrainIndexMemory_);
    vkMapMemory(device_, terrainIndexMemory_, 0, indexBytes, 0, &data);
    std::memcpy(data, mesh.indices.data(), static_cast<std::size_t>(indexBytes));
    vkUnmapMemory(device_, terrainIndexMemory_);
    core::log(core::LogLevel::Info, "Uploaded terrain grid to Vulkan vertex/index buffers");
}

void VulkanRenderer::setFollowCamera(glm::vec3 vehiclePosition, float vehicleHeadingRadians) {
    cameraVehiclePosition_ = vehiclePosition;
    cameraVehicleHeadingRadians_ = vehicleHeadingRadians;
}

void VulkanRenderer::setVehicleTransform(glm::vec3 vehiclePosition, float vehicleHeadingRadians) {
    setFollowCamera(vehiclePosition, vehicleHeadingRadians);
}

void VulkanRenderer::drawFrame() {
    vkWaitForFences(device_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &inFlightFence_);

    std::uint32_t imageIndex = 0;
    const VkResult acquireResult = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) throw std::runtime_error("Failed to acquire swapchain image");

    recordCommandBuffer(imageIndex);

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
    if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFence_) != VK_SUCCESS) throw std::runtime_error("Failed to submit Vulkan draw command buffer");

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
    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) throw std::runtime_error("Failed to create Vulkan instance");
}

void VulkanRenderer::createSurface() {
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS) throw std::runtime_error("Failed to create Vulkan surface");
}

void VulkanRenderer::pickPhysicalDevice() {
    std::uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) throw std::runtime_error("No Vulkan-capable GPU found");
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
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U && presentSupport == VK_TRUE) return i;
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
    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) throw std::runtime_error("Failed to create logical device");
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
    if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_) != VK_SUCCESS) throw std::runtime_error("Failed to create Vulkan swapchain");
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
        if (vkCreateImageView(device_, &createInfo, nullptr, &swapchainImageViews_[i]) != VK_SUCCESS) throw std::runtime_error("Failed to create swapchain image view");
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
    VkSubpassDependency dependency {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;
    if (vkCreateRenderPass(device_, &createInfo, nullptr, &renderPass_) != VK_SUCCESS) throw std::runtime_error("Failed to create render pass");
}

VkShaderModule VulkanRenderer::loadShaderModule(const std::filesystem::path& path) const {
    const auto bytes = readBinaryFile(path);
    VkShaderModuleCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = bytes.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(bytes.data());
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device_, &createInfo, nullptr, &module) != VK_SUCCESS) throw std::runtime_error("Failed to create shader module: " + path.string());
    return module;
}

void VulkanRenderer::createGraphicsPipeline() {
    const std::filesystem::path shaderDir = OSM_DRIVE_SHADER_DIR;
    const auto vertPath = shaderDir / "road.vert.spv";
    const auto fragPath = shaderDir / "road.frag.spv";
    if (!std::filesystem::exists(vertPath) || !std::filesystem::exists(fragPath) ||
        !std::filesystem::exists(shaderDir / "car.vert.spv") || !std::filesystem::exists(shaderDir / "car.frag.spv")) {
        core::log(core::LogLevel::Warning, "Compiled road/car shaders not found; showing clear color only. Install glslc and rerun CMake to render meshes.");
        return;
    }

    VkShaderModule vert = loadShaderModule(vertPath);
    VkShaderModule frag = loadShaderModule(fragPath);
    std::array<VkPipelineShaderStageCreateInfo, 2> stages {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding {};
    binding.binding = 0;
    binding.stride = sizeof(map::RoadVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    std::array<VkVertexInputAttributeDescription, 3> attributes {};
    attributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(map::RoadVertex, position)};
    attributes[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(map::RoadVertex, normal)};
    attributes[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(map::RoadVertex, uv)};
    VkPipelineVertexInputStateCreateInfo vertexInput {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo assembly {};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport viewport {0.0f, 0.0f, static_cast<float>(swapchainExtent_.width), static_cast<float>(swapchainExtent_.height), 0.0f, 1.0f};
    VkRect2D scissor {{0, 0}, swapchainExtent_};
    VkPipelineViewportStateCreateInfo viewportState {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    VkPipelineRasterizationStateCreateInfo rasterizer {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisampling {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blendAttachment {};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blending {};
    blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.attachmentCount = 1;
    blending.pAttachments = &blendAttachment;
    VkPushConstantRange pushRange {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants)};
    VkPipelineLayoutCreateInfo layoutInfo {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) throw std::runtime_error("Failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo pipelineInfo {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<std::uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &assembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &blending;
    pipelineInfo.layout = pipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &roadPipeline_) != VK_SUCCESS) throw std::runtime_error("Failed to create road graphics pipeline");

    // The car uses the same render pass and push-constant layout as roads, but
    // has a smaller position/color vertex format and its own fragment shader.
    VkShaderModule carVert = loadShaderModule(shaderDir / "car.vert.spv");
    VkShaderModule carFrag = loadShaderModule(shaderDir / "car.frag.spv");
    stages[0].module = carVert;
    stages[1].module = carFrag;
    binding.stride = sizeof(CarVertex);
    std::array<VkVertexInputAttributeDescription, 2> carAttributes {};
    carAttributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(CarVertex, position)};
    carAttributes[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(CarVertex, color)};
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(carAttributes.size());
    vertexInput.pVertexAttributeDescriptions = carAttributes.data();
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &carPipeline_) != VK_SUCCESS) throw std::runtime_error("Failed to create car graphics pipeline");

    vkDestroyShaderModule(device_, carFrag, nullptr);
    vkDestroyShaderModule(device_, carVert, nullptr);

    const auto terrainFragPath = shaderDir / "terrain.frag.spv";
    if (std::filesystem::exists(terrainFragPath)) {
        VkShaderModule terrainFrag = loadShaderModule(terrainFragPath);
        stages[0].module = vert;
        stages[1].module = terrainFrag;
        binding.stride = sizeof(map::RoadVertex);
        vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();
        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &terrainPipeline_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create terrain graphics pipeline");
        }
        vkDestroyShaderModule(device_, terrainFrag, nullptr);
    }
    vkDestroyShaderModule(device_, frag, nullptr);
    vkDestroyShaderModule(device_, vert, nullptr);

    // HUD is a second pipeline in the same render pass. Its vertices are
    // already in clip space, so it needs no uniforms or descriptor sets.
    vert = loadShaderModule(shaderDir / "hud.vert.spv");
    frag = loadShaderModule(shaderDir / "hud.frag.spv");
    stages[0].module = vert;
    stages[1].module = frag;
    binding.stride = sizeof(HudVertex);
    std::array<VkVertexInputAttributeDescription, 2> hudAttributes {};
    hudAttributes[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(HudVertex, position)};
    hudAttributes[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(HudVertex, color)};
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(hudAttributes.size());
    vertexInput.pVertexAttributeDescriptions = hudAttributes.data();
    VkPipelineLayoutCreateInfo hudLayoutInfo {};
    hudLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (vkCreatePipelineLayout(device_, &hudLayoutInfo, nullptr, &hudPipelineLayout_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create HUD pipeline layout");
    pipelineInfo.layout = hudPipelineLayout_;
    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &hudPipeline_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create HUD graphics pipeline");
    vkDestroyShaderModule(device_, frag, nullptr);
    vkDestroyShaderModule(device_, vert, nullptr);
}

void VulkanRenderer::createCarMeshBuffers() {
    if (carPipeline_ == VK_NULL_HANDLE) return;
    carIndexCount_ = static_cast<std::uint32_t>(CarIndices.size());

    // These host-visible buffers are straightforward for this static, tiny
    // mesh. A staging/device-local upload path can replace them for large assets.
    const VkDeviceSize vertexBytes = sizeof(CarVertices);
    createBuffer(vertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 carVertexBuffer_, carVertexMemory_);
    void* data = nullptr;
    vkMapMemory(device_, carVertexMemory_, 0, vertexBytes, 0, &data);
    std::memcpy(data, CarVertices.data(), sizeof(CarVertices));
    vkUnmapMemory(device_, carVertexMemory_);

    const VkDeviceSize indexBytes = sizeof(CarIndices);
    createBuffer(indexBytes, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 carIndexBuffer_, carIndexMemory_);
    vkMapMemory(device_, carIndexMemory_, 0, indexBytes, 0, &data);
    std::memcpy(data, CarIndices.data(), sizeof(CarIndices));
    vkUnmapMemory(device_, carIndexMemory_);
    core::log(core::LogLevel::Info, "Uploaded procedural player car mesh");
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
        if (vkCreateFramebuffer(device_, &createInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS) throw std::runtime_error("Failed to create framebuffer");
    }
}

void VulkanRenderer::createCommandPool() {
    VkCommandPoolCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = graphicsQueueFamily_;
    if (vkCreateCommandPool(device_, &createInfo, nullptr, &commandPool_) != VK_SUCCESS) throw std::runtime_error("Failed to create command pool");
}

void VulkanRenderer::createCommandBuffers() {
    commandBuffers_.resize(framebuffers_.size());
    VkCommandBufferAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = static_cast<std::uint32_t>(commandBuffers_.size());
    if (vkAllocateCommandBuffers(device_, &allocateInfo, commandBuffers_.data()) != VK_SUCCESS) throw std::runtime_error("Failed to allocate command buffers");
}

void VulkanRenderer::recordCommandBuffer(std::uint32_t imageIndex) {
    const float aspect = static_cast<float>(swapchainExtent_.width) / static_cast<float>(swapchainExtent_.height);
    glm::mat4 projection = glm::perspective(glm::radians(62.0f), aspect, 0.1f, 2500.0f);
    projection[1][1] *= -1.0f;

    const glm::vec3 forward {std::sin(cameraVehicleHeadingRadians_), 0.0f, std::cos(cameraVehicleHeadingRadians_)};
    const glm::vec3 cameraPosition = cameraVehiclePosition_ - forward * 22.0f + glm::vec3(0.0f, 10.0f, 0.0f);
    const glm::vec3 lookAt = cameraVehiclePosition_ + forward * 18.0f + glm::vec3(0.0f, 1.5f, 0.0f);
    const glm::mat4 view = glm::lookAt(cameraPosition, lookAt, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 viewProjection = projection * view;
    const PushConstants roadPush {viewProjection};
    glm::mat4 carModel = glm::translate(glm::mat4(1.0f), cameraVehiclePosition_);
    carModel = glm::rotate(carModel, cameraVehicleHeadingRadians_, glm::vec3(0.0f, 1.0f, 0.0f));
    const PushConstants carPush {viewProjection * carModel};

    VkCommandBuffer commandBuffer = commandBuffers_[imageIndex];
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    VkClearValue clearColor {{{0.48f, 0.66f, 0.86f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = framebuffers_[imageIndex];
    renderPassInfo.renderArea.extent = swapchainExtent_;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    const VkDeviceSize offset = 0;
    if (terrainPipeline_ != VK_NULL_HANDLE && terrainVertexBuffer_ != VK_NULL_HANDLE && terrainIndexBuffer_ != VK_NULL_HANDLE && terrainIndexCount_ > 0) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, terrainPipeline_);
        vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &roadPush);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &terrainVertexBuffer_, &offset);
        vkCmdBindIndexBuffer(commandBuffer, terrainIndexBuffer_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, terrainIndexCount_, 1, 0, 0, 0);
    }
    if (roadPipeline_ != VK_NULL_HANDLE && roadVertexBuffer_ != VK_NULL_HANDLE && roadIndexBuffer_ != VK_NULL_HANDLE && roadIndexCount_ > 0) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, roadPipeline_);
        vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &roadPush);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &roadVertexBuffer_, &offset);
        vkCmdBindIndexBuffer(commandBuffer, roadIndexBuffer_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, roadIndexCount_, 1, 0, 0, 0);
    }
    if (carPipeline_ != VK_NULL_HANDLE && carVertexBuffer_ != VK_NULL_HANDLE && carIndexBuffer_ != VK_NULL_HANDLE) {
        // Push constants are copied directly into the command buffer, so each
        // object can use a different model transform without a uniform buffer.
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, carPipeline_);
        vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &carPush);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &carVertexBuffer_, &offset);
        vkCmdBindIndexBuffer(commandBuffer, carIndexBuffer_, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, carIndexCount_, 1, 0, 0, 0);
    }
    if (hudPipeline_ != VK_NULL_HANDLE && hudVertexBuffer_ != VK_NULL_HANDLE) {
        std::vector<HudVertex> hudVertices;
        hudVertices.reserve(6000);
        char text[64] {};
        const int mph = static_cast<int>(std::round(telemetry_.speedMetersPerSecond * 2.23694f));
        const int kmh = static_cast<int>(std::round(telemetry_.speedMetersPerSecond * 3.6f));
        // Draw a simple opaque panel first so the world cannot visually merge
        // with the glyphs. Lines use a fixed baseline and conservative spacing
        // to avoid overlapping on small WSLg/window framebuffers.
        addHudRect(hudVertices, 24.0f, 24.0f, 270.0f, 150.0f, {0.02f, 0.025f, 0.03f}, swapchainExtent_);
        addHudRect(hudVertices, 28.0f, 28.0f, 262.0f, 142.0f, {0.06f, 0.075f, 0.085f}, swapchainExtent_);

        std::snprintf(text, sizeof(text), "MPH %03d", mph);
        addHudText(hudVertices, text, 40.0f, 40.0f, 4.0f, {1.0f, 1.0f, 1.0f}, swapchainExtent_);
        std::snprintf(text, sizeof(text), "KMH %03d", kmh);
        addHudText(hudVertices, text, 40.0f, 78.0f, 3.5f, {0.72f, 0.88f, 1.0f}, swapchainExtent_);
        std::snprintf(text, sizeof(text), "RPM %04d", static_cast<int>(std::round(telemetry_.rpm)));
        addHudText(hudVertices, text, 40.0f, 111.0f, 3.5f,
                   telemetry_.rpm > 6000.0f ? glm::vec3{1.0f, 0.25f, 0.2f} : glm::vec3{1.0f, 0.75f, 0.18f}, swapchainExtent_);
        std::snprintf(text, sizeof(text), "GEAR %d", telemetry_.gear);
        addHudText(hudVertices, text, 40.0f, 144.0f, 3.5f, {0.55f, 1.0f, 0.55f}, swapchainExtent_);
        const VkDeviceSize bytes = sizeof(HudVertex) * hudVertices.size();
        if (bytes <= HudBufferBytes) {
            void* mapped = nullptr;
            vkMapMemory(device_, hudVertexMemory_, 0, bytes, 0, &mapped);
            std::memcpy(mapped, hudVertices.data(), static_cast<std::size_t>(bytes));
            vkUnmapMemory(device_, hudVertexMemory_);
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hudPipeline_);
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &hudVertexBuffer_, &offset);
            vkCmdDraw(commandBuffer, static_cast<std::uint32_t>(hudVertices.size()), 1, 0, 0);
        }
    }
    vkCmdEndRenderPass(commandBuffer);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) throw std::runtime_error("Failed to record command buffer");
}

void VulkanRenderer::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphore_) != VK_SUCCESS ||
        vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphore_) != VK_SUCCESS ||
        vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFence_) != VK_SUCCESS) throw std::runtime_error("Failed to create Vulkan sync objects");
}

std::uint32_t VulkanRenderer::findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memoryProperties {};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties);
    for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1U << i)) != 0U && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) return i;
    }
    throw std::runtime_error("Failed to find suitable Vulkan memory type");
}

void VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                                  VkBuffer& buffer, VkDeviceMemory& memory) const {
    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) throw std::runtime_error("Failed to create Vulkan buffer");
    VkMemoryRequirements requirements {};
    vkGetBufferMemoryRequirements(device_, buffer, &requirements);
    VkMemoryAllocateInfo allocateInfo {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, properties);
    if (vkAllocateMemory(device_, &allocateInfo, nullptr, &memory) != VK_SUCCESS) throw std::runtime_error("Failed to allocate Vulkan buffer memory");
    vkBindBufferMemory(device_, buffer, memory, 0);
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
