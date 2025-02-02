#include "ComputerShadeSystem.h"
#include "SwapChain.h"
#include "Model.h"
#include "GameObjectManager.h"

jhb::ComputerShadeSystem::ComputerShadeSystem(Device& device):
	device(device)
{
	computeCommandBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
	createPipeLineLayoutAndPipeline();
	BuildComputeCommandBuffer();
}

jhb::ComputerShadeSystem::~ComputerShadeSystem()
{
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
	std::string code = "shaders/computeCull.comp.spv";

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

void jhb::ComputerShadeSystem::BuildComputeCommandBuffer()
{
	VkCommandBufferBeginInfo cmdBufferBeginInfo{};
	cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		if(vkBeginCommandBuffer(computeCommandBuffers[i], &cmdBufferBeginInfo))
		{
			throw std::runtime_error("failed to begin command buffer");
		}

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
				IndirectCommandBuffer[i]->getBuffer(),
				0,
				IndirectCommandBuffer[i]->descriptorInfo().range
			};

			vkCmdPipelineBarrier(
				computeCommandBuffers[i],
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_DEPENDENCY_BY_REGION_BIT,
				0, nullptr,
				1, &buffer_barrier,
				0, nullptr);

			vkCmdBindPipeline(computeCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
			vkCmdBindDescriptorSets(computeCommandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, pipelinelayout, 0, 1, &descriptorSet[i], 0, 0);

			// Clear the buffer that the compute shader pass will write statistics and draw calls to
			vkCmdFillBuffer(computeCommandBuffers[i], IndirectCommandBuffer[i]->getBuffer(), 0, IndirectCommandBuffer[i]->descriptorInfo().range, 0);

			VkMemoryBarrier memoryBarrier{};
			memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(
				computeCommandBuffers[i],
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_DEPENDENCY_BY_REGION_BIT,
				1, &memoryBarrier,
				0, nullptr,
				0, nullptr);
		}
	}
}

void jhb::ComputerShadeSystem::SetupDescriptor()
{
	std::vector<Model::InstanceData> instanceDatas;
	for (auto& gameObj : GameObjectManager::GetSingleton().gameObjects)
	{
		if (gameObj.first == 0)
		{
			auto tmp = gameObj.second.model->instanceData;
			instanceDatas.insert(instanceDatas.end(), tmp.begin(), tmp.end());
			gameObj.second.model->buildIndirectCommand(indirectCommands);
		}
	}

	{
		// 모든 오브젝트들의 인스턴스 정보를 device 버퍼로 넘김.
		instanceBuffer = std::make_unique<Buffer>(device, sizeof(instanceDatas), instanceDatas.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

		Buffer stagingBuffer(device, sizeof(instanceDatas), instanceDatas.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		stagingBuffer.map();
		stagingBuffer.writeToBuffer(instanceDatas.data(), instanceBuffer->getBufferSize(), 0);

		device.copyBuffer(stagingBuffer.getBuffer(), instanceBuffer->getBuffer(), instanceBuffer->getBufferSize());
		stagingBuffer.unmap();
	}
	
	for (int i = 0; i < jhb::SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		{
			IndirectCommandBuffer[i] = std::make_unique<Buffer>(
				device,
				sizeof(VkDrawIndexedIndirectCommand) * ojectCount,
				ojectCount,
				VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			);

			Buffer stagingBuffer{
			device,
			ojectCount,
			ojectCount * (uint32_t)sizeof(VkDrawIndexedIndirectCommand),
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			};

			stagingBuffer.map();
			stagingBuffer.writeToBuffer(indirectCommands.data());

			device.copyBuffer(stagingBuffer.getBuffer(), IndirectCommandBuffer[i]->getBuffer(), ojectCount * sizeof(VkDrawIndexedIndirectCommand));
			stagingBuffer.unmap();
		}

		// indirectcommand

		computeUboBuffer[i] = std::make_unique<Buffer>(
			device,
			sizeof(UniformData),
			1,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		);

		computeUboBuffer[i]->map();
	}

	discriptorPool[0] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT * 3).build(); // Indirect command


	computeDescriptorSetLayout =DescriptorSetLayout::Builder(device).addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT).
		addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT).
		addBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT).build();



	for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		auto instancebufferInfo = instanceBuffer->descriptorInfo();
		auto indirectbufferInfo = IndirectCommandBuffer[i]->descriptorInfo();
		auto uboInfo = uboBuffer[i]->descriptorInfo();
		DescriptorWriter(*computeDescriptorSetLayout, *discriptorPool[0]).writeBuffer(0, &instancebufferInfo).writeBuffer(1, &indirectbufferInfo).writeBuffer(2, &uboInfo).build(descriptorSet[i]);
	}
}

void jhb::ComputerShadeSystem::UpdateUniform(uint32_t framIndex, glm::mat4 view, glm::mat4 projection)
{
	uniformData.projection = projection;
	uniformData.view = view;
	frustum tmp;
	tmp.update(view * projection);
	for (int i = 0; i < 6; i++)
	{
		uniformData.frustumPlanes[i] = tmp.planes[i];
	}
	uboBuffer[framIndex]->writeToBuffer(&uniformData);
}
