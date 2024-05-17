#include "MousePickingRenderSystem.h"

jhb::MousePickingRenderSystem::MousePickingRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange)
	: BaseRenderSystem(device, renderPass, globalSetLayOut, pushConstanRange)
{
	createPipeline(renderPass,vert, frag);
}

jhb::MousePickingRenderSystem::~MousePickingRenderSystem()
{

}

void jhb::MousePickingRenderSystem::renderMousePickedObjToOffscreen(VkCommandBuffer cmd, jhb::GameObject::Map& gameObjects, const std::vector<VkDescriptorSet>& descriptorsets,
	int frameIndex, Buffer* instanceBuffer, Buffer* pickingUbo)
{
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipelineLayout,
		0, 1
		, &descriptorsets[0],
		0, nullptr
	);

	// update object id per object
	for (auto& kv : gameObjects)
	{
		auto& obj = kv.second;

		if (obj.model == nullptr)
		{
			continue;
		}

		PickingUbo ubo{
			obj.getId()+1
		};

		pickingUbo->writeToBuffer(&ubo);
		pickingUbo->flush();

		vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			1, 1
			, &descriptorsets[1],
			0, nullptr
		);

		if (kv.first == 1)
		{
			if (kv.first == 1)
			{
				VkBuffer buffer[1] = { instanceBuffer->getBuffer() };
				obj.model->bind(cmd, buffer);
			}
			else
			{
				obj.model->bind(cmd);
			}
			obj.model->draw(cmd, pipelineLayout, frameIndex, 64);
		}
	}
}

void jhb::MousePickingRenderSystem::createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag)
{
	PipelineConfigInfo pipelineConfig{};
	pipelineConfig.depthStencilInfo.depthTestEnable = true;
	pipelineConfig.depthStencilInfo.depthWriteEnable = true;
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
