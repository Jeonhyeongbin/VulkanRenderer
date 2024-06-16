#include "DeferedPBRRenderSystem.h"
#include <memory>
#include <array>

namespace jhb {
	DeferedPBRRenderSystem::DeferedPBRRenderSystem(Device& device, std::vector<VkDescriptorSetLayout> descSetlayouts, const std::string& vert, const std::string& frag)
		: BaseRenderSystem(device)
	{
		BaseRenderSystem::createPipeLineLayout(descSetlayouts, { VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) } });
		createOffscreenRenderPass();
		createPipeline(offScreenRenderPass, vert, frag);
		createOffscreenFrameBuffer();
	}

	DeferedPBRRenderSystem::~DeferedPBRRenderSystem()
	{

	}

	void DeferedPBRRenderSystem::createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag)
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
		pipeline = std::make_unique<Pipeline>(
			device,
			"shaders/pbr.vert.spv",
			"shaders/pbr.frag.spv",
			pipelineConfig);
	}

	void DeferedPBRRenderSystem::createAttachment(VkFormat format, VkImageUsageFlagBits usage, Texture* attachment)
	{
		VkImageAspectFlags aspectMask = 0;
		VkImageLayout imageLayout;
		attachment->format = format;

		if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		{
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (format >= VK_FORMAT_D16_UNORM_S8_UINT)
				aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		VkImageCreateInfo imageCIa{};
		imageCIa.imageType = VK_IMAGE_TYPE_2D;
		imageCIa.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCIa.format = format;
		imageCIa.extent.width = 1024;
		imageCIa.extent.height = 1024;
		imageCIa.extent.depth = 1;
		imageCIa.mipLevels = 1;
		imageCIa.arrayLayers = 1;
		imageCIa.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCIa.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCIa.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCIa.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		device.createImageWithInfo(imageCIa, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachment->image, attachment->memory);
		// Image view
		VkImageViewCreateInfo viewCIa{};
		viewCIa.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewCIa.format = format;
		viewCIa.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCIa.subresourceRange = {};
		viewCIa.subresourceRange.aspectMask = aspectMask;
		viewCIa.subresourceRange.baseArrayLayer = 0;
		viewCIa.subresourceRange.baseMipLevel = 0;
		viewCIa.subresourceRange.levelCount = 1;
		viewCIa.subresourceRange.layerCount = 6;
		viewCIa.image = attachment->image;
		if (vkCreateImageView(device.getLogicalDevice(), &viewCIa, nullptr, &attachment->view))
		{
			throw std::runtime_error("failed to create ImageView!");
		}
	}

	void DeferedPBRRenderSystem::createOffscreenFrameBuffer()
	{
		// Color attachments

		// (World space) Positions
		createAttachment(
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			&PositionAttachment);

		// (World space) Normals
		createAttachment(
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			&NormalAttachment);

		// Albedo (color)
		createAttachment(
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			&AlbedoAttachment);

		// Depth attachment

		// Find a suitable depth format
		VkFormat attDepthFormat;
		VkFormat validDepthFormat = device.findSupportedFormat({ VK_FORMAT_D32_SFLOAT_S8_UINT,
	VK_FORMAT_D32_SFLOAT,
	VK_FORMAT_D24_UNORM_S8_UINT,
	VK_FORMAT_D16_UNORM_S8_UINT,
	VK_FORMAT_D16_UNORM
			}, VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
		assert(validDepthFormat);

		createAttachment(
			attDepthFormat,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			&DepthAttachment);

		// Set up separate renderpass with references to the color and depth attachments
		std::array<VkAttachmentDescription, 4> attachmentDescs = {};

		// Init attachment properties
		for (uint32_t i = 0; i < 4; ++i)
		{
			attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			if (i == 3)
			{
				attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
			else
			{
				attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
		}

		// Formats
		attachmentDescs[0].format = PositionAttachment.format;
		attachmentDescs[1].format = NormalAttachment.format;
		attachmentDescs[2].format = AlbedoAttachment.format;
		attachmentDescs[3].format = DepthAttachment.format;

		std::vector<VkAttachmentReference> colorReferences;
		colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 3;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pColorAttachments = colorReferences.data();
		subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
		subpass.pDepthStencilAttachment = &depthReference;

		// Use subpass dependencies for attachment layout transitions
		std::array<VkSubpassDependency, 3> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = 1;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[2].srcSubpass = 1;
		dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.pAttachments = attachmentDescs.data();
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies.data();

		if (vkCreateRenderPass(device.getLogicalDevice(), &renderPassInfo, nullptr, &offScreenRenderPass))
		{
			throw std::runtime_error("failed to create offscreen RenderPass!");
		}

		std::array<VkImageView, 4> attachments;
		attachments[0] = PositionAttachment.view;
		attachments[1] = NormalAttachment.view;
		attachments[2] = AlbedoAttachment.view;
		attachments[3] = DepthAttachment.view;

		VkFramebufferCreateInfo fbufCreateInfo = {};
		fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbufCreateInfo.pNext = NULL;
		fbufCreateInfo.renderPass = offScreenRenderPass;
		fbufCreateInfo.pAttachments = attachments.data();
		fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		fbufCreateInfo.width = device.getWindow().getExtent().width;
		fbufCreateInfo.height = device.getWindow().getExtent().height;
		fbufCreateInfo.layers = 1;
		if (vkCreateFramebuffer(device.getLogicalDevice(), &fbufCreateInfo, nullptr, &OffscreenFrameBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create frameBuffer per face!");
		}
	} 

	std::vector<VkDescriptorSetLayout> DeferedPBRRenderSystem::initializeOffScreenDescriptor()
	{
		descriptorPool = DescriptorPool::Builder(device).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 4).build(); // albedo, normal, material, depth

		descriptorSetLayout = DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_ALL_GRAPHICS).addBinding(1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_ALL_GRAPHICS).
			addBinding(2, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_ALL_GRAPHICS).
			addBinding(3, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_ALL_GRAPHICS).
			build();

		VkDescriptorImageInfo albedoImgInfo{};
		albedoImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		albedoImgInfo.imageView = AlbedoAttachment.view;
		VkDescriptorImageInfo normalImgInfo{};
		normalImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normalImgInfo.imageView = NormalAttachment.view;
		VkDescriptorImageInfo depthImgInfo{};
		depthImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		depthImgInfo.imageView = DepthAttachment.view;
		VkDescriptorImageInfo materialImgInfo{};
		materialImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		materialImgInfo.imageView = MaterialAttachment.view;

		DescriptorWriter(*descriptorSetLayout, *descriptorPool).writeImage(0, &albedoImgInfo).writeImage(1, &normalImgInfo).writeImage(2, &depthImgInfo).writeImage(3, &materialImgInfo).build(descriptorSet);
		return { descriptorSetLayout->getDescriptorSetLayout() };
	}

	std::shared_ptr<Model> DeferedPBRRenderSystem::loadGLTFFile(const std::string& filename)
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
			model->loadImages(glTFInput);
			model->loadMaterials(glTFInput);
			model->loadTextures(glTFInput);
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
		model->updateInstanceBuffer(6, 2.5f, 2.5f);
		return model;
	}

	void DeferedPBRRenderSystem::createDamagedHelmet()
	{
		auto helmet = GameObject::createGameObject();
		helmet.transform.translation = { 0.f, 0.f, 0.f };
		helmet.transform.scale = { 1.f, 1.f, 1.f };
		helmet.transform.rotation = { -glm::radians(90.f), 0.f, 0.f };
		helmet.model = loadGLTFFile("Models/DamagedHelmet/DamagedHelmet.gltf");
		helmet.model->modelMatrix = helmet.transform.mat4();
		helmet.setId(id++);
		pbrObjects.emplace(helmet.getId(), std::move(helmet));
	}

	void DeferedPBRRenderSystem::createFloor(VkRenderPass renderPass)
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
		floor.model->updateInstanceBuffer(1, 0.f, 0.f, 0, 0);

		pbrObjects.emplace(floor.getId(), std::move(floor));
		//this is not gltf model, so using different pipeline 
	}

	void DeferedPBRRenderSystem::renderGameObjects(FrameInfo& frameInfo)
	{

	}
};