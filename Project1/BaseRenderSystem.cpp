#include "BaseRenderSystem.h"
#include "JHBApplication.h"
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