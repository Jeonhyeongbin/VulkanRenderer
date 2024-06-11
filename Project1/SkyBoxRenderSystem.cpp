#include "SkyBoxRenderSystem.h"
#include <memory>
#include <array>

namespace jhb {
	SkyBoxRenderSystem::SkyBoxRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange) :
		BaseRenderSystem(device, globalSetLayOut, pushConstanRange) {
		createPipeline(renderPass, vert, frag);
	}

	SkyBoxRenderSystem::~SkyBoxRenderSystem()
	{
	}

	void SkyBoxRenderSystem::renderSkyBox(FrameInfo& frameInfo)
	{
		pipeline->bind(frameInfo.commandBuffer);
		BaseRenderSystem::renderGameObjects(frameInfo);
		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			1, 1
			, &frameInfo.skyBoxImageSamplerDecriptorSet,
			0, nullptr
		);

		auto& obj = frameInfo.gameObjects[0];
		SimplePushConstantData push{};
		push.ModelMatrix = obj.transform.mat4();
		push.normalMatrix = obj.transform.normalMatrix();

		vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SimplePushConstantData), &push);

		obj.model->bind(frameInfo.commandBuffer);
		obj.model->draw(frameInfo.commandBuffer, pipelineLayout, 0);
	}

	void SkyBoxRenderSystem::createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag)
	{
		assert(pipelineLayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

		PipelineConfigInfo pipelineConfig{};
		pipelineConfig.depthStencilInfo.depthTestEnable = false;
		pipelineConfig.depthStencilInfo.depthWriteEnable= false;
		pipelineConfig.attributeDescriptions = jhb::Vertex::getAttrivuteDescriptions();
		pipelineConfig.bindingDescriptions = jhb::Vertex::getBindingDescriptions();
		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		pipelineConfig.multisampleInfo.rasterizationSamples = device.msaaSamples;
		pipeline = std::make_unique<Pipeline>(
			device,
			vert,
			frag,
			pipelineConfig);
	}
}