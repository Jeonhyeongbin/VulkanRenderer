#include "SwapChain.h"
#include <array>

namespace jhb {
	VkFormat SwapChain::swapChainImageFormat;
	SwapChain::SwapChain(Device& device, VkExtent2D extent, const std::vector<VkSubpassDependency>& dependencies, bool shouldSwapChainCreate, VkFormat format, int attachmentCount)
		:device{ device }, swapChainExtent{ extent }
	{
		if (shouldSwapChainCreate)
		{
			createSwapChain();
			format = swapChainImageFormat;
		}
		createRenderPass(dependencies, format, attachmentCount);
		init();
		oldSwapchain = nullptr; // clean up  old swap chain since it's no longer needed
	}

	SwapChain::SwapChain(Device& device, VkExtent2D extent, std::shared_ptr<SwapChain> prevSwapchain, const std::vector<VkSubpassDependency>& dependencies, bool shouldSwapChainCreate, VkFormat format, int attachmentCount)
		:device{ device }, swapChainExtent{ extent }, oldSwapchain(prevSwapchain)
	{
		if (shouldSwapChainCreate)
		{
			createSwapChain();
			format = swapChainImageFormat;
		}
		createRenderPass(dependencies, format, attachmentCount);
		init();
		oldSwapchain = nullptr; // clean up  old swap chain since it's no longer needed
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

		for (int i = 0; i < colorImage.size(); i++) {
			vkDestroyImageView(device.getLogicalDevice(), colorImageView[i], nullptr);
			vkDestroyImage(device.getLogicalDevice(), colorImage[i], nullptr);
			vkFreeMemory(device.getLogicalDevice(), colorImageMemory[i], nullptr);
		}

		for (int i = 0; i < depthImages.size(); i++) {
			vkDestroyImageView(device.getLogicalDevice(), depthImageViews[i], nullptr);
			vkDestroyImage(device.getLogicalDevice(), depthImages[i], nullptr);
			vkFreeMemory(device.getLogicalDevice(), depthImageMemorys[i], nullptr);
		}

		for (auto framebuffer : swapChainFramebuffers) {
			vkDestroyFramebuffer(device.getLogicalDevice(), framebuffer, nullptr);
		}

		vkDestroyRenderPass(device.getLogicalDevice(), renderPass, nullptr);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(device.getLogicalDevice(), imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(device.getLogicalDevice(), inFlightFences[i], nullptr);
		}

	}

	void SwapChain::init()
	{
		createImageViews();
		createColorResources();
		createDepthResources();
		createFrameBuffers();
		createSyncObjects();
	}

	VkResult SwapChain::acquireNextImage(uint32_t* imageIndex)
	{
		vkWaitForFences(
			device.getLogicalDevice(),
			1,
			&inFlightFences[currentFrame],
			VK_TRUE,
			std::numeric_limits<uint64_t>::max());

		VkResult result = vkAcquireNextImageKHR(
			device.getLogicalDevice(),
			swapChain,
			std::numeric_limits<uint64_t>::max(),
			imageAvailableSemaphores[currentFrame],  // must be a not signaled semaphore
			VK_NULL_HANDLE,
			imageIndex);

		return result;
	}

	VkFormat SwapChain::findDepthFormat()
	{
		return device.findSupportedFormat(
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

	VkResult SwapChain::submitCommandBuffers(const VkCommandBuffer* buffers, uint32_t* imageIndex)
	{
		if (imagesInFlight[*imageIndex] != VK_NULL_HANDLE) {
			vkWaitForFences(device.getLogicalDevice(), 1, &imagesInFlight[*imageIndex], VK_TRUE, UINT64_MAX);
		}
		imagesInFlight[*imageIndex] = inFlightFences[currentFrame];

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame], computeSemaphores[currentFrame]};
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT , VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = buffers;

		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkResetFences(device.getLogicalDevice(), 1, &inFlightFences[currentFrame]);
		if (vkQueueSubmit(device.getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) !=
			VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = imageIndex;

		auto result = vkQueuePresentKHR(device.getPresentQueue(), &presentInfo);

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

		return result;
	}

	VkResult SwapChain::submitComputeCommandBuffers(const VkCommandBuffer* buffers, uint32_t* frameIndex)
	{
		vkWaitForFences(device.getLogicalDevice(), 1, &computeFences[currentFrame], VK_TRUE, UINT64_MAX);
		vkResetFences(device.getLogicalDevice(), 1, &computeFences[currentFrame]);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = buffers;

		VkSemaphore signalSemaphores[] = { computeSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		VkResult ret = vkQueueSubmit(device.getComputeQueue(), 1, &submitInfo, nullptr);
		if (ret != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}
		return ret;
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
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // direct render to images, if you want to like a PostProcessing, then use VK_IMAGE_USAGE_TRANSFER_DST_BIT flag.

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
		createInfo.oldSwapchain = oldSwapchain == nullptr ? VK_NULL_HANDLE : oldSwapchain->swapChain; // TODO : cause of window resizing

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
	}

	void SwapChain::createRenderPass(const std::vector<VkSubpassDependency>& dependencies, VkFormat format, int attachmentCount)
	{
		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = findDepthFormat();
		depthAttachment.samples = device.msaaSamples;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = format;
		colorAttachment.samples = device.msaaSamples;

		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription colorAttachmentResolve{};
		colorAttachmentResolve.format = swapChainImageFormat;
		colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentResolveRef{};
		colorAttachmentResolveRef.attachment = 2;
		colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		subpass.colorAttachmentCount = 1;
		subpass.pResolveAttachments = &colorAttachmentResolveRef;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		std::vector<VkAttachmentDescription> attachments;
		if (attachmentCount == 1)
		{
			attachments.push_back(colorAttachment);
		}
		else {
			attachments.push_back(colorAttachment);
			attachments.push_back(depthAttachment);
			attachments.push_back(colorAttachmentResolve);
		}

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = dependencies.size();
		renderPassInfo.pDependencies = dependencies.data();

		if (vkCreateRenderPass(device.getLogicalDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
			throw std::runtime_error("failed to create render pass!");
		}
	}

	void SwapChain::createFrameBuffers()
	{
		swapChainFramebuffers.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImageviews.size(); i++) {
			std::array<VkImageView, 3> attachments = { colorImageView[i], depthImageViews[i], swapChainImageviews[i]};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(device.getLogicalDevice(), &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}

	void SwapChain::createDepthResources()
	{
		VkFormat depthFormat = findDepthFormat();
		swapChainDepthFormat = depthFormat;
		VkExtent2D swapChainExtent = getSwapChainExtent();

		auto imageCount = swapChainImages.size();
		depthImages.resize(imageCount);
		depthImageMemorys.resize(imageCount);
		depthImageViews.resize(imageCount);

		for (int i = 0; i < depthImages.size(); i++) {
			VkImageCreateInfo imageInfo{};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent.width = swapChainExtent.width;
			imageInfo.extent.height = swapChainExtent.height;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.format = depthFormat;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			imageInfo.samples = device.msaaSamples;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.flags = 0;

			device.createImageWithInfo(
				imageInfo,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				depthImages[i],
				depthImageMemorys[i]);

			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = depthImages[i];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = depthFormat;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(device.getLogicalDevice(), &viewInfo, nullptr, &depthImageViews[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create texture image view!");
			}
		}
	}

	void SwapChain::createImageViews()
	{
		swapChainImageviews.resize(swapChainImages.size());
		for (size_t i = 0; i < swapChainImages.size(); i++) {
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

			if (vkCreateImageView(device.getLogicalDevice(), &viewInfo, nullptr, &swapChainImageviews[i]) !=
				VK_SUCCESS) {
				throw std::runtime_error("failed to create texture image view!");
			}
		}
	}

	void SwapChain::createSyncObjects()
	{
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
		imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);


		computeFences.resize(MAX_FRAMES_IN_FLIGHT);
		computeSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			if (vkCreateSemaphore(device.getLogicalDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) !=
				VK_SUCCESS || 
				vkCreateSemaphore(device.getLogicalDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) !=
				VK_SUCCESS ||
				vkCreateFence(device.getLogicalDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS
				|| vkCreateSemaphore(device.getLogicalDevice(), &semaphoreInfo, nullptr, &computeSemaphores[i]) !=
				VK_SUCCESS
				|| vkCreateFence(device.getLogicalDevice(), &fenceInfo, nullptr, &computeFences[i]) != VK_SUCCESS
				) {
				throw std::runtime_error("failed to create synchronization objects for a frame!");
			}
		}

	}

	void SwapChain::createColorResources()
	{
		colorImage.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
		colorImageView.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
		colorImageMemory.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
		VkFormat colorFormat = swapChainImageFormat;
		for (int i = 0; i < colorImage.size(); i++)
		{
			VkImageCreateInfo imageCI{};
			imageCI.imageType = VK_IMAGE_TYPE_2D;
			imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageCI.format = colorFormat;
			imageCI.extent.width = swapChainExtent.width;
			imageCI.extent.height = swapChainExtent.height;
			imageCI.extent.depth = 1;
			imageCI.mipLevels = 1;
			imageCI.arrayLayers = 1;
			imageCI.samples = device.msaaSamples;
			imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
			device.createImageWithInfo(imageCI, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorImage[i], colorImageMemory[i]);
			// Image view
			VkImageViewCreateInfo viewCI{};
			viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewCI.format = colorFormat;
			viewCI.subresourceRange = {};
			viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewCI.subresourceRange.levelCount = 1;
			viewCI.subresourceRange.layerCount = 1;
			viewCI.image = colorImage[i];
			if (vkCreateImageView(device.getLogicalDevice(), &viewCI, nullptr, &colorImageView[i]))
			{
				throw std::runtime_error("failed to create ImageView!");
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