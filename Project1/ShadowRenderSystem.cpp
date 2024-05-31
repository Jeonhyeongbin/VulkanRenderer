#include "ShadowRenderSystem.h"
#include <memory>
#include <array>

namespace jhb {
	ShadowRenderSystem::ShadowRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange) :
		BaseRenderSystem(device, renderPass, globalSetLayOut, pushConstanRange) {
		createPipeline(renderPass, vert, frag);
	}

	ShadowRenderSystem::~ShadowRenderSystem()
	{
	}

	void ShadowRenderSystem::createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag)
	{
		assert(pipelineLayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

		PipelineConfigInfo pipelineConfig{};
		pipelineConfig.depthStencilInfo.depthWriteEnable = VK_TRUE;
		pipelineConfig.depthStencilInfo.depthTestEnable = VK_TRUE;
		pipelineConfig.attributeDescriptions = jhb::Vertex::getAttrivuteDescriptions();
		pipelineConfig.bindingDescriptions = jhb::Vertex::getBindingDescriptions();
		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.attributeDescriptions.clear();
		pipelineConfig.bindingDescriptions.clear();
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		pipelineConfig.multisampleInfo.rasterizationSamples = device.msaaSamples;
		pipeline = std::make_unique<Pipeline>(
			device,
			vert,
			frag,
			pipelineConfig);
	}

	void ShadowRenderSystem::createOffscreenFrameBuffer()
	{

	}

	void ShadowRenderSystem::createOffscreenRenderPass()
	{

	}

	void ShadowRenderSystem::createShadowCubeMap()
	{

	}


	void ShadowRenderSystem::update(FrameInfo& frameInfo, GlobalUbo& ubo)
	{
		auto rotateLight = glm::rotate(glm::mat4(1.f), frameInfo.frameTime, { 0.f, -1.f, 0.f });
		int lightIndex = 0;
		for (auto& kv : frameInfo.gameObjects)
		{
			auto& obj = kv.second;
			if (obj.pointLight == nullptr) continue;

			// update position
			//obj.transform.translation = glm::vec3(rotateLight * glm::vec4(obj.transform.translation, 1.f));

			// copy light to ubo
			ubo.pointLights[lightIndex].position = glm::vec4(obj.transform.translation, 1.f);
			ubo.pointLights[lightIndex].color = glm::vec4(obj.color, obj.pointLight->lightIntensity);

			lightIndex += 1;
		}
		ubo.numLights = lightIndex;
	}

	void ShadowRenderSystem::renderGameObjects(FrameInfo& frameInfo, Buffer* instanceBuffer)
	{
		pipeline->bind(frameInfo.commandBuffer);
		BaseRenderSystem::renderGameObjects(frameInfo, instanceBuffer);
		for (auto& kv : frameInfo.gameObjects)
		{
			auto& obj = kv.second;
			if (obj.pointLight == nullptr) continue;
			{
				/*vkCmdPushConstants(
					frameInfo.commandBuffer,
					pipelineLayout,
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					0,
					sizeof(PointLightPushConstants),
					&push
				);*/
				vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
			}
		}
	}
}