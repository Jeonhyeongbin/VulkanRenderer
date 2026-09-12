#pragma once
#ifndef ANDROID
#include "RenderGraph/RenderGraph.h"

struct ImGuiContext;

namespace jhb
{
class Device;

// Owns ImGui lifetime and its Vulkan/GLFW integration, not render targets or passes.
class ImGuiContext
{
  public:
    ImGuiContext(Device &device, VkFormat targetFormat, uint32_t imageCount);
    ~ImGuiContext();
    ImGuiContext(const ImGuiContext &) = delete;
    ImGuiContext &operator=(const ImGuiContext &) = delete;

    void configureTarget(VkFormat format, uint32_t imageCount);
    void beginFrame();
    void endFrame();
    rg::RenderFunction drawFunction();

  private:
    void initializeVulkan();
    void cleanup() noexcept;
    Device &device_;
    ::ImGuiContext *context_{};
    VkDescriptorPool pool_{VK_NULL_HANDLE};
    VkFormat format_{VK_FORMAT_UNDEFINED};
    uint32_t imageCount_{};
    uint64_t generation_{};
    bool glfwReady_{false}, vulkanReady_{false}, frameOpen_{false}, drawReady_{false};
};
} // namespace jhb
#endif
