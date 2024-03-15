#include "SwapChain.h"

namespace jhb {
	SwapChain::SwapChain(Device& device, VkExtent2D extent)
		:device{ device }, swapChainExtent {extent}
	{
		createSwapChain();
	}

	SwapChain::~SwapChain()
	{
		if (swapChain != nullptr) {
			vkDestroySwapchainKHR(device.getLogicalDevice(), swapChain, nullptr);
			swapChain = nullptr;
		}

		for (auto imageView : swapChainImageviews) {
			vkDestroyImageView(device.getLogicalDevice(), imageView, nullptr);
		}

	}

	void SwapChain::createSwapChain()
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device.getPhysicalDevice(), device.getSurface());

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = device.getSurface();
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1; //always 1
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // direct render to images, if want to like a PostProcessing, then use VK_IMAGE_USAGE_TRANSFER_DST_BIT flag.

		jhb::Device::QueueFamilyIndexes indices = device.findQueueFamilies(device.getPhysicalDevice());
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		if (indices.graphicsFamily != indices.presentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			// right now just using a VK_SHARING_MODE_CONCURRENT
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // ignore alpha channl
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE; // dont care about obscured pixes color
		createInfo.oldSwapchain = VK_NULL_HANDLE; // TODO : cause of window resizing

		if (vkCreateSwapchainKHR(device.getLogicalDevice(), &createInfo, nullptr, &swapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create swap chain!");
		}

		// retrieve swapchain images
		vkGetSwapchainImagesKHR(device.getLogicalDevice(), swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device.getLogicalDevice(), swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;

		swapChainImageviews.resize(swapChainImages.size());
		for (size_t i = 0; i < swapChainImages.size(); i++) {
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = swapChainImageFormat;

			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(device.getLogicalDevice(), &createInfo, nullptr, &swapChainImageviews[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create image views!");
			}
		}
	}

	jhb::SwapChain::SwapChainSupportDetails SwapChain::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		SwapChainSupportDetails details;

		// process all three properties of detatils below section
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		// about format and color space
		// format : color channels and types(what types??)
		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}
	}

	VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		// if performence strong enough, then use VK_PRESENT_MODE_MAIL_BOX_KHR. there are no tearing or another problems
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilites)
	{
		// about pixels and Coordinate Systems
		// glfw has two primary coordinate systems : virtual secreen and window content area
		// both sue the same unit : virtual screen coordinates (doesn't correspond to pixels)
		// but vulkan works with pixels not screen coordinates, so you must convert coordinates to pixels
		if (capabilites.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilites.currentExtent;
		}
		else {
			int width, height;
			glfwGetFramebufferSize(&device.getWindow().GetGLFWwindow(), &width, &height);

			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilites.minImageExtent.width, capabilites.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilites.minImageExtent.height, capabilites.maxImageExtent.height);

			return actualExtent;
		}
	}
}