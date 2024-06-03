#include "ShadowRenderSystem.h"
#include <memory>
#include <array>

namespace jhb {
	ShadowRenderSystem::ShadowRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange) :
		BaseRenderSystem(device, renderPass, globalSetLayOut, pushConstanRange) {
		createPipeline(renderPass, vert, frag);
		createShadowCubeMap();
	}

	ShadowRenderSystem::~ShadowRenderSystem()
	{
	}

	void ShadowRenderSystem::createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag)
	{
		assert(pipelineLayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

		PipelineConfigInfo pipelineConfig{};
		pipelineConfig.depthStencilInfo.depthWriteEnable = VK_TRUE;
		pipelineConfig.depthStencilInfo.depthTestEnable = VK_TRUE;
		pipelineConfig.attributeDescriptions = jhb::Vertex::getAttrivuteDescriptions();
		pipelineConfig.bindingDescriptions = jhb::Vertex::getBindingDescriptions();
		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.attributeDescriptions.clear();
		pipelineConfig.bindingDescriptions.clear();
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		pipelineConfig.multisampleInfo.rasterizationSamples = device.msaaSamples;
		pipeline = std::make_unique<Pipeline>(
			device,
			vert,
			frag,
			pipelineConfig);
	}

	void ShadowRenderSystem::createOffscreenFrameBuffer()
	{
		const VkExtent3D offscreenImageSize{ 1024, 1024 , 1 };

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
		imageCIa.extent = offscreenImageSize;
		imageCIa.mipLevels = 1;
		imageCIa.arrayLayers = 1;
		imageCIa.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCIa.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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

		device.transitionImageLayout(cmd, shadowMap.image, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

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

	void ShadowRenderSystem::createOffscreenRenderPass()
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
	}

	void ShadowRenderSystem::createShadowCubeMap()
	{
		const VkFormat offscreenImageFormat{ VK_FORMAT_R32_SFLOAT };
		const VkExtent2D offscreenImageSize{ 1024, 1024 };

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
		imageCIa.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageCIa.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

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

		for (uint32_t i = 0; i < 6; i++)
		{
			viewCIa.subresourceRange.layerCount = i;
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
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = 1.f;
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		if (vkCreateSampler(device.getLogicalDevice(), &samplerCI, nullptr, &shadowMap.sampler))
		{
			throw std::runtime_error("failed to create Sampler!");
		}
	}

	void ShadowRenderSystem::updateCubeFace(VkCommandBuffer cmd, uint32_t faceIndex, GameObject& gameobj, VkDescriptorSet discriptorSet)
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

		glm::mat4 viewMatrix = glm::mat4(1.0f);
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

		// Update shader push constant block
		// Contains current face view matrix
		vkCmdPushConstants(
			cmd,
			pipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT,
			0,
			sizeof(glm::mat4),
			&viewMatrix);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipeline());
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &discriptorSet, 0, NULL);
		models.scene.draw(cmd);

		vkCmdEndRenderPass(cmd);
	}


	void ShadowRenderSystem::update(FrameInfo& frameInfo, GlobalUbo& ubo)
	{
		auto rotateLight = glm::rotate(glm::mat4(1.f), frameInfo.frameTime, { 0.f, -1.f, 0.f });
		int lightIndex = 0;
		for (auto& kv : frameInfo.gameObjects)
		{
			auto& obj = kv.second;
			if (obj.pointLight == nullptr) continue;

			// update position
			//obj.transform.translation = glm::vec3(rotateLight * glm::vec4(obj.transform.translation, 1.f));

			// copy light to ubo
			ubo.pointLights[lightIndex].position = glm::vec4(obj.transform.translation, 1.f);
			ubo.pointLights[lightIndex].color = glm::vec4(obj.color, obj.pointLight->lightIntensity);

			lightIndex += 1;
		}
		ubo.numLights = lightIndex;
	}
}