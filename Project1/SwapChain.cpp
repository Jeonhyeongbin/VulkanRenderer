#include "SwapChain.h"
#include <array>

namespace jhb
{
SwapChain::SwapChain(Device &device, VkExtent2D extent, std::shared_ptr<SwapChain> previous)
    : device(device), swapChainExtent(extent), oldSwapchain(std::move(previous))
{
}

SwapChain::~SwapChain()
{
    destroyResources();
}

void SwapChain::destroyResources() noexcept
{
    for (auto view : swapChainImageviews)
    {
        vkDestroyImageView(device.getLogicalDevice(), view, nullptr);
    }
    vkDestroySwapchainKHR(device.getLogicalDevice(), swapChain, nullptr);

    for (auto semaphore : imageAvailableSemaphores)
    {
        vkDestroySemaphore(device.getLogicalDevice(), semaphore, nullptr);
    }
    for (auto semaphore : renderFinishedSemaphores)
    {
        vkDestroySemaphore(device.getLogicalDevice(), semaphore, nullptr);
    }
    for (auto semaphore : computeSemaphores)
    {
        vkDestroySemaphore(device.getLogicalDevice(), semaphore, nullptr);
    }
    for (auto fence : inFlightFences)
    {
        vkDestroyFence(device.getLogicalDevice(), fence, nullptr);
    }
    for (auto fence : computeFences)
    {
        vkDestroyFence(device.getLogicalDevice(), fence, nullptr);
    }
    swapChain = VK_NULL_HANDLE;
    swapChainImageviews.clear();
    swapChainImages.clear();
    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    computeSemaphores.clear();
    inFlightFences.clear();
    computeFences.clear();
}

VkResult SwapChain::init()
{
    if (swapChain != VK_NULL_HANDLE)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkResult result = createSwapChain();
    if (result == VK_SUCCESS)
    {
        result = createImageViews();
    }
    if (result == VK_SUCCESS)
    {
        result = createSyncObjects();
    }
    if (result != VK_SUCCESS)
    {
        destroyResources();
        return result;
    }
    oldSwapchain.reset();
    return VK_SUCCESS;
}

VkResult SwapChain::acquireNextImage(uint32_t *imageIndex)
{
    const auto wait = vkWaitForFences(device.getLogicalDevice(), 1, &inFlightFences[currentFrame],
                                      VK_TRUE, std::numeric_limits<uint64_t>::max());
    if (wait != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to wait for frame slot completion");
    }

    VkResult result = vkAcquireNextImageKHR(
        device.getLogicalDevice(), swapChain, std::numeric_limits<uint64_t>::max(),
        imageAvailableSemaphores[currentFrame], // must be a not signaled semaphore
        VK_NULL_HANDLE, imageIndex);

    return result;
}

VkFormat SwapChain::findDepthFormat()
{
    return device.findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkResult SwapChain::submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex)
{
    if (!buffers || !imageIndex || *imageIndex >= swapChainImages.size())
    {
        throw std::invalid_argument("Invalid command buffer or acquired swapchain image");
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = buffers;

    // A graphics fence does not cover presentation's semaphore wait. Reacquiring
    // this image (and waiting on imageAvailable) makes its present semaphore reusable.
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores.at(*imageIndex)};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkResetFences(device.getLogicalDevice(), 1, &inFlightFences[currentFrame]) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to reset frame fence before submission");
    }
    if (vkQueueSubmit(device.getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = imageIndex;

    auto result = vkQueuePresentKHR(device.getPresentQueue(), &presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    return result;
}

VkResult SwapChain::submitComputeCommandBuffers(const VkCommandBuffer *buffers)
{
    vkWaitForFences(device.getLogicalDevice(), 1, &computeFences[currentFrame], VK_TRUE,
                    UINT64_MAX);
    vkResetFences(device.getLogicalDevice(), 1, &computeFences[currentFrame]);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = buffers;

    VkSemaphore signalSemaphores[] = {computeSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkResult ret = vkQueueSubmit(device.getComputeQueue(), 1, &submitInfo, nullptr);
    if (ret != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
    return ret;
}

VkResult SwapChain::createSwapChain()
{
    SwapChainSupportDetails swapChainSupport =
        querySwapChainSupport(device.getPhysicalDevice(), device.getSurface());
    if (swapChainSupport.result != VK_SUCCESS)
    {
        return swapChainSupport.result;
    }
    if (swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty())
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = device.getSurface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1; // always 1
    createInfo.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // direct render to images, if you want to like a
                                             // PostProcessing, then use
                                             // VK_IMAGE_USAGE_TRANSFER_DST_BIT flag.

    jhb::Device::QueueFamilyIndexes indices = device.findQueueFamilies(device.getPhysicalDevice());
    if (!indices.graphicsFamily || !indices.presentFamily)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        // right now just using a VK_SHARING_MODE_CONCURRENT
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;     // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // ignore alpha channl
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE; // dont care about obscured pixes color
    createInfo.oldSwapchain = oldSwapchain == nullptr
                                  ? VK_NULL_HANDLE
                                  : oldSwapchain->swapChain; // TODO : cause of window resizing

    VkResult result =
        vkCreateSwapchainKHR(device.getLogicalDevice(), &createInfo, nullptr, &swapChain);
    if (result != VK_SUCCESS)
    {
        return result;
    }

    // retrieve swapchain images
    result = vkGetSwapchainImagesKHR(device.getLogicalDevice(), swapChain, &imageCount, nullptr);
    if (result != VK_SUCCESS)
    {
        return result;
    }
    if (imageCount == 0)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    swapChainImages.resize(imageCount);
    result = vkGetSwapchainImagesKHR(device.getLogicalDevice(), swapChain, &imageCount,
                                     swapChainImages.data());
    if (result != VK_SUCCESS)
    {
        return result;
    }
    swapChainImages.resize(imageCount);

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
    return VK_SUCCESS;
}

VkResult SwapChain::createImageViews()
{
    swapChainImageviews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapChainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapChainImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        const auto result = vkCreateImageView(device.getLogicalDevice(), &viewInfo, nullptr,
                                              &swapChainImageviews[i]);
        if (result != VK_SUCCESS)
        {
            return result;
        }
    }
    return VK_SUCCESS;
}

VkResult SwapChain::createSyncObjects()
{
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(swapChainImages.size());
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    computeFences.resize(MAX_FRAMES_IN_FLIGHT);
    computeSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        auto result = vkCreateSemaphore(device.getLogicalDevice(), &semaphoreInfo, nullptr,
                                        &imageAvailableSemaphores[i]);
        if (result != VK_SUCCESS)
        {
            return result;
        }
        result = vkCreateFence(device.getLogicalDevice(), &fenceInfo, nullptr, &inFlightFences[i]);
        if (result != VK_SUCCESS)
        {
            return result;
        }
        result = vkCreateSemaphore(device.getLogicalDevice(), &semaphoreInfo, nullptr,
                                   &computeSemaphores[i]);
        if (result != VK_SUCCESS)
        {
            return result;
        }
        result = vkCreateFence(device.getLogicalDevice(), &fenceInfo, nullptr, &computeFences[i]);
        if (result != VK_SUCCESS)
        {
            return result;
        }
    }
    for (auto &semaphore : renderFinishedSemaphores)
    {
        const auto result =
            vkCreateSemaphore(device.getLogicalDevice(), &semaphoreInfo, nullptr, &semaphore);
        if (result != VK_SUCCESS)
        {
            return result;
        }
    }
    return VK_SUCCESS;
}

jhb::SwapChain::SwapChainSupportDetails SwapChain::querySwapChainSupport(VkPhysicalDevice device,
                                                                         VkSurfaceKHR surface)
{
    SwapChainSupportDetails details;

    // process all three properties of detatils below section
    details.result =
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
    if (details.result != VK_SUCCESS)
    {
        return details;
    }

    uint32_t formatCount = 0;
    details.result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (details.result != VK_SUCCESS)
    {
        return details;
    }

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        details.result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                                              details.formats.data());
        if (details.result != VK_SUCCESS)
        {
            return details;
        }
        details.formats.resize(formatCount);
    }

    uint32_t presentModeCount = 0;
    details.result =
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (details.result != VK_SUCCESS)
    {
        return details;
    }

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        details.result = vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface, &presentModeCount, details.presentModes.data());
        if (details.result != VK_SUCCESS)
        {
            return details;
        }
        details.presentModes.resize(presentModeCount);
    }

    return details;
}

VkSurfaceFormatKHR
SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats)
{
    // 1순위: B8G8R8A8_SRGB (PC에서 일반적)
    for (const auto &availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }
    // 2순위: R8G8B8A8_UNORM (Android에서 일반적)
    for (const auto &availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }
    // fallback: 첫 번째 지원 포맷 사용
    return availableFormats[0];
}

VkPresentModeKHR
SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes)
{
    // if performence strong enough, then use VK_PRESENT_MODE_MAIL_BOX_KHR. there are no tearing or
    // another problems
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilites)
{
    // about pixels and Coordinate Systems
    // glfw has two primary coordinate systems : virtual secreen and window content area
    // both sue the same unit : virtual screen coordinates (doesn't correspond to pixels)
    // but vulkan works with pixels not screen coordinates, so you must convert coordinates to
    // pixels
    if (capabilites.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilites.currentExtent;
    }
    else
    {
        int width, height;
#ifdef ANDROID
        width = static_cast<int>(device.getWindow().getExtent().width);
        height = static_cast<int>(device.getWindow().getExtent().height);
#else
        glfwGetFramebufferSize(&device.getWindow().GetGLFWwindow(), &width, &height);
#endif

        VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        actualExtent.width = std::clamp(actualExtent.width, capabilites.minImageExtent.width,
                                        capabilites.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilites.minImageExtent.height,
                                         capabilites.maxImageExtent.height);

        return actualExtent;
    }
}
} // namespace jhb
