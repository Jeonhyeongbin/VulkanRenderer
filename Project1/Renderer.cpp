#include "Renderer.h"
#include <array>
#include <memory>

namespace jhb
{
Renderer::Renderer(Window &window, Device &device) : window(window), device(device)
{
}

VkResult Renderer::init()
{
    const auto result = recreateSwapChain();
    if (result != VK_SUCCESS)
    {
        return result;
    }
    return createCommandBuffers();
}

Renderer::~Renderer()
{
    vkDeviceWaitIdle(device.getLogicalDevice());
    freeCommandBuffers();
}

VkResult Renderer::createCommandBuffers()
{
    commandBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = device.getCommnadPool();
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    const auto result =
        vkAllocateCommandBuffers(device.getLogicalDevice(), &allocInfo, commandBuffers.data());
    if (result != VK_SUCCESS)
    {
        commandBuffers.clear();
    }
    return result;
}

void Renderer::freeCommandBuffers()
{
    if (commandBuffers.empty())
    {
        return;
    }
    vkFreeCommandBuffers(device.getLogicalDevice(), device.getCommnadPool(),
                         static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    commandBuffers.clear();
}

VkResult Renderer::recreateSwapChain()
{
    // Keep the request until recreation succeeds, including OUT_OF_DATE requests.
    window.setWindowResizedFlag();
    extent = window.getExtent();
    if (extent.width == 0 || extent.height == 0)
    {
        // Retry from the next frame so the application can still process exit requests.
        return VK_NOT_READY;
    }

    const auto wait = vkDeviceWaitIdle(device.getLogicalDevice());
    if (wait != VK_SUCCESS)
    {
        return wait;
    }

    std::shared_ptr<SwapChain> previous = std::move(swapChain);
    auto next = std::make_unique<SwapChain>(device, extent, previous);
    const auto result = next->init();
    if (result != VK_SUCCESS)
    {
        // Creating a replacement can retire the old swapchain even on failure.
        // Do not restore it and continue rendering; the caller terminates the loop.
        return result;
    }
    swapChain = std::move(next);
    window.resetWindowResizedFlag();

    ++swapChainGeneration_;

    return VK_SUCCESS;
}

void Renderer::excuteComputeDispatch(const VkCommandBuffer *buffers)
{
    swapChain->submitComputeCommandBuffers(buffers);
}

VkResult Renderer::beginFrame(VkCommandBuffer &commandBuffer)
{
    commandBuffer = VK_NULL_HANDLE;
    assert(!isFrameStarted && "Can't call beginFrame while already in progress");

    // wasWindowResized()를 acquireNextImage() 보다 먼저 체크한다.
    // acquireNextImage() 이후에 스왑체인을 파괴하면 Android에서
    // "freeAllBuffers: N buffers were freed while being dequeued!" 경고가 발생하고
    // 드라이버 상태가 오염되어 다음 프레임에서 SIGSEGV가 발생할 수 있다.
    if (window.wasWindowResized())
    {
        const auto result = recreateSwapChain();
        return result == VK_SUCCESS ? VK_NOT_READY : result;
    }

    auto result = swapChain->acquireNextImage(&currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // OUT_OF_DATE: 이미지가 획득되지 않았으므로 즉시 재생성 가능
        result = recreateSwapChain();
        return result == VK_SUCCESS ? VK_NOT_READY : result;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        return result;
    }

    // VK_SUBOPTIMAL_KHR: 이미지는 획득됨 — 이번 프레임을 그대로 진행하고
    // endFrame()에서 present 후 재생성한다.
    if (result == VK_SUBOPTIMAL_KHR)
    {
        window.setWindowResizedFlag();
    }
    currentFrameIndex = static_cast<int>(swapChain->currentFrame);
    const auto frameCommandBuffer = commandBuffers[currentFrameIndex];

    result = vkResetCommandBuffer(frameCommandBuffer, 0);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    result = vkBeginCommandBuffer(frameCommandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
    {
        return result;
    }
    isFrameStarted = true;
    commandBuffer = frameCommandBuffer;
    return VK_SUCCESS;
}

VkResult Renderer::endFrame()
{
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress!!");
    auto commandBuffer = getCurrentCommandBuffer();

    assert(!isRendering_ && "End the dynamic rendering pass before ending the frame");

    isFrameStarted = false;
    auto result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    result = swapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);
    if (result != VK_SUCCESS && result != VK_ERROR_OUT_OF_DATE_KHR &&
        result != VK_SUBOPTIMAL_KHR)
    {
        return result;
    }
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        window.wasWindowResized())
    {
        return recreateSwapChain();
    }
    currentFrameIndex = static_cast<int>(swapChain->currentFrame);
    return VK_SUCCESS;
}

} // namespace jhb
