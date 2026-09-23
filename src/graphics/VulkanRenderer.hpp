#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include <vector>

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
    [[nodiscard]] GLFWwindow* window() const { return window_; }

private:
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createDevice();

    GLFWwindow* window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
};

} // namespace osm_drive::graphics
