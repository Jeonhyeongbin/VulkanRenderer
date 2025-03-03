#include "PointLightSystem.h"
#include <memory>
#include <array>

namespace jhb {
	PointLightSystem::PointLightSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag) :
		BaseRenderSystem(device, globalSetLayOut, { VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PointLightPushConstants)} }) {
		createPipeline(renderPass, vert, frag);
		createLights();
	}

	PointLightSystem::~PointLightSystem()
	{
	}

	void PointLightSystem::createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag)
	{
		assert(pipelineLayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

		PipelineConfigInfo pipelineConfig{};
		pipelineConfig.depthStencilInfo.depthWriteEnable = VK_TRUE;
		pipelineConfig.depthStencilInfo.depthTestEnable= VK_TRUE;
		pipelineConfig.attributeDescriptions = jhb::Vertex::getAttrivuteDescriptions();
		pipelineConfig.bindingDescriptions = jhb::Vertex::getBindingDescriptions();
		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.attributeDescriptions.clear();
		pipelineConfig.bindingDescriptions.clear();
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<Pipeline>(
			device,
			vert,
			frag,
			pipelineConfig);
	}



	void PointLightSystem::update(FrameInfo& frameInfo, GlobalUbo& ubo)
	{
		auto rotateLight = glm::rotate(glm::mat4(1.f), frameInfo.frameTime, { 0.f, -1.f, 0.f });
		int lightIndex = 0;
		for (auto& kv : lightObjects)
		{
			auto& obj = kv.second;
			if (obj.pointLight == nullptr) continue;

			// update position
			//obj.transform.translation = glm::vec3(rotateLight * glm::vec4(obj.transform.translation, 1.f));

			// copy light to ubo
			if (kv.first == 0)
			{
				ubo.pointLights[lightIndex].position = glm::vec4(obj.transform.translation, 1.f);
				ubo.pointLights[lightIndex].color = glm::vec4(obj.color, obj.pointLight->lightIntensity);

				lightIndex += 1;
			}
		}
		ubo.numLights = lightIndex;
	}

	void PointLightSystem::renderGameObjects(FrameInfo& frameInfo)
	{
		pipeline->bind(frameInfo.commandBuffer);
		BaseRenderSystem::renderGameObjects(frameInfo);
		for (auto& kv : lightObjects)
		{
			auto& obj = kv.second;
			if (obj.pointLight == nullptr) continue;
			{
				if (kv.first == 0)
				{
					PointLightPushConstants push{};
					push.position = glm::vec4(obj.transform.translation, 1.f);
					push.color = glm::vec4(obj.color, obj.pointLight->lightIntensity);
					push.radius = obj.transform.scale.x;

					vkCmdPushConstants(
						frameInfo.commandBuffer,
						pipelineLayout,
						VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
						0,
						sizeof(PointLightPushConstants),
						&push
					);
					vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
				}
			}
		}
	}

	void PointLightSystem::createLights()
	{
		static uint32_t currentId = 0;
		std::vector<glm::vec3> lightColors{
			{1.f, 1.f, 1.f},
		};

		for (int i = 0; i < lightColors.size(); i++)
		{
			auto pointLight = GameObject::makePointLight(1.f);
			pointLight.color = lightColors[i];
			pointLight.pointLight->lightIntensity = 10.f;
			auto rotateLight = glm::rotate(glm::mat4(1.f), (i * glm::two_pi<float>() / lightColors.size()), { 0.f, -1.f, 0.f });
			pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(0.f, -10.5f, 0.f, 1.f));
			pointLight.setId(currentId++);
			lightObjects.emplace(pointLight.getId(), std::move(pointLight));
		}
	}
}