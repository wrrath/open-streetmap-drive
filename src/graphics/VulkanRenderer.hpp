#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdint>
#include <filesystem>
#include <glm/vec3.hpp>
#include <string>
#include <vector>
#include "game/VehicleTelemetry.hpp"

namespace osm_drive::map { struct RoadMesh; }

namespace osm_drive::graphics {

class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, const char* title);
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    [[nodiscard]] bool shouldClose() const;
    void pollEvents() const;
    void drawFrame();
    void setRoadMesh(const map::RoadMesh& mesh);
    void setTerrainMesh(const map::RoadMesh& mesh);
    void setFollowCamera(glm::vec3 vehiclePosition, float vehicleHeadingRadians);
    void setVehicleTransform(glm::vec3 vehiclePosition, float vehicleHeadingRadians);
    void setVehicleTelemetry(const game::VehicleTelemetry& telemetry) { telemetry_ = telemetry; }
    [[nodiscard]] GLFWwindow* window() const { return window_; }

private:
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createDevice();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createGraphicsPipeline();
    void createCarMeshBuffers();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void cleanupSwapchain();
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& memory) const;
    [[nodiscard]] std::uint32_t findMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    [[nodiscard]] std::uint32_t findGraphicsQueueFamily() const;
    [[nodiscard]] VkShaderModule loadShaderModule(const std::filesystem::path& path) const;
    void recordCommandBuffer(std::uint32_t imageIndex);

    GLFWwindow* window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    std::uint32_t graphicsQueueFamily_ = 0;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_ {};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline roadPipeline_ = VK_NULL_HANDLE;
    VkPipeline terrainPipeline_ = VK_NULL_HANDLE;
    VkPipeline carPipeline_ = VK_NULL_HANDLE;
    VkPipeline hudPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout hudPipelineLayout_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    VkBuffer roadVertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory roadVertexMemory_ = VK_NULL_HANDLE;
    VkBuffer roadIndexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory roadIndexMemory_ = VK_NULL_HANDLE;
    std::uint32_t roadIndexCount_ = 0;
    VkBuffer terrainVertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory terrainVertexMemory_ = VK_NULL_HANDLE;
    VkBuffer terrainIndexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory terrainIndexMemory_ = VK_NULL_HANDLE;
    std::uint32_t terrainIndexCount_ = 0;

    // The car is a tiny procedural mesh with its own position/color layout.
    VkBuffer carVertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory carVertexMemory_ = VK_NULL_HANDLE;
    VkBuffer carIndexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory carIndexMemory_ = VK_NULL_HANDLE;
    std::uint32_t carIndexCount_ = 0;

    VkBuffer hudVertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory hudVertexMemory_ = VK_NULL_HANDLE;
    game::VehicleTelemetry telemetry_ {};

    glm::vec3 cameraVehiclePosition_ {0.0f, 0.35f, 0.0f};
    float cameraVehicleHeadingRadians_ = 0.0f;

    VkSemaphore imageAvailableSemaphore_ = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore_ = VK_NULL_HANDLE;
    VkFence inFlightFence_ = VK_NULL_HANDLE;
};

} // namespace osm_drive::graphics
