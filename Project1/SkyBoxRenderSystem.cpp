#include "SkyBoxRenderSystem.h"
#include <memory>
#include <array>

namespace jhb {
	SkyBoxRenderSystem::SkyBoxRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag) :
		BaseRenderSystem(device, globalSetLayOut, { VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SimplePushConstantData)} }) {
		createPipeline(renderPass, vert, frag);
		createSkyBox();
	}

	SkyBoxRenderSystem::~SkyBoxRenderSystem()
	{
	}

	void SkyBoxRenderSystem::createSkyBox()
	{
		std::vector<std::string> cubefiles = {  };
		std::vector<Vertex> vertices = {
			{{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
	  {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},
	  {{-.5f, -.5f, .5f}, {.9f, .9f, .9f}},
	  {{-.5f, .5f, -.5f}, {.9f, .9f, .9f}},

	  {{.5f, -.5f, -.5f}, {.8f, .8f, .5f}},
	  {{.5f, .5f, .5f}, {.8f, .8f, .5f}},
	  {{.5f, -.5f, .5f}, {.8f, .8f, .5f}},
	  {{.5f, .5f, -.5f}, {.8f, .8f, .5f}},

	  {{-.5f, -.5f, -.5f}, {.9f, .6f, .5f}},
	  {{.5f, -.5f, .5f}, {.9f, .6f, .5f}},
	  {{-.5f, -.5f, .5f}, {.9f, .6f, .5f}},
	  {{.5f, -.5f, -.5f}, {.9f, .6f, .5f}},

	  {{-.5f, .5f, -.5f}, {.8f, .5f, .5f}},
	  {{.5f, .5f, .5f}, {.8f, .5f, .5f}},
	  {{-.5f, .5f, .5f}, {.8f, .5f, .5f}},
	  {{.5f, .5f, -.5f}, {.8f, .5f, .5f}},

	  {{-.5f, -.5f, 0.5f}, {.5f, .5f, .8f}},
	  {{.5f, .5f, 0.5f}, {.5f, .5f, .8f}},
	  {{-.5f, .5f, 0.5f}, {.5f, .5f, .8f}},
	  {{.5f, -.5f, 0.5f}, {.5f, .5f, .8f}},

	  {{-.5f, -.5f, -0.5f}, {.5f, .8f, .5f}},
	  {{.5f, .5f, -0.5f}, {.5f, .8f, .5f}},
	  {{-.5f, .5f, -0.5f}, {.5f, .8f, .5f}},
	  {{.5f, -.5f, -0.5f}, {.5f, .8f, .5f}},
		};

		std::vector<uint32_t> indices = { 0,  1,  2,  0,  3,  1,  4,  5,  6,  4,  7,  5,  8,  9,  10, 8,  11, 9,
								12, 13, 14, 12, 15, 13, 16, 17, 18, 16, 19, 17, 20, 21, 22, 20, 23, 21 };

		std::shared_ptr<Model> cube = std::make_unique<Model>(device);
		cube->createVertexBuffer(vertices);
		cube->createIndexBuffer(indices);
		cube->getTexture(0).loadKTXTexture(device, "Texture/pisa_cube.ktx", VK_IMAGE_VIEW_TYPE_CUBE, 6);
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