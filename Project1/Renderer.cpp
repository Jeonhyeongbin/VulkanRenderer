#include "Renderer.h"
#include <memory>
#include <array>

namespace jhb {
	Renderer::Renderer(Window& window, Device& device, const std::vector<VkSubpassDependency>& dependencies, bool shouldSwapchainCreate, VkFormat format, int attachmentCount) : window{ window }, device{ device }, dependencies{ dependencies } {
		recreateSwapChain(dependencies, window.getExtent(), shouldSwapchainCreate, format, attachmentCount);
		createCommandBuffers();
	}

	Renderer::Renderer(Window& window, Device& device, const std::vector<VkSubpassDependency>& dependencies, VkExtent2D extent, bool shouldSwapchainCreate, VkFormat format, int attachmentCount) : window{ window }, device{ device }, dependencies{ dependencies }, extent{extent}
	{
		recreateSwapChain(dependencies, extent, shouldSwapchainCreate, format, attachmentCount);
		createCommandBuffers();
	}

	Renderer::~Renderer()
	{
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
			VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}

	void Renderer::freeCommandBuffers()
	{
		vkFreeCommandBuffers(device.getLogicalDevice(), device.getCommnadPool(), static_cast<float>(commandBuffers.size()), commandBuffers.data());
		commandBuffers.clear();
	}

	void Renderer::recreateSwapChain(const std::vector<VkSubpassDependency>& dependencies, VkExtent2D _extent, bool shouldSwapchainCreate, VkFormat format, int attachmentCount)
	{
		extent = _extent;
		while (extent.width == 0 || extent.height == 0)
		{
			extent = window.getExtent();
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(device.getLogicalDevice()); // wait until current swapchain is no longer being used

		if (swapChain == nullptr)
		{
			swapChain = std::make_unique<SwapChain>(device, extent, dependencies, shouldSwapchainCreate, format, attachmentCount);
		}
		else {
			std::shared_ptr<SwapChain> oldSwapChain = std::move(swapChain);
			swapChain = std::make_unique<SwapChain>(device, extent, oldSwapChain, dependencies, shouldSwapchainCreate, format, attachmentCount);

			if (!oldSwapChain->compareSwapChainFormats(*swapChain.get()))
			{
				throw std::runtime_error("Swap chain image(or depth) format has changed!!");
			}

		}

		// if new swap chain created, there is no longer create new commandbuffer
		// if render pass compatible than pipeline doesn't need to recreate
		// todo
	}

	VkCommandBuffer Renderer::beginFrame()
	{
		assert(!isFrameStarted && "Can't call beginFrame while already in progress");

		auto result = swapChain->acquireNextImage(&currentImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			// right after window resized
			recreateSwapChain(dependencies, extent, true, VK_FORMAT_R16G16B16A16_SFLOAT, 2);
			return nullptr;
		}

		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		isFrameStarted = true;
		auto commandBuffer = getCurrentCommandBuffer();

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}
		return commandBuffer;
	}

	void Renderer::endFrame()
	{
		assert(isFrameStarted && "Can't call endFrame while frame is not in progress!!");
		auto commandBuffer = getCurrentCommandBuffer();

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to record command buffer!");
		}

		auto result = swapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.wasWindowResized())
		{
			//window.resetWindowResizedFlag();
			recreateSwapChain(dependencies, extent, true, VK_FORMAT_R16G16B16A16_SFLOAT, 2);
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to present swap chain image!");
		}

		isFrameStarted = false;
		currentFrameIndex = (currentFrameIndex + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
	}

	void Renderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer)
	{
		beginSwapChainRenderPass(commandBuffer, swapChain->getRenderPass(), swapChain->getFrameBuffer(currentFrameIndex), swapChain->getSwapChainExtent());
	}

	void Renderer::beginSwapChainRenderPassWithMouseCoordinate(VkCommandBuffer commandBuffer, VkRenderPass renderPass, VkFramebuffer frameBuffer, VkExtent2D extent, float x, float y)
	{
		assert(isFrameStarted && "Can't call beginSwapChainRenderPass while frame is not in progress!!");
		assert(commandBuffer == getCurrentCommandBuffer() && "Can't begining render pass on command buffer from a different frame!");

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = frameBuffer;

		float halfWidth = extent.width / 2;
		float halfHeight = extent.height / 2;
		if (x - halfWidth < 0)
		{
			x = halfWidth;
		}
		if (y - halfHeight < 0)
		{
			y = halfHeight;
		}
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { extent.width, extent.height };

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { 0.2f, 0.2f, 0.2f, 1.f };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.x = x - halfWidth;
		viewport.y = y - halfHeight;
		viewport.width = static_cast<float>(extent.width);
		viewport.height = static_cast<float>(extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor{ {0, 0}, {extent.width, extent.height} };
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	void Renderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer, VkRenderPass renderPass, VkFramebuffer frameBuffer, VkExtent2D extent)
	{
		assert(isFrameStarted && "Can't call beginSwapChainRenderPass while frame is not in progress!!");
		assert(commandBuffer == getCurrentCommandBuffer() && "Can't begining render pass on command buffer from a different frame!");

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = frameBuffer;

		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { extent.width, extent.height };

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { 0, 0, 0, 1 };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(swapChain->getSwapChainExtent().width);
		viewport.height = static_cast<float>(swapChain->getSwapChainExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor{ {0, 0}, {swapChain->getSwapChainExtent().width, swapChain->getSwapChainExtent().height} };
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	void Renderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer)
	{
		assert(isFrameStarted && "Can't call endSwapChainRenderPass if frame is not in progress!!");
		assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame!");

		vkCmdEndRenderPass(commandBuffer);
	}
}