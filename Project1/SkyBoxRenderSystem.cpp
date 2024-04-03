#include "SkyBoxRenderSystem.h"
#include <memory>
#include <array>

namespace jhb {
	struct SimplePushConstantData {
		glm::mat4 ModelMatrix{1.f};
		glm::mat4 normalMatrix{1.f};
	};

	SkyBoxRenderSystem::SkyBoxRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<std::unique_ptr<jhb::DescriptorSetLayout>>& descSetLayOuts) : device{ device } {
		createPipeLineLayout(descSetLayOuts);
		createPipeline(renderPass);
	}

	SkyBoxRenderSystem::~SkyBoxRenderSystem()
	{
		vkDestroyPipelineLayout(device.getLogicalDevice(), pipelineLayout, nullptr);
	}

	void SkyBoxRenderSystem::renderSkyBox(FrameInfo& frameInfo)
	{
		pipeline->bind(frameInfo.commandBuffer);

		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			0, 1
			, &frameInfo.globaldDescriptorSet,
			0, nullptr
		);
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
			obj.model->draw(frameInfo.commandBuffer);
		}
	}

	void SkyBoxRenderSystem::loadCubemap(const std::string& filename, VkFormat format)
	{
	}

	void SkyBoxRenderSystem::createPipeLineLayout(const std::vector<std::unique_ptr<jhb::DescriptorSetLayout>>& descriptorSetLayOuts)
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // This means that both vertex and fragment shader using constant 
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(SimplePushConstantData);

		std::vector<VkDescriptorSetLayout> descsets{};
		for (auto& desc : descriptorSetLayOuts)
		{
			descsets.push_back(desc->getDescriptorSetLayout());
		}
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descsets.size());
		pipelineLayoutInfo.pSetLayouts = descsets.data();
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(device.getLogicalDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}
	}

	void SkyBoxRenderSystem::createPipeline(VkRenderPass renderPass)
	{
		assert(pipelineLayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

		PipelineConfigInfo pipelineConfig{};
		pipelineConfig.depthStencilInfo.depthTestEnable = false;
		pipelineConfig.depthStencilInfo.depthWriteEnable= false;
		pipelineConfig.attributeDescriptions = jhb::Model::Vertex::getAttrivuteDescriptions();
		pipelineConfig.bindingDescriptions = jhb::Model::Vertex::getBindingDescriptions();
		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<Pipeline>(
			device,
			"shaders/skybox.vert.spv",
			"shaders/skybox.frag.spv",
			pipelineConfig);
	}
}