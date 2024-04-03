#include "SimpleRenderSystem.h"
#include "TriangleApplication.h"
#include "Model.h"
#include <memory>
#include <array>

namespace jhb {
	struct SimplePushConstantData {
		glm::mat4 ModelMatrix{1.f};
		glm::mat4 normalMatrix{1.f};
	};

	SimpleRenderSystem::SimpleRenderSystem(Device& device, VkRenderPass renderPass,const std::vector<std::unique_ptr<jhb::DescriptorSetLayout>>& descSetLayOuts) : device{device} {
		createPipeLineLayout(descSetLayOuts);
		createPipeline(renderPass);
	}

	SimpleRenderSystem::~SimpleRenderSystem()
	{
		vkDestroyPipelineLayout(device.getLogicalDevice(), pipelineLayout, nullptr);
	}

	void SimpleRenderSystem::createPipeLineLayout(const std::vector<std::unique_ptr<jhb::DescriptorSetLayout>>& descriptorSetLayOuts)
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
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayOuts.size());
		pipelineLayoutInfo.pSetLayouts = descsets.data();
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(device.getLogicalDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}
	}

	void SimpleRenderSystem::createPipeline(VkRenderPass renderPass)
	{
		assert(pipelineLayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

		PipelineConfigInfo pipelineConfig{};
		pipelineConfig.depthStencilInfo.depthTestEnable = true;
		pipelineConfig.depthStencilInfo.depthWriteEnable = true;
		pipelineConfig.attributeDescriptions = jhb::Model::Vertex::getAttrivuteDescriptions();
		pipelineConfig.bindingDescriptions = jhb::Model::Vertex::getBindingDescriptions();
		VkVertexInputBindingDescription bindingdesc{};

		bindingdesc.binding = 1;
		bindingdesc.stride = sizeof(jhb::HelloTriangleApplication::InstanceData);
		bindingdesc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
		
		pipelineConfig.bindingDescriptions.push_back(bindingdesc);

		std::vector<VkVertexInputAttributeDescription> attrdesc(3);

		attrdesc[0].binding = 1;
		attrdesc[0].location = 4;
		attrdesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrdesc[0].offset = offsetof(HelloTriangleApplication::InstanceData, HelloTriangleApplication::InstanceData::pos);

		attrdesc[1].binding = 1;
		attrdesc[1].location = 5;
		attrdesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrdesc[1].offset = offsetof(HelloTriangleApplication::InstanceData, HelloTriangleApplication::InstanceData::rot);

		attrdesc[2].binding = 1;
		attrdesc[2].location = 6;
		attrdesc[2].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[2].offset = offsetof(HelloTriangleApplication::InstanceData, HelloTriangleApplication::InstanceData::scale);

		pipelineConfig.attributeDescriptions.insert(pipelineConfig.attributeDescriptions.end(), attrdesc.begin(), attrdesc.end());

		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<Pipeline>(
			device,
			"shaders/shader.vert.spv",
			"shaders/shader.frag.spv",
			pipelineConfig);
	}


	void SimpleRenderSystem::renderGameObjects(FrameInfo& frameInfo, const Buffer& instanceBuffer)
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
			, &frameInfo.globalImageSamplerDescriptorSet,
			0, nullptr
		);


		for (auto& kv : frameInfo.gameObjects)
		{
			auto& obj = kv.second;
			if (kv.first == 0)
				continue;
			if (obj.model == nullptr)
			{
				continue;
			}
			SimplePushConstantData push{};
			push.ModelMatrix = obj.transform.mat4();
			push.normalMatrix = obj.transform.normalMatrix();

			vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SimplePushConstantData), &push);
			
			if (kv.first == 1)
			{
				VkBuffer buffer[1] = {instanceBuffer.getBuffer()};
				obj.model->bind(frameInfo.commandBuffer, buffer);
			}
			else
			{
				obj.model->bind(frameInfo.commandBuffer);
			}
			obj.model->draw(frameInfo.commandBuffer, 64);
		}
	}
}