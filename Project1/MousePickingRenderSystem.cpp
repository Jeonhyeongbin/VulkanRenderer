#include "MousePickingRenderSystem.h"
#include "JHBApplication.h"

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
			if (pickingUbo)
			{
				uint32_t objId = obj.getId()+1;
				vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(uint32_t), &objId);
				obj.model->drawInPickPhase(cmd, pipelineLayout, pipeline->getPipeline(), frameIndex, 64);
			}
			else {
				obj.model->draw(cmd, pipelineLayout, frameIndex, 64);
			}
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
	VkVertexInputBindingDescription bindingdesc{};

	bindingdesc.binding = 1;
	bindingdesc.stride = sizeof(jhb::JHBApplication::InstanceData);
	bindingdesc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

	pipelineConfig.bindingDescriptions.push_back(bindingdesc);

	std::vector<VkVertexInputAttributeDescription> attrdesc(8);

	attrdesc[0].binding = 1;
	attrdesc[0].location = 5;
	attrdesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attrdesc[0].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::pos);

	attrdesc[1].binding = 1;
	attrdesc[1].location = 6;
	attrdesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attrdesc[1].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::rot);

	attrdesc[2].binding = 1;
	attrdesc[2].location = 7;
	attrdesc[2].format = VK_FORMAT_R32_SFLOAT;
	attrdesc[2].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::scale);

	attrdesc[3].binding = 1;
	attrdesc[3].location = 8;
	attrdesc[3].format = VK_FORMAT_R32_SFLOAT;
	attrdesc[3].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::roughness);
	attrdesc[4].binding = 1;
	attrdesc[4].location = 9;
	attrdesc[4].format = VK_FORMAT_R32_SFLOAT;
	attrdesc[4].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::metallic);
	attrdesc[5].binding = 1;
	attrdesc[5].location = 10;
	attrdesc[5].format = VK_FORMAT_R32_SFLOAT;
	attrdesc[5].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::r);
	attrdesc[6].binding = 1;
	attrdesc[6].location = 11;
	attrdesc[6].format = VK_FORMAT_R32_SFLOAT;
	attrdesc[6].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::g);
	attrdesc[7].binding = 1;
	attrdesc[7].location = 12;
	attrdesc[7].format = VK_FORMAT_R32_SFLOAT;
	attrdesc[7].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::b);

	pipelineConfig.attributeDescriptions.insert(pipelineConfig.attributeDescriptions.end(), attrdesc.begin(), attrdesc.end());

	Pipeline::defaultPipelineConfigInfo(pipelineConfig);
	pipelineConfig.renderPass = renderPass;
	pipelineConfig.pipelineLayout = pipelineLayout;
	pipeline = std::make_unique<Pipeline>(
		device,
		vert,
		frag,
		pipelineConfig);
}
