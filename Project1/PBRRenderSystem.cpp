#include "PBRRenderSystem.h"
#include "JHBApplication.h"
#include "Model.h"
#include "FrameInfo.h"
#include <memory>
#include <array>

namespace jhb {
	uint32_t PBRRendererSystem::id = 0;

	PBRRendererSystem::PBRRendererSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag) :
		BaseRenderSystem(device, globalSetLayOut, { VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(gltfPushConstantData)} }) {
		createPipeline(renderPass, vert, frag);
		createDamagedHelmet();

		// if pbrobject is gltf model and it has materials then create pipeline per material.
		for (auto& pbrobj : pbrObjects)
		{
			if (!pbrobj.second.model->materials.empty())
			{
				createPipelinePerMaterial(renderPass, vert, frag, pbrobj.second.model->materials);
			}
		}
	}

	PBRRendererSystem::~PBRRendererSystem()
	{
	}

	void PBRRendererSystem::createPipelinePerMaterial(VkRenderPass renderPass, const std::string& vert, const std::string& frag, std::vector<Material>& materials)
	{
		assert(pipelineLayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

		PipelineConfigInfo pipelineConfig{};
		pipelineConfig.depthStencilInfo.depthTestEnable = true;
		pipelineConfig.depthStencilInfo.depthWriteEnable = true;
		pipelineConfig.attributeDescriptions = jhb::Vertex::getAttrivuteDescriptions();
		pipelineConfig.bindingDescriptions = jhb::Vertex::getBindingDescriptions();
		VkVertexInputBindingDescription bindingdesc{};

		bindingdesc.binding = 1;
		bindingdesc.stride = sizeof(jhb::Model::InstanceData);
		bindingdesc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
		
		pipelineConfig.bindingDescriptions.push_back(bindingdesc);

		std::vector<VkVertexInputAttributeDescription> attrdesc(8);

		attrdesc[0].binding = 1;
		attrdesc[0].location = 5;
		attrdesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrdesc[0].offset = offsetof(Model::InstanceData, Model::InstanceData::pos);

		attrdesc[1].binding = 1;
		attrdesc[1].location = 6;
		attrdesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrdesc[1].offset = offsetof(Model::InstanceData, Model::InstanceData::rot);

		attrdesc[2].binding = 1;
		attrdesc[2].location = 7;
		attrdesc[2].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[2].offset = offsetof(Model::InstanceData, Model::InstanceData::scale);

		attrdesc[3].binding = 1;
		attrdesc[3].location = 8;
		attrdesc[3].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[3].offset = offsetof(Model::InstanceData, Model::InstanceData::roughness);
		attrdesc[4].binding = 1;
		attrdesc[4].location = 9;
		attrdesc[4].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[4].offset = offsetof(Model::InstanceData, Model::InstanceData::metallic);
		attrdesc[5].binding = 1;
		attrdesc[5].location = 10;
		attrdesc[5].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[5].offset = offsetof(Model::InstanceData, Model::InstanceData::r);
		attrdesc[6].binding = 1;
		attrdesc[6].location = 11;
		attrdesc[6].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[6].offset = offsetof(Model::InstanceData, Model::InstanceData::g);
		attrdesc[7].binding = 1;
		attrdesc[7].location = 12;
		attrdesc[7].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[7].offset = offsetof(Model::InstanceData, Model::InstanceData::b);

		pipelineConfig.attributeDescriptions.insert(pipelineConfig.attributeDescriptions.end(), attrdesc.begin(), attrdesc.end());

		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
	/*	pipeline = std::make_unique<Pipeline>(
			device,
			"shaders/pbr.vert.spv",
			"shaders/pbr.frag.spv",
			pipelineConfig, materials);*/
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
		bindingdesc.stride = sizeof(jhb::Model::InstanceData);
		bindingdesc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

		pipelineConfig.bindingDescriptions.push_back(bindingdesc);

		std::vector<VkVertexInputAttributeDescription> attrdesc(8);

		attrdesc[0].binding = 1;
		attrdesc[0].location = 5;
		attrdesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrdesc[0].offset = offsetof(Model::InstanceData, Model::InstanceData::pos);

		attrdesc[1].binding = 1;
		attrdesc[1].location = 6;
		attrdesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrdesc[1].offset = offsetof(Model::InstanceData, Model::InstanceData::rot);

		attrdesc[2].binding = 1;
		attrdesc[2].location = 7;
		attrdesc[2].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[2].offset = offsetof(Model::InstanceData, Model::InstanceData::scale);

		attrdesc[3].binding = 1;
		attrdesc[3].location = 8;
		attrdesc[3].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[3].offset = offsetof(Model::InstanceData, Model::InstanceData::roughness);
		attrdesc[4].binding = 1;
		attrdesc[4].location = 9;
		attrdesc[4].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[4].offset = offsetof(Model::InstanceData, Model::InstanceData::metallic);
		attrdesc[5].binding = 1;
		attrdesc[5].location = 10;
		attrdesc[5].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[5].offset = offsetof(Model::InstanceData, Model::InstanceData::r);
		attrdesc[6].binding = 1;
		attrdesc[6].location = 11;
		attrdesc[6].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[6].offset = offsetof(Model::InstanceData, Model::InstanceData::g);
		attrdesc[7].binding = 1;
		attrdesc[7].location = 12;
		attrdesc[7].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[7].offset = offsetof(Model::InstanceData, Model::InstanceData::b);

		pipelineConfig.attributeDescriptions.insert(pipelineConfig.attributeDescriptions.end(), attrdesc.begin(), attrdesc.end());

		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		pipelineConfig.multisampleInfo.rasterizationSamples = device.msaaSamples;
		pipeline = std::make_unique<Pipeline>(
			device,
			"shaders/pbr.vert.spv",
			"shaders/pbr.frag.spv",
			pipelineConfig);
	}

	std::shared_ptr<Model> PBRRendererSystem::loadGLTFFile(const std::string& filename)
	{
		tinygltf::Model glTFInput;
		tinygltf::TinyGLTF gltfContext;
		std::string error, warning;

		bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);

		size_t pos = filename.find_last_of('/');

		std::vector<uint32_t> indexBuffer;
		std::vector<Vertex> vertexBuffer;

		std::shared_ptr<Model> model = std::make_shared<Model>(device);

		model->path = filename.substr(0, pos);

		if (fileLoaded) {
			model->loadMaterials(glTFInput);
			model->loadTextures(glTFInput);
			model->loadImages(glTFInput, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT);
			const tinygltf::Scene& scene = glTFInput.scenes[0];
			for (size_t i = 0; i < scene.nodes.size(); i++) {
				const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
				model->loadNode(node, glTFInput, nullptr, indexBuffer, vertexBuffer);
			}

			if (!model->hasTangent)
			{
				for (int i = 0; i < indexBuffer.size(); i += 3)
				{
					Vertex& vertex1 = vertexBuffer[indexBuffer[i]];
					Vertex& vertex2 = vertexBuffer[indexBuffer[i + 1]];
					Vertex& vertex3 = vertexBuffer[indexBuffer[i + 2]];
					model->calculateTangent(vertex1.uv, vertex2.uv, vertex3.uv, vertex1.position, vertex2.position, vertex3.position, vertex1.tangent);
					model->calculateTangent(vertex2.uv, vertex1.uv, vertex3.uv, vertex2.position, vertex1.position, vertex3.position, vertex2.tangent);
					model->calculateTangent(vertex3.uv, vertex2.uv, vertex1.uv, vertex3.position, vertex2.position, vertex1.position, vertex3.tangent);
				}
			}
		}
		else {
			throw std::runtime_error("Could not open the glTF file.\n\nMake sure the assets submodule has been checked out and is up-to-date.");
			return nullptr;
		}

		model->createVertexBuffer(vertexBuffer);
		model->createIndexBuffer(indexBuffer);
		model->createObjectSphere(vertexBuffer);
		//model->updateInstanceBuffer(6, 2.5f, 2.5f);
		return model;
	}


	void PBRRendererSystem::renderGameObjects(FrameInfo& frameInfo)
	{
		BaseRenderSystem::renderGameObjects(frameInfo);

		for (auto& kv : pbrObjects)
		{
			auto& obj = kv.second;

			if (obj.model == nullptr)
			{
				continue;
			}

			obj.model->bind(frameInfo.commandBuffer);

			if (kv.first == 1)
			{
				auto modelmat = kv.second.transform.mat4();
				vkCmdPushConstants(frameInfo.commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &modelmat);
			}

			obj.model->draw(frameInfo. commandBuffer, pipelineLayout, frameInfo.frameIndex);
		}
	}

	void PBRRendererSystem::createDamagedHelmet()
	{
		auto helmet = GameObject::createGameObject();
		helmet.transform.translation = { 0.f, 0.f, 0.f };
		helmet.transform.scale = { 1.f, 1.f, 1.f };
		helmet.transform.rotation = { -glm::radians(90.f), 0.f, 0.f};
		helmet.model = loadGLTFFile("Models/DamagedHelmet/DamagedHelmet.gltf");
		helmet.setId(id++);
		pbrObjects.emplace(helmet.getId(), std::move(helmet));
	}

	void PBRRendererSystem::createFloor(VkRenderPass renderPass)
	{
		std::shared_ptr<Model> floorModel = std::make_unique<Model>(device);
		floorModel->loadModel("Models/quad.obj");
		auto floor = GameObject::createGameObject();
		floor.model = floorModel;
		floor.transform.translation = { 0.f, 5.f, 0.f };
		floor.transform.scale = { 10.f, 1.f ,10.f };
		floor.transform.rotation = { 0.f, 0.f, 0.f };

		PipelineConfigInfo pipelineconfigInfo{};
		Pipeline::defaultPipelineConfigInfo(pipelineconfigInfo);
		pipelineconfigInfo.depthStencilInfo.depthTestEnable = true;
		pipelineconfigInfo.depthStencilInfo.depthWriteEnable = true;
		pipelineconfigInfo.attributeDescriptions = jhb::Vertex::getAttrivuteDescriptions();
		pipelineconfigInfo.bindingDescriptions = jhb::Vertex::getBindingDescriptions();

		VkVertexInputBindingDescription bindingdesc{};

		bindingdesc.binding = 1;
		bindingdesc.stride = sizeof(jhb::Model::InstanceData);
		bindingdesc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

		pipelineconfigInfo.bindingDescriptions.push_back(bindingdesc);

		std::vector<VkVertexInputAttributeDescription> attrdesc(8);

		attrdesc[0].binding = 1;
		attrdesc[0].location = 5;
		attrdesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrdesc[0].offset = offsetof(Model::InstanceData, Model::InstanceData::pos);

		attrdesc[1].binding = 1;
		attrdesc[1].location = 6;
		attrdesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attrdesc[1].offset = offsetof(Model::InstanceData, Model::InstanceData::rot);

		attrdesc[2].binding = 1;
		attrdesc[2].location = 7;
		attrdesc[2].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[2].offset = offsetof(Model::InstanceData, Model::InstanceData::scale);

		attrdesc[3].binding = 1;
		attrdesc[3].location = 8;
		attrdesc[3].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[3].offset = offsetof(Model::InstanceData, Model::InstanceData::roughness);
		attrdesc[4].binding = 1;
		attrdesc[4].location = 9;
		attrdesc[4].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[4].offset = offsetof(Model::InstanceData, Model::InstanceData::metallic);
		attrdesc[5].binding = 1;
		attrdesc[5].location = 10;
		attrdesc[5].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[5].offset = offsetof(Model::InstanceData, Model::InstanceData::r);
		attrdesc[6].binding = 1;
		attrdesc[6].location = 11;
		attrdesc[6].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[6].offset = offsetof(Model::InstanceData, Model::InstanceData::g);
		attrdesc[7].binding = 1;
		attrdesc[7].location = 12;
		attrdesc[7].format = VK_FORMAT_R32_SFLOAT;
		attrdesc[7].offset = offsetof(Model::InstanceData, Model::InstanceData::b);

		pipelineconfigInfo.attributeDescriptions.insert(pipelineconfigInfo.attributeDescriptions.end(), attrdesc.begin(), attrdesc.end());

		pipelineconfigInfo.renderPass = renderPass;
		pipelineconfigInfo.pipelineLayout = pipelineLayout;
		pipelineconfigInfo.multisampleInfo.rasterizationSamples = device.msaaSamples;
		floor.setId(id++);
		floor.model->createPipelineForModel("shaders/pbr.vert.spv",
			"shaders/pbrnotexture.frag.spv", pipelineconfigInfo);
		//floor.model->updateInstanceBuffer(1, 0.f ,0.f,0,0);

		pbrObjects.emplace(floor.getId(), std::move(floor));
		//this is not gltf model, so using different pipeline 
	}
}