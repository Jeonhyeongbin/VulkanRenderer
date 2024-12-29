#include "ComputerShadeSystem.h"
#include "SwapChain.h"

jhb::ComputerShadeSystem::ComputerShadeSystem(Device& device)
{
	computeSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	icomputeFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateSemaphore(device.getLogicalDevice(), &semaphoreInfo, nullptr, &computeSemaphores[i]) !=
			VK_SUCCESS ||
			vkCreateFence(device.getLogicalDevice(), &fenceInfo, nullptr, &icomputeFences[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create synchronization objects for a frame!");
		}
	}
}

void jhb::ComputerShadeSystem::createPipeLineLayoutAndPipeline()
{
	VkPipelineLayoutCreateInfo pipelinelayoutCreateInfo{};
	pipelinelayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelinelayoutCreateInfo.setLayoutCount = 1;
	const VkDescriptorSetLayout tmp = { computeDescriptorSetLayout->getDescriptorSetLayout() };
	pipelinelayoutCreateInfo.pSetLayouts = &tmp;
	
	if (vkCreatePipelineLayout(device.getLogicalDevice(), &pipelinelayoutCreateInfo, nullptr, &pipelinelayout) == VK_FALSE)
	{
		throw std::runtime_error("failed to create Compute Pipelinelayout!");
	}

	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT;

	// todo
	std::string code = "";

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	if (vkCreateShaderModule(device.getLogicalDevice(), &createInfo, nullptr, &computeShader) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module");
	}

	// Create pipeline
	VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.layout = pipelinelayout;
	computePipelineCreateInfo.stage = shaderStage;

	if (vkCreateComputePipelines(device.getLogicalDevice(), nullptr, 1, &computePipelineCreateInfo, nullptr, &pipeline) == VK_FALSE)
	{
		throw std::runtime_error("failed to create shader module");
	}
}

jhb::ComputerShadeSystem::BuildComputeCommandBuffer()
{
	VkCommandBuffer cmdBuffer = device.beginSingleComputeCommands();  
	auto queueFamilies = device.findQueueFamilies(device.getPhysicalDevice());
	if (queueFamilies.computeFamily != queueFamilies.graphicsFamily)
	{
		VkBufferMemoryBarrier buffer_barrier =
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
			nullptr,
			0,
			VK_ACCESS_SHADER_WRITE_BIT,
			queueFamilies.graphicsFamily.value(),
			queueFamilies.computeFamily.value(),
			IndirectCommandBuffer->getBuffer(),
			0,
			IndirectCommandBuffer->descriptorInfo().range
		};

		vkCmdPipelineBarrier(
			cmdBuffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_FLAGS_NONE,
			0, nullptr,
			1, &buffer_barrier,
			0, nullptr);

		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelinelayout, 0, 1, &compute.descriptorSet, 0, 0);

		// Clear the buffer that the compute shader pass will write statistics and draw calls to
		vkCmdFillBuffer(cmdBuffer, indirectDrawCountBuffer.buffer, 0, indirectCommandsBuffer.descriptor.range, 0);

		// This barrier ensures that the fill command is finished before the compute shader can start writing to the buffer
		VkMemoryBarrier memoryBarrier = vks::initializers::memoryBarrier();
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			compute.commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_FLAGS_NONE,
			1, &memoryBarrier,
			0, nullptr,
			0, nullptr);
	}


}

void jhb::ComputerShadeSystem::SetupDescriptor()
{

}
