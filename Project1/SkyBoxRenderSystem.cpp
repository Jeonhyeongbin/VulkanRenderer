#include "SkyBoxRenderSystem.h"
#include "Scene.h"
#include <memory>
#include <array>

namespace jhb {
	SkyBoxRenderSystem::SkyBoxRenderSystem(Device& device, Scene& scene, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag) :
		BaseRenderSystem(device, globalSetLayOut, { VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SimplePushConstantData)} }), scene_(scene) {
		createPipeline(renderPass, vert, frag);
		createSkyBox();
	}

	SkyBoxRenderSystem::~SkyBoxRenderSystem()
	{
	}

	void SkyBoxRenderSystem::createSkyBox()
	{
		auto cube = scene_.createCube();
		skyBox = GameObject::createGameObject();
		skyBox.model = cube;
		skyBox.transform.translation = { 0.f, 0.f, 0.f };
		skyBox.transform.scale = { 10.f, 10.f ,10.f };
	}

	void SkyBoxRenderSystem::renderSkyBox(FrameInfo& frameInfo)
	{
		pipeline->bind(frameInfo.commandBuffer);
		BaseRenderSystem::renderGameObjects(frameInfo);
		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			1, 1
			, &frameInfo.skyBoxImageSamplerDecriptorSet,
			0, nullptr
		);

		SimplePushConstantData push{};
		push.ModelMatrix = skyBox.transform.mat4();
		push.normalMatrix = skyBox.transform.normalMatrix();

		vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SimplePushConstantData), &push);

		skyBox.model->bind(frameInfo.commandBuffer);
		skyBox.model->draw(frameInfo.commandBuffer, pipelineLayout, 0);
	}

	void SkyBoxRenderSystem::createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag)
	{
		assert(pipelineLayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

		PipelineConfigInfo pipelineConfig{};
		pipelineConfig.depthStencilInfo.depthTestEnable = false;
		pipelineConfig.depthStencilInfo.depthWriteEnable= false;
		
		pipelineConfig.attributeDescriptions = jhb::Vertex::getAttrivuteDescriptions();
		pipelineConfig.bindingDescriptions = jhb::Vertex::getBindingDescriptions();
		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		pipelineConfig.multisampleInfo.rasterizationSamples = device.msaaSamples;
		pipeline = std::make_unique<Pipeline>(
			device,
			vert,
			frag,
			pipelineConfig);
	}
}
