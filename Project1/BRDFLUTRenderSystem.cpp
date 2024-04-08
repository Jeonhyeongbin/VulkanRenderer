#include "BRDFLUTRenderSystem.h"
#include "Model.h"

jhb::BRDFLUTRenderSystem::BRDFLUTRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange)
:BaseRenderSystem(device, renderPass, globalSetLayOut, pushConstanRange)
{
	createPipeline(renderPass, vert, frag);
}

jhb::BRDFLUTRenderSystem::~BRDFLUTRenderSystem()
{
}

void jhb::BRDFLUTRenderSystem::renderBRDFLUTImage(VkCommandBuffer cmd, GameObject& gameObject, std::vector<VkDescriptorSet> descSets)
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

	gameObject.model->bind(cmd);
	gameObject.model->draw(cmd);
}

void jhb::BRDFLUTRenderSystem::createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag)
{
	assert(pipelineLayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

	std::vector<VkVertexInputAttributeDescription> attributeDescriptions(1);
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(jhb::Model::Vertex2D, xy);

	std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
	bindingDescriptions[0].binding = 0;
	bindingDescriptions[0].stride = sizeof(jhb::Model::Vertex2D);
	bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	PipelineConfigInfo pipelineConfig{};
	pipelineConfig.depthStencilInfo.depthTestEnable = false;
	pipelineConfig.depthStencilInfo.depthWriteEnable = false;
	pipelineConfig.attributeDescriptions = attributeDescriptions;
	pipelineConfig.bindingDescriptions = bindingDescriptions;
	Pipeline::defaultPipelineConfigInfo(pipelineConfig);
	pipelineConfig.renderPass = renderPass;
	pipelineConfig.pipelineLayout = pipelineLayout;
	pipeline = std::make_unique<Pipeline>(
		device,
		vert,
		frag,
		pipelineConfig);
}
