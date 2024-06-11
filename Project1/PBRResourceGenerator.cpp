#include "PBRResourceGenerator.h"

jhb::PBRResourceGenerator::PBRResourceGenerator(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange)
: BaseRenderSystem(device, globalSetLayOut, pushConstanRange) {
	createPipeline(renderPass, vert, frag);
}

jhb::PBRResourceGenerator::~PBRResourceGenerator()
{
}


void jhb::PBRResourceGenerator::generateBRDFLUT(VkCommandBuffer cmd, GameObject& gameObject)
{
	pipeline->bind(cmd);

	gameObject.model->bind(cmd);
	gameObject.model->draw(cmd, pipelineLayout, 0);
}

void jhb::PBRResourceGenerator::generateIrradianceCube(VkCommandBuffer cmd, GameObject& gameObject, std::vector<VkDescriptorSet> descSets, const jhb::IrradiencePushBlock& push)
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
	gameObject.model->draw(cmd, pipelineLayout, 0);
}

void jhb::PBRResourceGenerator::generatePrefilteredCube(VkCommandBuffer cmd, GameObject& gameObject, std::vector<VkDescriptorSet> descSets, const jhb::PrefileterPushBlock& push)
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
	gameObject.model->draw(cmd, pipelineLayout, 0);
}

void jhb::PBRResourceGenerator::createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag)
{
	assert(pipelineLayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

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
