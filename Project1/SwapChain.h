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
		static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
		SwapChain(Device& device, VkExtent2D extent);
		SwapChain(Device& device, VkExtent2D extent, std::shared_ptr<SwapChain> prevSwapchain);
		~SwapChain();

		void init();

		static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
		VkExtent2D getSwapChainExtent() { return swapChainExtent; }
		VkFormat getSwapChainImageFormat() { return swapChainImageFormat; }
		VkFramebuffer getFrameBuffer(int index) { return swapChainFramebuffers[index]; }

		VkRenderPass getRenderPass() { return renderPass; }
		size_t getImageCount() { return swapChainImages.size(); }
		VkResult acquireNextImage(uint32_t* imageIndex);

		VkFormat findDepthFormat();

		VkResult submitCommandBuffers(const VkCommandBuffer* buffers, uint32_t* imageIndex);

	private:
		void createSwapChain();
		void createRenderPass();
		void createFrameBuffers();
		void createDepthResources();
		void createImageViews();
		void createSyncObjects();

		VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilites);

	private:
		Device& device;
		VkExtent2D swapChainExtent;

		VkSwapchainKHR swapChain;
		VkFormat swapChainImageFormat;
		VkRenderPass renderPass;

		std::shared_ptr<SwapChain> oldSwapchain;

		std::vector<VkFramebuffer> swapChainFramebuffers;
		std::vector<VkImage> swapChainImages; // images created by swapchain config
		std::vector<VkImageView> swapChainImageviews;
		std::vector<VkImage> depthImages;
		std::vector<VkDeviceMemory> depthImageMemorys;
		std::vector<VkImageView> depthImageViews;

		std::vector<VkSemaphore> imageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> inFlightFences;
		std::vector<VkFence> imagesInFlight;

		size_t currentFrame = 0;
	};
}

