#include "Renderer.h"
#include <memory>

namespace jhb {
	Renderer::Renderer(Window& window, Device& device, const std::vector<VkSubpassDependency>& dependencies, bool shouldSwapchainCreate, VkFormat format, int attachmentCount) : window{ window }, device{ device }, dependencies{ dependencies } {
		recreateSwapChain(dependencies, window.getExtent(), shouldSwapchainCreate, format, attachmentCount);
		createCommandBuffers();
#ifdef ANDROID
		// 스왑체인이 올바른 크기로 생성됐으므로, Window 생성자에서 SetNativeWindow 호출 시
		// 설정된 framebufferResized 플래그를 초기화한다.
		// 이 초기화를 하지 않으면 Run() 첫 프레임에서 불필요한 스왑체인 재생성이 일어나고,
		// "freeAllBuffers: buffers were freed while being dequeued!" 경고 후 SIGSEGV가 발생한다.
		window.resetWindowResizedFlag();
#endif
	}

	Renderer::Renderer(Window& window, Device& device, const std::vector<VkSubpassDependency>& dependencies, VkExtent2D extent, bool shouldSwapchainCreate, VkFormat format, int attachmentCount) : window{ window }, device{ device }, dependencies{ dependencies }, extent{extent}
	{
		recreateSwapChain(dependencies, extent, shouldSwapchainCreate, format, attachmentCount);
		createCommandBuffers();
#ifdef ANDROID
		window.resetWindowResizedFlag();
#endif
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
			VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}

	void Renderer::freeCommandBuffers()
	{
		vkFreeCommandBuffers(device.getLogicalDevice(), device.getCommnadPool(), static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
		commandBuffers.clear();
	}

	void Renderer::recreateSwapChain(const std::vector<VkSubpassDependency>& dependencies, VkExtent2D _extent, bool shouldSwapchainCreate, VkFormat format, int attachmentCount)
	{
		extent = _extent;
		while (extent.width == 0 || extent.height == 0)
		{
			extent = window.getExtent();
#ifndef ANDROID
			glfwWaitEvents();
#endif
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

	void Renderer::excuteComputeDispatch(const VkCommandBuffer* buffers)
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
			// Android에서 "freeAllBuffers: buffers were freed while being dequeued!" + SIGSEGV가 발생한다.
			window.resetWindowResizedFlag();
			recreateSwapChain(dependencies, window.getExtent(), true, VK_FORMAT_R16G16B16A16_SFLOAT, 2);
			return nullptr;
		}

		auto result = swapChain->acquireNextImage(&currentImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			// OUT_OF_DATE: 이미지가 획득되지 않았으므로 즉시 재생성 가능
			// window.getExtent()로 최신 크기를 전달하고 플래그도 함께 리셋한다.
			window.resetWindowResizedFlag();
			recreateSwapChain(dependencies, window.getExtent(), true, VK_FORMAT_R16G16B16A16_SFLOAT, 2);
			return nullptr;
		}

		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		// VK_SUBOPTIMAL_KHR: 이미지는 획득됨 — 이번 프레임을 그대로 진행하고
		// endFrame()에서 present 후 재생성한다.
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
			window.resetWindowResizedFlag();
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
		// 스왑체인 프레임버퍼는 currentImageIndex(획득한 이미지)로 선택해야 한다.
		// currentFrameIndex(0 또는 1)를 사용하면 Android처럼 스왑체인 이미지가 3개일 때
		// 잘못된 프레임버퍼에 렌더링하여 검은 화면이 발생한다.
		beginSwapChainRenderPass(commandBuffer, swapChain->getRenderPass(), swapChain->getFrameBuffer(currentImageIndex), swapChain->getSwapChainExtent());
	}

	void Renderer::beginSwapChainRenderPassWithMouseCoordinate(VkCommandBuffer commandBuffer, VkRenderPass renderPass, VkFramebuffer frameBuffer, VkExtent2D _extent, float x, float y)
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

	bool Renderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer, VkRenderPass renderPass, VkFramebuffer frameBuffer, VkExtent2D _extent, int attachmentCount)
	{
		assert(isFrameStarted && "Can't call beginSwapChainRenderPass while frame is not in progress!!");
		assert(commandBuffer == getCurrentCommandBuffer() && "Can't begining render pass on command buffer from a different frame!");

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = frameBuffer;

		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { _extent.width, _extent.height };
		if (extent.width != _extent.width || extent.height != _extent.height)
		{
			//rectea
			extent = _extent;
			return false;
		}

		std::vector<VkClearValue> clearValues(attachmentCount);
		for (int i = 0; i < attachmentCount; i++)
		{
			if (i == 6)
			{
				clearValues[i].depthStencil = { 1.0f, 0 };
			}
			else {
				clearValues[i].color = { 0, 0, 0, 1 };
			}
		}
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		// framebuffer는 _extent 기준으로 생성됐으므로 뷰포트/시저도 _extent로 통일한다.
		viewport.width = static_cast<float>(_extent.width);
		viewport.height = static_cast<float>(_extent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor{ {0, 0}, {_extent.width, _extent.height} };
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
		return true;
	}

	void Renderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer)
	{
		assert(isFrameStarted && "Can't call endSwapChainRenderPass if frame is not in progress!!");
		assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame!");

		vkCmdEndRenderPass(commandBuffer);
	}
}
