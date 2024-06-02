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

	}

	void ShadowRenderSystem::createOffscreenRenderPass()
	{

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
		viewCIa.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCIa.format = offscreenImageFormat;
		viewCIa.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCIa.subresourceRange = {};
		viewCIa.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCIa.subresourceRange.levelCount = 1;
		viewCIa.subresourceRange.layerCount = 1;
		viewCIa.image = shadowMap.image;
		if (vkCreateImageView(device.getLogicalDevice(), &viewCIa, nullptr, &shadowMap.view))
		{
			throw std::runtime_error("failed to create ImageView!");
		}

		// Sampler
		//VkSamplerCreateInfo samplerCI{};
		//samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		//samplerCI.magFilter = VK_FILTER_LINEAR;
		//samplerCI.minFilter = VK_FILTER_LINEAR;
		//samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		//samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		//samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		//samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		//samplerCI.minLod = 0.0f;
		//samplerCI.maxLod = 1.f;
		//samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		//if (vkCreateSampler(device.getLogicalDevice(), &samplerCI, nullptr, ))
		//{
		//	throw std::runtime_error("failed to create Sampler!");
		//}
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

	void ShadowRenderSystem::renderGameObjects(FrameInfo& frameInfo, Buffer* instanceBuffer)
	{
		pipeline->bind(frameInfo.commandBuffer);
		BaseRenderSystem::renderGameObjects(frameInfo, instanceBuffer);
		for (auto& kv : frameInfo.gameObjects)
		{
			auto& obj = kv.second;
			if (obj.pointLight == nullptr) continue;
			{
				/*vkCmdPushConstants(
					frameInfo.commandBuffer,
					pipelineLayout,
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					0,
					sizeof(PointLightPushConstants),
					&push
				);*/
				vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
			}
		}
	}
}