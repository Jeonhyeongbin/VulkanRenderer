#pragma once
#define GLM_FORCE_RADIANS // not use degree;
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "Window.h"
#include "Device.h"
#include "SwapChain.h"
#include "Model.h"

#include <stdint.h>
#include <cassert>

namespace jhb {
	class Renderer { //manage swap chain and render pass
	public:
		Renderer(Window& window, Device& device, const std::vector<VkSubpassDependency>& dependencies, bool shouldSwapchainCreate, VkFormat format, int attachmentCount);
		Renderer(Window& window, Device& device, const std::vector<VkSubpassDependency>& dependencies, 
			VkExtent2D extent, bool shouldSwapchainCreate, VkFormat format, int attachmentCount);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer(Renderer&&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		// application need to access render pass
		// and pipe line also
		VkRenderPass getSwapChainRenderPass() const { return swapChain->getRenderPass(); }
		float getAspectRatio() const { return swapChain->extentAspectRatio(); }

		VkCommandBuffer beginFrame();
		void endFrame();
		void beginSwapChainRenderPass(VkCommandBuffer commandBuffer, VkFramebuffer frameBuffer, uint32_t width, uint32_t height);
		void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
		void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

		bool isFrameInProgress() const { return isFrameStarted; }
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

	private:
		void createCommandBuffers();
		void freeCommandBuffers();
		void recreateSwapChain(const std::vector<VkSubpassDependency>& dependencies, VkExtent2D extent, bool shouldSwapchainCreate, VkFormat format, int attachmentCount);

		// init top to bottom
		Window& window;
		Device& device;
		VkExtent2D extent;

		std::unique_ptr<SwapChain> swapChain;
		std::vector<VkCommandBuffer> commandBuffers;
		std::vector<VkSubpassDependency> dependencies;

		uint32_t currentImageIndex;
		int currentFrameIndex;
		bool isFrameStarted;
	};
}