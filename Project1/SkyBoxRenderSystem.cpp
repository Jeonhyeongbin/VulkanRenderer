#include "SkyBoxRenderSystem.h"
#include <memory>
#include <array>

namespace jhb {
	SkyBoxRenderSystem::SkyBoxRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange) :
		BaseRenderSystem(device, renderPass, globalSetLayOut, pushConstanRange) {
		createPipeline(renderPass, vert, frag);
	}

	SkyBoxRenderSystem::~SkyBoxRenderSystem()
	{
	}

	void SkyBoxRenderSystem::renderSkyBox(VkCommandBuffer cmd, GameObject& gameObject, std::vector<VkDescriptorSet> descSets)
	{
		pipeline->bind(cmd);

		gameObject.model->bind(cmd);
		gameObject.model->draw(cmd, pipelineLayout, 0 ,1);
	}

	void SkyBoxRenderSystem::renderSkyBox(VkCommandBuffer cmd, GameObject& gameObject, std::vector<VkDescriptorSet> descSets, const jhb::PrefileterPushBlock& push)
	{
		pipeline->bind(cmd);

		vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			0, 1
			, &descSets[0],
			0, nullptr
		);

		vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			1, 1
			, &descSets[1],
			0, nullptr
		);
		vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PrefileterPushBlock), &push);
		gameObject.model->bind(cmd);
		gameObject.model->draw(cmd, pipelineLayout, 0 ,1);
	}

	void SkyBoxRenderSystem::renderSkyBox(VkCommandBuffer cmd, GameObject& gameObject, std::vector<VkDescriptorSet> descSets, const jhb::IrradiencePushBlock& push)
	{
		pipeline->bind(cmd);

		vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			0, 1
			, &descSets[0],
			0, nullptr
		);

		vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			1, 1
			, &descSets[1],
			0, nullptr
		);

		vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(IrradiencePushBlock), &push);
		gameObject.model->bind(cmd);
		gameObject.model->draw(cmd, pipelineLayout, 0, 1);
	}

	void SkyBoxRenderSystem::renderSkyBox(FrameInfo& frameInfo)
	{
		BaseRenderSystem::renderGameObjects(frameInfo, nullptr);
		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			1, 1
			, &frameInfo.skyBoxImageSamplerDecriptorSet,
			0, nullptr
		);

		for (auto& kv : frameInfo.gameObjects)
		{
			auto& obj = kv.second;
			if (obj.model == nullptr)
			{
				continue;
			}
			SimplePushConstantData push{};
			push.ModelMatrix = obj.transform.mat4();
			push.normalMatrix = obj.transform.normalMatrix();

			vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SimplePushConstantData), &push);

			obj.model->bind(frameInfo.commandBuffer);
			obj.model->draw(frameInfo.commandBuffer, pipelineLayout, 0, 1);
		}
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
		pipeline = std::make_unique<Pipeline>(
			device,
			vert,
			frag,
			pipelineConfig);
	}
}