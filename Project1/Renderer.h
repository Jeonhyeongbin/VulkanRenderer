#pragma once
#define GLM_FORCE_RADIANS // not use degree;
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "Window.h"
#include "Device.h"
#include "SwapChain.h"

#include <stdint.h>
#include <cassert>

namespace jhb
{
class Renderer
{ // manage swap chain and render pass
  public:
    Renderer(Window &window, Device &device);
    ~Renderer();

    Renderer(const Renderer &) = delete;
    Renderer(Renderer &&) = delete;
    Renderer &operator=(const Renderer &) = delete;

    SwapChain &GetSwapChain()
    {
        return *swapChain;
    }

    VkImageView getSwapChainImageView(int index)
    {
        return swapChain->getSwapChianImageView(index);
    }

    const std::vector<VkImageView> &getSwapChainImageViews() const
    {
        return swapChain->getSwapChianImageViews();
    }

    float getAspectRatio() const
    {
        return swapChain->extentAspectRatio();
    }

    void setWindowExtent(VkExtent2D _extent)
    {
        extent = _extent;
    }

    void excuteComputeDispatch(const VkCommandBuffer *buffers);

    VkCommandBuffer beginFrame();
    void endFrame();

    // Dynamic rendering only; frame acquisition/submission stays in beginFrame/endFrame.
    bool isRendering() const
    {
        return isRendering_;
    }

    void beginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfoKHR &info)
    {
        if (!isFrameStarted || commandBuffer != getCurrentCommandBuffer() || isRendering_)
        {
            throw std::logic_error(
                "Cannot begin rendering outside the current frame or inside a pass");
        }
        // Device initialization already requires Vulkan 1.3 dynamicRendering.
        vkCmdBeginRendering(commandBuffer, &info);
        isRendering_ = true;
    }

    void endRendering(VkCommandBuffer commandBuffer)
    {
        if (!isFrameStarted || commandBuffer != getCurrentCommandBuffer() || !isRendering_)
        {
            throw std::logic_error("No matching dynamic rendering pass");
        }
        vkCmdEndRendering(commandBuffer);
        isRendering_ = false;
    }

    // Changes after every successful swapchain recreation, including endFrame.
    uint64_t getSwapChainGeneration() const
    {
        return swapChainGeneration_;
    }

    VkExtent2D getSwapChainExtent() const
    {
        return swapChain->getSwapChainExtent();
    }

    bool isFrameInProgress() const
    {
        return isFrameStarted;
    }

    VkCommandBuffer getCurrentCommandBuffer() const
    {
        assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
        return commandBuffers[currentFrameIndex];
    }

    int getFrameIndex() const
    {
        assert(isFrameStarted && "Cannot get frame index when frame not in progress!");
        return currentFrameIndex;
    }

    // 실제로 획득한 스왑체인 이미지 인덱스를 반환한다.
    // Android에서는 스왑체인 이미지가 3개일 수 있으므로
    // 스왑체인 기반 프레임버퍼 선택에는 getFrameIndex() 대신 이 함수를 사용해야 한다.
    int getImageIndex() const
    {
        assert(isFrameStarted && "Cannot get image index when frame not in progress!");
        return (int)currentImageIndex;
    }

  private:
    void createCommandBuffers();
    void freeCommandBuffers();
    void recreateSwapChain();

    // init top to bottom
    Window &window;
    Device &device;
    VkExtent2D extent;

    std::unique_ptr<SwapChain> swapChain;
    std::vector<VkCommandBuffer> commandBuffers;

    uint32_t currentImageIndex{0};
    int currentFrameIndex{0};
    bool isFrameStarted{false};
    bool isRendering_{false};
    uint64_t swapChainGeneration_{0};
};
} // namespace jhb
