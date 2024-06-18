#include "DeferedPBRRenderSystem.h"
#include <memory>
#include <array>

namespace jhb {
	uint32_t DeferedPBRRenderSystem::id = 0;
	// descriptortsetlayouts =>  { globaluniform, gltfmaterial,pbrresource, shadow } 
	DeferedPBRRenderSystem::DeferedPBRRenderSystem(Device& device, std::vector<VkDescriptorSetLayout> descSetlayouts, const std::vector<VkImageView>& swapchainImageViews, VkFormat swapchainFormat)
		: BaseRenderSystem(device)
	{
		assert(descSetlayouts.size() == 4 && "descriptor setlayout size in defered render system less than 4!!!!!!!");
		// 첫번째 subpass는 gltf모델의 머터리얼용 descriptorsetlayout을 첫번쨰 subpass용 pipelinelayout에 묶어 주어야함. 그러므로 uniform buffer 와 함께 총 2개의 descriptor set layout이 필요.
		createRenderPass(swapchainFormat);
		initializeOffScreenDescriptor();

		BaseRenderSystem::createPipeLineLayout({ descSetlayouts[0],descSetlayouts[1], descSetlayouts[1] }, { VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)} });
		createPipeline(nullptr, "shaders/deferedoffscreen.vert.spv",
			"shaders/deferedoffscreen.frag.spv");
		// 두번째 subpass는 pbr을 해야하기 때문에 pbrresource용 descriptorsetlayout을 두번째 subpass용 pipelinelayout에 묶어 주어야함. 이 때도 유니폼 버퍼가 필요하고(light 위치), pbr 이미지용 descriptor set layout, 
		// 쉐도우용 descriptor set layout 3개 필요.
		createLightingPipelineAndPipelinelayout({descSetlayouts[0], descSetlayouts[2], descSetlayouts[3] }); // second subapss용
		createFrameBuffers(swapchainImageViews);
		createDamagedHelmet();
		createFloor();
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
		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		createVertexAttributeAndBindingDesc(pipelineConfig);

		std::array<VkPipelineColorBlendAttachmentState, 5> blendAttachmentStates;
		for (int i = 0; i < 5; i++)
		{
			VkPipelineColorBlendAttachmentState colorblendState{};
			colorblendState.blendEnable = VK_FALSE;
			colorblendState.colorWriteMask = 0xf;
			blendAttachmentStates[i] = colorblendState;
		}

		pipelineConfig.colorBlendInfo.attachmentCount = blendAttachmentStates.size();
		pipelineConfig.colorBlendInfo.pAttachments = blendAttachmentStates.data();
		pipelineConfig.renderPass = offScreenRenderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<Pipeline>(
			device,
			vert,
			frag,
			pipelineConfig);
	}

	void DeferedPBRRenderSystem::createGBuffers()
	{
		createAttachment(
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			&PositionAttachment);

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

		// Albedo (color)
		createAttachment(
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			&MaterialAttachment);

		createAttachment(
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			&EmmisiveAttachment);

		// Depth attachment

		// Find a suitable depth format
		VkFormat attDepthFormat;
		VkFormat validDepthFormat = device.findSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
		//assert(validDepthFormat);

		createAttachment(
			validDepthFormat,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			&DepthAttachment);
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
		imageCIa.extent.width = device.getWindow().getExtent().width;
		imageCIa.extent.height = device.getWindow().getExtent().height;
		imageCIa.extent.depth = 1;
		imageCIa.mipLevels = 1;
		imageCIa.arrayLayers = 1;
		imageCIa.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCIa.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCIa.usage = usage | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		device.createImageWithInfo(imageCIa, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, attachment->image, attachment->memory);
		// Image view
		VkImageViewCreateInfo viewCIa{};
		viewCIa.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCIa.format = format;
		viewCIa.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCIa.subresourceRange = {};
		viewCIa.subresourceRange.aspectMask = aspectMask;
		viewCIa.subresourceRange.baseArrayLayer = 0;
		viewCIa.subresourceRange.baseMipLevel = 0;
		viewCIa.subresourceRange.levelCount = 1;
		viewCIa.subresourceRange.layerCount = 1;
		viewCIa.image = attachment->image;

		if (vkCreateImageView(device.getLogicalDevice(), &viewCIa, nullptr, &attachment->view))
		{
			throw std::runtime_error("failed to create ImageView!");
		}
	}

	void DeferedPBRRenderSystem::createFrameBuffers(const std::vector<VkImageView>& swapchainImageViews)
	{
		std::array<VkImageView, 7> attachments;
		attachments[1] = PositionAttachment.view;
		attachments[2] = NormalAttachment.view;
		attachments[3] = AlbedoAttachment.view;
		attachments[4] = MaterialAttachment.view;
		attachments[5] = EmmisiveAttachment.view;
		attachments[6] = DepthAttachment.view;

		frameBuffers.resize(swapchainImageViews.size());
		for (int i = 0; i < swapchainImageViews.size(); i++)
		{
			attachments[0] = swapchainImageViews[i];
			VkFramebufferCreateInfo fbufCreateInfo = {};
			fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbufCreateInfo.pNext = NULL;
			fbufCreateInfo.renderPass = offScreenRenderPass;
			fbufCreateInfo.pAttachments = attachments.data();
			fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			fbufCreateInfo.width = device.getWindow().getExtent().width;
			fbufCreateInfo.height = device.getWindow().getExtent().height;
			fbufCreateInfo.layers = 1;
			if (vkCreateFramebuffer(device.getLogicalDevice(), &fbufCreateInfo, nullptr, &frameBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create frameBuffer per face!");
			}
		}
	}

	void DeferedPBRRenderSystem::createRenderPass(VkFormat swapchianFormat)
	{
		createGBuffers();

		// Set up separate renderpass with references to the color and depth attachments
		std::array<VkAttachmentDescription, 7> attachmentDescs = {};

		// Init attachment properties
		for (uint32_t i = 0; i <7; ++i)
		{
			attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			if (i == 6)
			{
				attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
			else if (i == 0)
			{
				attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			}
			else
			{
				attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
		}

		// Formats
		attachmentDescs[0].format = swapchianFormat;
		attachmentDescs[1].format = PositionAttachment.format;
		attachmentDescs[2].format = NormalAttachment.format;
		attachmentDescs[3].format = AlbedoAttachment.format;
		attachmentDescs[4].format = MaterialAttachment.format;
		attachmentDescs[5].format = EmmisiveAttachment.format;
		attachmentDescs[6].format = DepthAttachment.format;

		std::vector<VkAttachmentReference> colorReferences;
		colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		colorReferences.push_back({ 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		colorReferences.push_back({ 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		colorReferences.push_back({ 5, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 6;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		std::array<VkSubpassDescription, 2> subpassDescriptions{};

		subpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescriptions[0].pColorAttachments = colorReferences.data();
		subpassDescriptions[0].colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
		subpassDescriptions[0].pDepthStencilAttachment = &depthReference;

		VkAttachmentReference colorReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		std::vector<VkAttachmentReference> colorInputReferences;
		colorInputReferences.push_back({ 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
		colorInputReferences.push_back({ 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
		colorInputReferences.push_back({ 3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
		colorInputReferences.push_back({ 4, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
		colorInputReferences.push_back({ 5, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });

		// light subpass will use inputattachemnts
		subpassDescriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescriptions[1].pColorAttachments = &colorReference;
		subpassDescriptions[1].colorAttachmentCount = 1;
		subpassDescriptions[1].pDepthStencilAttachment = &depthReference;
		subpassDescriptions[1].inputAttachmentCount = colorInputReferences.size();
		subpassDescriptions[1].pInputAttachments = colorInputReferences.data();

		// Subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 4> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;;
		dependencies[0].srcAccessMask = 0;
		dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = 0;

		dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].dstSubpass = 0;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].srcAccessMask = 0;
		dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[2].srcSubpass = 0;
		dependencies[2].dstSubpass = 1;
		dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[2].srcAccessMask =  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[2].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[3].srcSubpass = 1;
		dependencies[3].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[3].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[3].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[3].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[3].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[3].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.pAttachments = attachmentDescs.data();
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
		renderPassInfo.subpassCount = subpassDescriptions.size();
		renderPassInfo.pSubpasses = subpassDescriptions.data();
		renderPassInfo.dependencyCount = dependencies.size();
		renderPassInfo.pDependencies = dependencies.data();

		if (vkCreateRenderPass(device.getLogicalDevice(), &renderPassInfo, nullptr, &offScreenRenderPass))
		{
			throw std::runtime_error("failed to create offscreen RenderPass!");
		}
	} 

	std::vector<VkDescriptorSetLayout> DeferedPBRRenderSystem::initializeOffScreenDescriptor()
	{
		gbufferDescriptorPool = DescriptorPool::Builder(device).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 5).build(); // albedo, normal, material, depth

		gbufferDescriptorSetLayout = DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT).addBinding(1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT).
			addBinding(2, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT).
			addBinding(3, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT).addBinding(4, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT).
			build();

		VkDescriptorImageInfo positionImgInfo{};
		positionImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		positionImgInfo.imageView = PositionAttachment.view;
		VkDescriptorImageInfo normalImgInfo{};
		normalImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normalImgInfo.imageView = NormalAttachment.view;
		VkDescriptorImageInfo albedoImgInfo{};
		albedoImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		albedoImgInfo.imageView = AlbedoAttachment.view;
		VkDescriptorImageInfo materialImgInfo{};
		materialImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		materialImgInfo.imageView = MaterialAttachment.view;
		VkDescriptorImageInfo emmsiveImgInfo{};
		emmsiveImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		emmsiveImgInfo.imageView = EmmisiveAttachment.view;

		DescriptorWriter(*gbufferDescriptorSetLayout, *gbufferDescriptorPool).writeImage(0, &positionImgInfo).writeImage(1, &normalImgInfo).writeImage(2, &albedoImgInfo).writeImage(3, &materialImgInfo)
			.writeImage(4, &emmsiveImgInfo).build(gbufferDescriptorSet);
		return { gbufferDescriptorSetLayout->getDescriptorSetLayout() };
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

		PipelineConfigInfo pipelineConfig{};
		pipelineConfig.depthStencilInfo.depthTestEnable = true;
		pipelineConfig.depthStencilInfo.depthWriteEnable = true;
		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		createVertexAttributeAndBindingDesc(pipelineConfig);
		std::array<VkPipelineColorBlendAttachmentState, 5> blendAttachmentStates;
		for (int i = 0; i < 5; i++)
		{
			VkPipelineColorBlendAttachmentState colorblendState{};
			colorblendState.blendEnable = VK_FALSE;
			colorblendState.colorWriteMask = 0xf;
			blendAttachmentStates[i] = colorblendState;
		}

		pipelineConfig.colorBlendInfo.attachmentCount = blendAttachmentStates.size();
		pipelineConfig.colorBlendInfo.pAttachments = blendAttachmentStates.data();

		pipelineConfig.renderPass = offScreenRenderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		model->createGraphicsPipelinePerMaterial("shaders/deferedoffscreen.vert.spv",
			"shaders/deferedoffscreen.frag.spv", pipelineConfig);
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

	void DeferedPBRRenderSystem::createFloor()
	{
		std::shared_ptr<Model> floorModel = std::make_unique<Model>(device);
		floorModel->loadModel("Models/quad.obj");
		auto floor = GameObject::createGameObject();
		floor.model = floorModel;
		floor.transform.translation = { 0.f, 5.f, 0.f };
		floor.transform.scale = { 10.f, 1.f ,10.f };
		floor.transform.rotation = { 0.f, 0.f, 0.f };

		floor.setId(id++);
		floor.model->updateInstanceBuffer(1, 0.f, 0.f, 0, 0);

		pbrObjects.emplace(floor.getId(), std::move(floor));
		//this is not gltf model, so using different pipeline 
	}

	void DeferedPBRRenderSystem::createVertexAttributeAndBindingDesc(PipelineConfigInfo& pipelineConfig)
	{
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
	}

	void DeferedPBRRenderSystem::createLightingPipelineAndPipelinelayout(const std::vector<VkDescriptorSetLayout>& externDescsetlayout)
	{
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		std::vector<VkDescriptorSetLayout> descriptorsetlayouts = { gbufferDescriptorSetLayout->getDescriptorSetLayout() };
		descriptorsetlayouts.insert(descriptorsetlayouts.end(), externDescsetlayout.begin(), externDescsetlayout.end());
		pipelineLayoutInfo.setLayoutCount = descriptorsetlayouts.size();
		pipelineLayoutInfo.pSetLayouts = descriptorsetlayouts.data();
		if (vkCreatePipelineLayout(device.getLogicalDevice(), &pipelineLayoutInfo, nullptr, &lightingPipelinelayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}

		PipelineConfigInfo pipelineConfig{};
		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.depthStencilInfo.depthTestEnable = true;
		pipelineConfig.depthStencilInfo.depthWriteEnable = true;
		pipelineConfig.subpass = 1;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(1);
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, position);
		
		std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
		bindingDescriptions[0].binding = 0;
		bindingDescriptions[0].stride = sizeof(glm::vec3);
		bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		pipelineConfig.attributeDescriptions = attributeDescriptions;
		pipelineConfig.bindingDescriptions = bindingDescriptions;

		pipelineConfig.renderPass = offScreenRenderPass;
		pipelineConfig.pipelineLayout = lightingPipelinelayout;
		lightingPipeline = std::make_unique<Pipeline>(device, "shaders/deferedPBR.vert.spv",
			"shaders/deferedPBR.frag.spv", pipelineConfig); 
	}

	void DeferedPBRRenderSystem::renderGameObjects(FrameInfo& frameInfo)
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

			obj.model->draw(frameInfo.commandBuffer, pipelineLayout, frameInfo.frameIndex);
		}

		vkCmdNextSubpass(frameInfo.commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipeline->getPipeline());
		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			lightingPipelinelayout,
			0, 1
			, &gbufferDescriptorSet,
			0, nullptr
		);
		vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipelinelayout, 1, 1, &frameInfo.globaldDescriptorSet, 0, NULL);
		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			lightingPipelinelayout,
			2, 1
			, &frameInfo.pbrImageSamplerDescriptorSet,
			0, nullptr
		);
		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			lightingPipelinelayout,
			3, 1
			, &frameInfo.shadowMapDescriptorSet,
			0, nullptr
		);
		vkCmdDraw(frameInfo.commandBuffer, 3, 1, 0, 0);
	}
};