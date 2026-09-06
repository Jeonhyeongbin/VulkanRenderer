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
		SwapChain& GetSwapChain() { return *swapChain; }
		VkRenderPass getSwapChainRenderPass() const { return swapChain->getRenderPass(); }
		VkImageView getSwapChainImageView(int index) { return swapChain->getSwapChianImageView(index); }
		const std::vector<VkImageView>& getSwapChainImageViews() const { return swapChain->getSwapChianImageViews(); }
		float getAspectRatio() const { return swapChain->extentAspectRatio(); }
		void setWindowExtent(VkExtent2D _extent) { extent = _extent; }

		void excuteComputeDispatch(const VkCommandBuffer* buffers);

		VkCommandBuffer beginFrame();
		void endFrame();
		bool beginSwapChainRenderPass(VkCommandBuffer commandBuffer, VkRenderPass renderPass, VkFramebuffer frameBuffer, VkExtent2D extent, int attachmentCount = 2);
		void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
		void beginSwapChainRenderPassWithMouseCoordinate(VkCommandBuffer commandBuffer, VkRenderPass renderPass, VkFramebuffer frameBuffer, VkExtent2D extent, float x, float y);
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
		void recreateSwapChain(const std::vector<VkSubpassDependency>& dependencies, VkExtent2D extent, bool shouldSwapchainCreate, VkFormat format, int attachmentCount);

		// init top to bottom
		Window& window;
		Device& device;
		VkExtent2D extent;

		std::unique_ptr<SwapChain> swapChain;
		std::vector<VkCommandBuffer> commandBuffers;
		std::vector<VkSubpassDependency> dependencies;

		uint32_t currentImageIndex{0};
		int currentFrameIndex{0};
		bool isFrameStarted{false};
	};
}
