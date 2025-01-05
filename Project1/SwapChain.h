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
		static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
		SwapChain(Device& device, VkExtent2D extent, const std::vector<VkSubpassDependency>& dependencies, bool shouldSwapChainCreate, VkFormat format, int attachmentCount);
		SwapChain(Device& device, VkExtent2D extent, std::shared_ptr<SwapChain> prevSwapchain, const std::vector<VkSubpassDependency>& dependencies, bool shouldSwapChainCreate, VkFormat format, int attachmentCount);
		~SwapChain();

		void init();

		static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
		VkExtent2D getSwapChainExtent() { return swapChainExtent; }
		VkFormat getSwapChainImageFormat() { return swapChainImageFormat; }
		VkFramebuffer getFrameBuffer(int index) { return swapChainFramebuffers[index]; }
		float extentAspectRatio() {
			return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
		}
		VkImageView getSwapChianImageView(int index) const {
			return swapChainImageviews[index];
		}

		const std::vector<VkImageView>& getSwapChianImageViews() const {
			return swapChainImageviews;
		}
		VkRenderPass getRenderPass() { return renderPass; }
		size_t getImageCount() { return swapChainImages.size(); }
		VkResult acquireNextImage(uint32_t* imageIndex);

		VkFormat findDepthFormat();

		VkResult submitCommandBuffers(const VkCommandBuffer* buffers, uint32_t* imageIndex);
		VkResult submitComputeCommandBuffers(const VkCommandBuffer* buffers, uint32_t* frameIndex);

		bool compareSwapChainFormats(const SwapChain& swapChain) const {
			return swapChain.swapChainDepthFormat == swapChainDepthFormat && swapChain.swapChainImageFormat == swapChainImageFormat;
		}

	private:
		void createSwapChain();
		void createRenderPass(const std::vector<VkSubpassDependency>& dependencies, VkFormat format, int attachmentCount);
		void createFrameBuffers();
		void createDepthResources();
		void createImageViews();
		void createSyncObjects();
		void createColorResources();

		VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilites);
	public:
		static VkFormat swapChainImageFormat;

	private:
		Device& device;
		VkExtent2D swapChainExtent;

		VkSwapchainKHR swapChain;
		VkFormat swapChainDepthFormat;
		VkRenderPass renderPass;

		std::shared_ptr<SwapChain> oldSwapchain;

		std::vector<VkFramebuffer> swapChainFramebuffers;
		std::vector<VkImage> swapChainImages; // images created by swapchain config
		std::vector<VkImageView> swapChainImageviews;
		std::vector<VkImage> depthImages;
		std::vector<VkDeviceMemory> depthImageMemorys;
		std::vector<VkImageView> depthImageViews;
		std::vector<VkImage> colorImage; 
		std::vector<VkDeviceMemory> colorImageMemory;
		std::vector<VkImageView> colorImageView;

	public:
		std::vector<VkSemaphore> imageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> inFlightFences;
		std::vector<VkFence> imagesInFlight;

		std::vector<VkSemaphore> computeSemaphores;
		std::vector<VkFence> computeFences;

		size_t currentFrame = 0;
	};
}

