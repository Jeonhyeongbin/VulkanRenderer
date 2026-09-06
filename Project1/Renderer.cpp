#include "Renderer.h"
#include <array>
#include <memory>

namespace jhb
{
Renderer::Renderer(Window &window, Device &device) : window(window), device(device)
{
    recreateSwapChain();
    createCommandBuffers();
    window.resetWindowResizedFlag();
}

Renderer::~Renderer()
{
    vkDeviceWaitIdle(device.getLogicalDevice());
    freeCommandBuffers();
}

void Renderer::createCommandBuffers()
{
    commandBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = device.getCommnadPool();
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(device.getLogicalDevice(), &allocInfo, commandBuffers.data()) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void Renderer::freeCommandBuffers()
{
    vkFreeCommandBuffers(device.getLogicalDevice(), device.getCommnadPool(),
                         static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    commandBuffers.clear();
}

void Renderer::recreateSwapChain()
{
    extent = window.getExtent();
    while (extent.width == 0 || extent.height == 0)
    {
        extent = window.getExtent();
#ifndef ANDROID
        glfwWaitEvents();
#endif
    }

    vkDeviceWaitIdle(
        device.getLogicalDevice()); // wait until current swapchain is no longer being used

    if (swapChain == nullptr)
    {
        swapChain = std::make_unique<SwapChain>(device, extent);
    }
    else
    {
        std::shared_ptr<SwapChain> oldSwapChain = std::move(swapChain);
        swapChain = std::make_unique<SwapChain>(device, extent, oldSwapChain);
    }

    ++swapChainGeneration_;

    // if new swap chain created, there is no longer create new commandbuffer
    // if render pass compatible than pipeline doesn't need to recreate
    // todo
}

void Renderer::excuteComputeDispatch(const VkCommandBuffer *buffers)
{
    swapChain->submitComputeCommandBuffers(buffers);
}

VkCommandBuffer Renderer::beginFrame()
{
    assert(!isFrameStarted && "Can't call beginFrame while already in progress");

    // wasWindowResized()를 acquireNextImage() 보다 먼저 체크한다.
    // acquireNextImage() 이후에 스왑체인을 파괴하면 Android에서
    // "freeAllBuffers: N buffers were freed while being dequeued!" 경고가 발생하고
    // 드라이버 상태가 오염되어 다음 프레임에서 SIGSEGV가 발생할 수 있다.
    if (window.wasWindowResized())
    {
        // 재생성 전에 플래그를 리셋해야 한다.
        // 리셋하지 않으면 beginFrame이 매 프레임 return nullptr를 반복하는 무한루프에 빠지고
        // Android에서 "freeAllBuffers: buffers were freed while being dequeued!" + SIGSEGV가
        // 발생한다.
        window.resetWindowResizedFlag();
        recreateSwapChain();
        return nullptr;
    }

    auto result = swapChain->acquireNextImage(&currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // OUT_OF_DATE: 이미지가 획득되지 않았으므로 즉시 재생성 가능
        // window.getExtent()로 최신 크기를 전달하고 플래그도 함께 리셋한다.
        window.resetWindowResizedFlag();
        recreateSwapChain();
        return nullptr;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    // VK_SUBOPTIMAL_KHR: 이미지는 획득됨 — 이번 프레임을 그대로 진행하고
    // endFrame()에서 present 후 재생성한다.
    currentFrameIndex = static_cast<int>(swapChain->currentFrame);
    isFrameStarted = true;
    auto commandBuffer = getCurrentCommandBuffer();

    if (vkResetCommandBuffer(commandBuffer, 0) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to reset the frame command buffer");
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }
    return commandBuffer;
}

void Renderer::endFrame()
{
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress!!");
    auto commandBuffer = getCurrentCommandBuffer();

    if (isRendering_)
    {
        throw std::logic_error("End the dynamic rendering pass before ending the frame");
    }

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }

    auto result = swapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        window.wasWindowResized())
    {
        window.resetWindowResizedFlag();
        recreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }

    isFrameStarted = false;
    currentFrameIndex = static_cast<int>(swapChain->currentFrame);
}

} // namespace jhb
