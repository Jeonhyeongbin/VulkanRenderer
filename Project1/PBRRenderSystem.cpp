#include "PBRRenderSystem.h"
#include "JHBApplication.h"
#include "Model.h"
#include "FrameInfo.h"
#include <memory>
#include <array>

namespace jhb {

	PBRRendererSystem::PBRRendererSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange,
		const std::vector<Material>& materials) :
		BaseRenderSystem(device, renderPass, globalSetLayOut, pushConstanRange) {
		createPipelinePerMaterial(renderPass, vert, frag, materials);
	}

	PBRRendererSystem::PBRRendererSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange) :
		BaseRenderSystem(device, renderPass, globalSetLayOut, pushConstanRange) {
		createPipeline(renderPass, vert, frag);
	}

	PBRRendererSystem::~PBRRendererSystem()
	{
	}

	void PBRRendererSystem::createPipelinePerMaterial(VkRenderPass renderPass, const std::string& vert, const std::string& frag, const std::vector<Material>& materials)
	{
		assert(pipelineLayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

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
		attrdesc[0].location = 4;
		attrdesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrdesc[0].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::pos);

		attrdesc[1].binding = 1;
		attrdesc[1].location = 5;
		attrdesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrdesc[1].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::rot);

		attrdesc[2].binding = 1;
		attrdesc[2].location = 6;
		attrdesc[2].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[2].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::scale);

		attrdesc[3].binding = 1;
		attrdesc[3].location = 7;
		attrdesc[3].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[3].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::roughness);
		attrdesc[4].binding = 1;
		attrdesc[4].location = 8;
		attrdesc[4].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[4].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::metallic);
		attrdesc[5].binding = 1;
		attrdesc[5].location = 9;
		attrdesc[5].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[5].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::r);
		attrdesc[6].binding = 1;
		attrdesc[6].location = 10;
		attrdesc[6].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[6].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::g);
		attrdesc[7].binding = 1;
		attrdesc[7].location = 11;
		attrdesc[7].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[7].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::b);

		pipelineConfig.attributeDescriptions.insert(pipelineConfig.attributeDescriptions.end(), attrdesc.begin(), attrdesc.end());

		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<Pipeline>(
			device,
			"shaders/shader.vert.spv",
			"shaders/shader.frag.spv",
			pipelineConfig, materials);
	}

	void PBRRendererSystem::createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag)
	{
		assert(pipelineLayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

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
		attrdesc[0].location = 4;
		attrdesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrdesc[0].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::pos);

		attrdesc[1].binding = 1;
		attrdesc[1].location = 5;
		attrdesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrdesc[1].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::rot);

		attrdesc[2].binding = 1;
		attrdesc[2].location = 6;
		attrdesc[2].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[2].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::scale);

		attrdesc[3].binding = 1;
		attrdesc[3].location = 7;
		attrdesc[3].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[3].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::roughness);
		attrdesc[4].binding = 1;
		attrdesc[4].location = 8;
		attrdesc[4].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[4].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::metallic);
		attrdesc[5].binding = 1;
		attrdesc[5].location = 9;
		attrdesc[5].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[5].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::r);
		attrdesc[6].binding = 1;
		attrdesc[6].location = 10;
		attrdesc[6].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[6].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::g);
		attrdesc[7].binding = 1;
		attrdesc[7].location = 11;
		attrdesc[7].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[7].offset = offsetof(JHBApplication::InstanceData, JHBApplication::InstanceData::b);

		pipelineConfig.attributeDescriptions.insert(pipelineConfig.attributeDescriptions.end(), attrdesc.begin(), attrdesc.end());

		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<Pipeline>(
			device,
			"shaders/shader.vert.spv",
			"shaders/shader.frag.spv",
			pipelineConfig);
	}


	void PBRRendererSystem::renderGameObjects(FrameInfo& frameInfo, Buffer* instanceBuffer)
	{
		BaseRenderSystem::renderGameObjects(frameInfo);
		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			1, 1
			, &frameInfo.brdfImageSamplerDescriptorSet,
			0, nullptr
		);
		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			2, 1
			, &frameInfo.irradianceImageSamplerDescriptorSet,
			0, nullptr
		);
		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			3, 1
			, &frameInfo.prefilterImageSamplerDescriptorSet,
			0, nullptr
		);


		for (auto& kv : frameInfo.gameObjects)
		{
			auto& obj = kv.second;

			if (obj.model == nullptr)
			{
				continue;
			}
			SimplePushConstantData push{};
			push.ModelMatrix = obj.transform.mat4();
			push.normalMatrix = obj.transform.normalMatrix();

			if (kv.first == 1)
			{
				vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SimplePushConstantData), &push);

				if (kv.first == 1)
				{
					VkBuffer buffer[1] = { instanceBuffer->getBuffer() };
					obj.model->bind(frameInfo.commandBuffer, buffer);
				}
				else
				{
					obj.model->bind(frameInfo.commandBuffer);
				}
				obj.model->draw(frameInfo.commandBuffer, pipelineLayout, frameInfo.frameIndex, 64);
			}
		}
	}
}