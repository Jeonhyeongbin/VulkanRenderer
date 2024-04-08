#include "BaseRenderSystem.h"
#include "TriangleApplication.h"
#include "Model.h"
#include <memory>
#include <array>

namespace jhb {

	BaseRenderSystem::BaseRenderSystem(Device& device, VkRenderPass renderPass,const std::vector<VkDescriptorSetLayout>& descSetLayOuts, const std::vector<VkPushConstantRange>& pushConstanRange) : device{ device } {
		createPipeLineLayout(descSetLayOuts, pushConstanRange);
	}

	BaseRenderSystem::~BaseRenderSystem()
	{
		vkDestroyPipelineLayout(device.getLogicalDevice(), pipelineLayout, nullptr);
	}

	void BaseRenderSystem::createPipeLineLayout(const std::vector<VkDescriptorSetLayout>& descriptorSetLayOuts, const std::vector<VkPushConstantRange>& pushConstanRange)
	{
		//VkPushConstantRange pushConstantRange{};
		//pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // This means that both vertex and fragment shader using constant 
		//pushConstantRange.offset = 0;
		//pushConstantRange.size = sizeof(SimplePushConstantData);

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = descriptorSetLayOuts.size();
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayOuts.data();
		if (!pushConstanRange.empty())
		{
			pipelineLayoutInfo.pushConstantRangeCount = pushConstanRange.size();
			pipelineLayoutInfo.pPushConstantRanges = pushConstanRange.data();
		}
		if (vkCreatePipelineLayout(device.getLogicalDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}
	}

	void BaseRenderSystem::renderGameObjects(FrameInfo& frameInfo, Buffer* instanceBuffer)
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
	}
}