#pragma once

#include "Device.h"
#include <vulkan/vulkan.h>

namespace jhb {
	class SwapChain
	{
	public:
		struct SwapChainSupportDetails {
			VkSurfaceCapabilitiesKHR capabilities;
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR> presentModes;
		};

	public:
		SwapChain(Device& device, VkExtent2D extent);
		~SwapChain();

		static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);


	private:
		void createSwapChain();
		VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilites);

	private:
		Device& device;
		VkExtent2D swapChainExtent;

		VkSwapchainKHR swapChain;
		VkFormat swapChainImageFormat;
		std::vector<VkImage> swapChainImages; // images created by swapchain config
		std::vector<VkImageView> swapChainImageviews;
	};
}

