#include "ShadowRenderSystem.h"
#include <memory>
#include <array>

namespace jhb {
	ShadowRenderSystem::ShadowRenderSystem(Device& device, const std::string& vert, const std::string& frag)
		:  BaseRenderSystem(device)
	{
		BaseRenderSystem::createPipeLineLayout({ initializeOffScreenDescriptor() }, { VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(OffscreenConstant) },  VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, sizeof(OffscreenConstant), sizeof(glm::mat4) } });
		createOffscreenRenderPass();
		createPipeline(offScreenRenderPass, vert, frag);
		createShadowCubeMap();
		createOffscreenFrameBuffer();
	}

	ShadowRenderSystem::~ShadowRenderSystem()
	{

	}

	void ShadowRenderSystem::createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag)
	{
		assert(pipelineLayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

		PipelineConfigInfo pipelineConfig{};
		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.depthStencilInfo.depthWriteEnable = VK_TRUE;
		pipelineConfig.depthStencilInfo.depthTestEnable = VK_TRUE;
		pipelineConfig.attributeDescriptions = jhb::Vertex::getAttrivuteDescriptions();
		pipelineConfig.bindingDescriptions = jhb::Vertex::getBindingDescriptions();

		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;

		pipeline = std::make_unique<Pipeline>(
			device,
			vert,
			frag,
			pipelineConfig);
	}

	void ShadowRenderSystem::createOffscreenFrameBuffer()
	{
		VkFormat validDepthFormat = device.findSupportedFormat({ VK_FORMAT_D32_SFLOAT_S8_UINT,
	VK_FORMAT_D32_SFLOAT,
	VK_FORMAT_D24_UNORM_S8_UINT,
	VK_FORMAT_D16_UNORM_S8_UINT,
	VK_FORMAT_D16_UNORM
			}, VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		VkImageCreateInfo imageCIa{};
		imageCIa.imageType = VK_IMAGE_TYPE_2D;
		imageCIa.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCIa.format = validDepthFormat;
		imageCIa.extent = { offscreenImageSize.width, offscreenImageSize.height, 1 };
		imageCIa.mipLevels = 1;
		imageCIa.arrayLayers = 1;
		imageCIa.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCIa.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCIa.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

		device.createImageWithInfo(imageCIa, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offScreen.image, offScreen.memory);
		// Image view
		VkImageViewCreateInfo viewCIa{};
		viewCIa.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCIa.format = validDepthFormat;
		viewCIa.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCIa.subresourceRange = {};
		viewCIa.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		viewCIa.subresourceRange.baseArrayLayer = 0;
		viewCIa.subresourceRange.baseMipLevel = 0;
		viewCIa.subresourceRange.levelCount = 1;
		viewCIa.subresourceRange.layerCount = 1;
		viewCIa.image = offScreen.image;
		if (vkCreateImageView(device.getLogicalDevice(), &viewCIa, nullptr, &offScreen.view))
		{
			throw std::runtime_error("failed to create ImageView!");
		}

		VkCommandBuffer cmd = device.beginSingleTimeCommands();

		device.transitionImageLayout(cmd, offScreen.image, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

		device.endSingleTimeCommands(cmd);

		VkImageView attachments[2];
		attachments[1] = offScreen.view;

		VkFramebufferCreateInfo fbufCreateInfo{};
		fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbufCreateInfo.renderPass = offScreenRenderPass;
		fbufCreateInfo.attachmentCount = 2;
		fbufCreateInfo.pAttachments = attachments;
		fbufCreateInfo.width = offscreenImageSize.width;
		fbufCreateInfo.height = offscreenImageSize.height;
		fbufCreateInfo.layers = 1;

		for (uint32_t i = 0; i < 6; i++)
		{
			attachments[0] = shadowmapCubeFaces[i];
			if (vkCreateFramebuffer(device.getLogicalDevice(), &fbufCreateInfo, nullptr, &FramebuffersPerCubeFaces[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create frameBuffer per face!");
			}
		}
	}

	VkRenderPass ShadowRenderSystem::createOffscreenRenderPass()
	{
		const VkFormat offscreenImageFormat{ VK_FORMAT_R32_SFLOAT };

		VkAttachmentDescription osAttachments[2] = {};

		VkFormat validDepthFormat = device.findSupportedFormat({VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM
	}, VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
		
		osAttachments[0].format = offscreenImageFormat;
		osAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		osAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		osAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		osAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		osAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		osAttachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		osAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// Depth attachment
		osAttachments[1].format = validDepthFormat;
		osAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		osAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		osAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		osAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		osAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		osAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		osAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorReference = {};
		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 1;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorReference;
		subpass.pDepthStencilAttachment = &depthReference;

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		//cause of this subpass dependencies, don't need to synchronization between offscreen renderpass and pbr render pass
		// or dont need to create multiple shadowmap framebuffer!

		VkRenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = 2;
		renderPassCreateInfo.pAttachments = osAttachments;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpass;

		if (vkCreateRenderPass(device.getLogicalDevice(), &renderPassCreateInfo, nullptr, &offScreenRenderPass))
		{
			throw std::runtime_error("failed to create offscreen RenderPass!");
		}

		return offScreenRenderPass;
	}

	void ShadowRenderSystem::createShadowCubeMap()
	{
		const VkFormat offscreenImageFormat{ VK_FORMAT_R32_SFLOAT };

		VkImageCreateInfo imageCIa{};
		imageCIa.imageType = VK_IMAGE_TYPE_2D;
		imageCIa.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCIa.format = offscreenImageFormat;
		imageCIa.extent.width = 1024;
		imageCIa.extent.height = 1024;
		imageCIa.extent.depth = 1;
		imageCIa.mipLevels = 1;
		imageCIa.arrayLayers = 6;
		imageCIa.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCIa.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCIa.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCIa.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		device.createImageWithInfo(imageCIa, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, shadowMap.image, shadowMap.memory);
		// Image view
		VkImageViewCreateInfo viewCIa{};
		viewCIa.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewCIa.format = offscreenImageFormat;
		viewCIa.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCIa.subresourceRange = {};
		viewCIa.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCIa.subresourceRange.baseArrayLayer = 0;
		viewCIa.subresourceRange.baseMipLevel = 0;
		viewCIa.subresourceRange.levelCount = 1;
		viewCIa.subresourceRange.layerCount = 6;
		viewCIa.image = shadowMap.image;
		if (vkCreateImageView(device.getLogicalDevice(), &viewCIa, nullptr, &shadowMap.view))
		{
			throw std::runtime_error("failed to create ImageView!");
		}

		viewCIa.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCIa.subresourceRange.layerCount = 1;
		for (uint32_t i = 0; i < 6; i++)
		{
			viewCIa.subresourceRange.baseArrayLayer = i;
			if (vkCreateImageView(device.getLogicalDevice(), &viewCIa, nullptr, &shadowmapCubeFaces[i]))
			{
				throw std::runtime_error("failed to create ImageView!");
			}
		}

		VkCommandBuffer cmd = device.beginSingleTimeCommands();

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 6;
		device.transitionImageLayout(cmd, shadowMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);

		device.endSingleTimeCommands(cmd);

		// Sampler
		VkSamplerCreateInfo samplerCI{};
		samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = 1.f;
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		if (vkCreateSampler(device.getLogicalDevice(), &samplerCI, nullptr, &shadowMap.sampler))
		{
			throw std::runtime_error("failed to create Sampler!");
		}
	}

	std::vector<VkDescriptorSetLayout> ShadowRenderSystem::initializeOffScreenDescriptor()
	{
		descriptorPool = DescriptorPool::Builder(device).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1).build();

		uboBuffer = std::make_unique<Buffer>(device, sizeof(UniformData), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		uboBuffer->map();

		descriptorSetLayout = DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS).
			build();

		auto ubo = uboBuffer->descriptorInfo();
		DescriptorWriter(*descriptorSetLayout, *descriptorPool).writeBuffer(0, &ubo).build(descriptorSet);
		return { descriptorSetLayout->getDescriptorSetLayout()};
	}

	void ShadowRenderSystem::updateShadowMap(VkCommandBuffer cmd, GameObject::Map& gameObjs,  uint32_t frameIndex)
	{
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (uint32_t)offscreenImageSize.width;
		viewport.height = (uint32_t)offscreenImageSize.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor{ {0, 0}, {(uint32_t)offscreenImageSize.width, (uint32_t)offscreenImageSize.height} };
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		for (int faceIndex = 0; faceIndex < 6; faceIndex++)
		{
			VkClearValue clearValues[2];
			clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
			clearValues[1].depthStencil = { 1.0f, 0 };

			const VkExtent2D offscreenImageSize{ 1024, 1024 };

			VkRenderPassBeginInfo renderPassBeginInfo{};
			renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			// Reuse render pass from example pass
			renderPassBeginInfo.renderPass = offScreenRenderPass;
			renderPassBeginInfo.framebuffer = FramebuffersPerCubeFaces[faceIndex];
			renderPassBeginInfo.renderArea.extent = offscreenImageSize;
			renderPassBeginInfo.clearValueCount = 2;
			renderPassBeginInfo.pClearValues = clearValues;

			// Update view matrix via push constant

			glm::mat4 viewMatrix = glm::mat4{ 1.f };
			switch (faceIndex)
			{
			case 0: // POSITIVE_X
				viewMatrix = glm::rotate(viewMatrix, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
				viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
				break;
			case 1:	// NEGATIVE_X
				viewMatrix = glm::rotate(viewMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
				viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
				break;
			case 2:	// POSITIVE_Y
				viewMatrix = glm::rotate(viewMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
				break;
			case 3:	// NEGATIVE_Y
				viewMatrix = glm::rotate(viewMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
				break;
			case 4:	// POSITIVE_Z
				viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
				break;
			case 5:	// NEGATIVE_Z
				viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
				break;
			}

			// Render scene from cube face's point of view
			vkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);


			//vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
			offscreenBuffer.lightView = viewMatrix;
			for (auto& obj : gameObjs)
			{
				// Update shader push constant block
				// Contains current face view matrix
				if (obj.first == 1 || obj.first == 4)
				{
					uint32_t instanceCount = 1;
					offscreenBuffer.modelMat = obj.second.transform.mat4();
					VkBuffer instance = nullptr;
					if (obj.first == 1)
					{
						offscreenBuffer.modelMat = glm::mat4{ 1.f };
					}

					vkCmdPushConstants(
						cmd,
						pipelineLayout,
						VK_SHADER_STAGE_VERTEX_BIT,
						0,
						sizeof(OffscreenConstant),
						&offscreenBuffer);
					obj.second.model->bind(cmd);
					obj.second.model->drawNoTexture(cmd, pipeline->getPipeline(), pipelineLayout, frameIndex);
				}
			}

			vkCmdEndRenderPass(cmd);
		}
	}

	void ShadowRenderSystem::updateUniformBuffer(glm::vec3 lightPos)
	{
		_lightpos = lightPos;
		uniformData.projection = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f , 1024.f);
		uniformData.view = glm::mat4(1.0f);
		uniformData.model = glm::translate(glm::mat4(1.0f), glm::vec3(-lightPos.x, -lightPos.y, -lightPos.z));
		uniformData.lightPos = { lightPos, 1 };
		memcpy(uboBuffer->getMappedMemory(), &uniformData, sizeof(UniformData));
	}

	void ShadowRenderSystem::renderGameObjects(FrameInfo& frameInfo)
	{

	}
};