#pragma once

#include "Device.h"
#include <vulkan/vulkan.h>

namespace jhb
{
class SwapChain
{
  public:
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        VkResult result{VK_SUCCESS};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

  public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
    SwapChain(Device &device, VkExtent2D extent, std::shared_ptr<SwapChain> previous = nullptr);
    ~SwapChain();

    // Constructor only stores configuration. Call init() before using the swapchain.
    VkResult init();

    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device,
                                                         VkSurfaceKHR surface);

    VkExtent2D getSwapChainExtent()
    {
        return swapChainExtent;
    }

    VkFormat getSwapChainImageFormat()
    {
        return swapChainImageFormat;
    }

    float extentAspectRatio()
    {
        return static_cast<float>(swapChainExtent.width) /
               static_cast<float>(swapChainExtent.height);
    }

    VkImageView getSwapChianImageView(int index) const
    {
        return swapChainImageviews[index];
    }

    const std::vector<VkImageView> &getSwapChianImageViews() const
    {
        return swapChainImageviews;
    }

    size_t getImageCount()
    {
        return swapChainImages.size();
    }

    VkImage getImage(uint32_t index) const
    {
        return swapChainImages.at(index);
    }

    VkResult acquireNextImage(uint32_t *imageIndex);

    VkFormat findDepthFormat();

    VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex);
    VkResult submitComputeCommandBuffers(const VkCommandBuffer *buffers);

    bool compareSwapChainFormats(const SwapChain &swapChain) const
    {
        return swapChain.swapChainImageFormat == swapChainImageFormat;
    }

  private:
    VkResult createSwapChain();
    VkResult createImageViews();
    VkResult createSyncObjects();
    void destroyResources() noexcept;

    VkSurfaceFormatKHR
    chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
    VkPresentModeKHR
    chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilites);

  private:
    Device &device;
    // Each old/new swapchain retains its own format during recreation.
    VkFormat swapChainImageFormat{VK_FORMAT_UNDEFINED};
    VkExtent2D swapChainExtent;

    VkSwapchainKHR swapChain{VK_NULL_HANDLE};

    std::shared_ptr<SwapChain> oldSwapchain;

    std::vector<VkImage> swapChainImages; // images created by swapchain config
    std::vector<VkImageView> swapChainImageviews;

  public:
    std::vector<VkSemaphore> imageAvailableSemaphores;
    // Indexed by acquired imageIndex, NOT by the frame-in-flight slot.
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    std::vector<VkSemaphore> computeSemaphores;
    std::vector<VkFence> computeFences;

    size_t currentFrame = 0;
};
} // namespace jhb
