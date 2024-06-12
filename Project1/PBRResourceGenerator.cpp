#include "PBRResourceGenerator.h"

jhb::PBRResourceGenerator::PBRResourceGenerator(Device& _device, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::vector<VkDescriptorSet>& _descSets)
	: device(_device), descSetlayouts(globalSetLayOut), descSets(_descSets)
{
	createCube();
	// need to multiple pipelinelayout
}

jhb::PBRResourceGenerator::~PBRResourceGenerator()
{
	vkDestroyImage(device.getLogicalDevice(), preFilterCubeImg, nullptr);
	vkDestroyImage(device.getLogicalDevice(), IrradianceCubeImg, nullptr);
	vkDestroyImage(device.getLogicalDevice(), lutBrdfImg, nullptr);
}


void jhb::PBRResourceGenerator::createCube()
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

	std::shared_ptr<Model> cubeModel = std::make_unique<Model>(device);
	cubeModel->createVertexBuffer(vertices);
	cubeModel->createIndexBuffer(indices);
	cubeModel->getTexture(0).loadKTXTexture(device, "Texture/pisa_cube.ktx", VK_IMAGE_VIEW_TYPE_CUBE, 6);
	cube = std::make_unique<GameObject>(GameObject::createGameObject());
	cube->model = cubeModel;
	cube->transform.translation = { 0.f, 0.f, 0.f };
	cube->transform.scale = { 10.f, 10.f ,10.f };
}

void jhb::PBRResourceGenerator::createPBRResource()
{
	generateBRDFLUT();
	generateIrradianceCube();
	generatePrefilteredCube();
}

void jhb::PBRResourceGenerator::generateBRDFLUT()
{
	const VkFormat format = VK_FORMAT_R16G16_SFLOAT;	// R16G16 is supported pretty much everywhere
	const int32_t dim = 512;

	DescriptorPool::Builder(device).setMaxSets(1)
		.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1).build();

	auto descriptorSetLayout = DescriptorSetLayout::Builder(device)
		.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS).
		build();
	// no apply descriptorset to descriptorsetlayout but allcoate descriptorsetlayout and descritptor pool in william sascha example

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	if (vkCreatePipelineLayout(device.getLogicalDevice(), &pipelineLayoutInfo, nullptr, &brdfPipelinelayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	std::vector<VkSubpassDependency> tmpdependencies(2);
	tmpdependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	tmpdependencies[0].dstSubpass = 0;
	tmpdependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	tmpdependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	tmpdependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	tmpdependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	tmpdependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	tmpdependencies[1].srcSubpass = 0;
	tmpdependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	tmpdependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	tmpdependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	tmpdependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	tmpdependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	tmpdependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkImageCreateInfo imageCIa{};
	imageCIa.imageType = VK_IMAGE_TYPE_2D;
	imageCIa.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCIa.format = format;
	imageCIa.extent.width = dim;
	imageCIa.extent.height = dim;
	imageCIa.extent.depth = 1;
	imageCIa.mipLevels = 1;
	imageCIa.arrayLayers = 1;
	imageCIa.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCIa.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCIa.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	device.createImageWithInfo(imageCIa, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, lutBrdfImg, lutBrdfMemory);
	// Image view
	VkImageViewCreateInfo viewCIa{};
	viewCIa.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCIa.format = format;
	viewCIa.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCIa.subresourceRange = {};
	viewCIa.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCIa.subresourceRange.levelCount = 1;
	viewCIa.subresourceRange.layerCount = 1;
	viewCIa.image = lutBrdfImg;
	if (vkCreateImageView(device.getLogicalDevice(), &viewCIa, nullptr, &lutBrdfView))
	{
		throw std::runtime_error("failed to create ImageView!");
	}

	// Sampler
	VkSamplerCreateInfo samplerCI{};
	samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCI.magFilter = VK_FILTER_LINEAR;
	samplerCI.minFilter = VK_FILTER_LINEAR;
	samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.minLod = 0.0f;
	samplerCI.maxLod = 1.f;
	samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	if (vkCreateSampler(device.getLogicalDevice(), &samplerCI, nullptr, &lutBrdfSampler))
	{
		throw std::runtime_error("failed to create Sampler!");
	}

	std::vector<VkImageView> attachments = { lutBrdfView };


	std::vector<VkPushConstantRange> pushConstantRanges;

	VkAttachmentDescription attDesc = {};
	// Color attachment
	attDesc.format = format;
	attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;

	// Renderpass
	VkRenderPassCreateInfo renderPassCI{};
	renderPassCI.attachmentCount = 1;
	renderPassCI.pAttachments = &attDesc;
	renderPassCI.subpassCount = 1;
	renderPassCI.pSubpasses = &subpassDescription;
	renderPassCI.dependencyCount = 2;
	renderPassCI.pDependencies = tmpdependencies.data();
	renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	VkRenderPass renderpass;
	if (vkCreateRenderPass(device.getLogicalDevice(), &renderPassCI, nullptr, &renderpass))
	{
		throw std::runtime_error("failed to create renderpass!");
	}

	assert(brdfPipelinelayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

	PipelineConfigInfo pipelineConfig{};
	pipelineConfig.depthStencilInfo.depthTestEnable = true;
	pipelineConfig.depthStencilInfo.depthWriteEnable = true;
	pipelineConfig.attributeDescriptions = jhb::Vertex::getAttrivuteDescriptions();
	pipelineConfig.bindingDescriptions = jhb::Vertex::getBindingDescriptions();

	Pipeline::defaultPipelineConfigInfo(pipelineConfig);
	pipelineConfig.renderPass = renderpass;
	pipelineConfig.pipelineLayout = brdfPipelinelayout;
	brdfPipeline = std::make_unique<Pipeline>(
		device,
		"shaders/genbrdflut.vert.spv",
		"shaders/genbrdflut.frag.spv",
		pipelineConfig);


	VkFramebufferCreateInfo fbufCreateInfo{};
	fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbufCreateInfo.renderPass = renderpass;
	fbufCreateInfo.attachmentCount = attachments.size();
	fbufCreateInfo.pAttachments = attachments.data();
	fbufCreateInfo.width = dim;
	fbufCreateInfo.height = dim;
	fbufCreateInfo.layers = 1;

	VkFramebuffer frameBuffer;
	if (vkCreateFramebuffer(device.getLogicalDevice(), &fbufCreateInfo, nullptr, &frameBuffer))
	{
		throw std::runtime_error("failed to create frameBuffer!");
	}

	auto cmd = device.beginSingleTimeCommands();

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderpass;
	renderPassInfo.framebuffer = frameBuffer;

	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = { (uint32_t)dim, (uint32_t)dim };

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { 0.01f, 0.1f, 0.1f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };
	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();


	vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (uint32_t)dim;
	viewport.height = (uint32_t)dim;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor{ {0, 0}, {(uint32_t)dim, (uint32_t)dim} };
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	brdfPipeline->bind(cmd);

	std::vector<Vertex> vertices = {
			{{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
	  {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},
	  {{-.5f, -.5f, .5f}, {.9f, .9f, .9f}},

	};

	std::vector<uint32_t> indices = { 0,  1,  2 };

	std::shared_ptr<Model> quad = std::make_unique<Model>(device);
	quad->createVertexBuffer(vertices);
	quad->createIndexBuffer(indices);
	quad->getTexture(0).loadKTXTexture(device, "Texture/pisa_cube.ktx", VK_IMAGE_VIEW_TYPE_CUBE, 6);
	auto brdfmodel = GameObject::createGameObject();
	brdfmodel.model = quad;
	brdfmodel.transform.translation = { 0.f, 0.f, 0.f };
	brdfmodel.transform.scale = { 10.f, 10.f ,10.f };

	brdfmodel.model->bind(cmd);
	brdfmodel.model->draw(cmd, brdfPipelinelayout, 0);

	vkCmdEndRenderPass(cmd);

	device.endSingleTimeCommands(cmd);
}

void jhb::PBRResourceGenerator::generateIrradianceCube()
{
	const VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
	const int32_t dim = 64;
	const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

	std::vector<VkSubpassDependency> tmpdependencies(2);
	tmpdependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	tmpdependencies[0].dstSubpass = 0;
	tmpdependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	tmpdependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	tmpdependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	tmpdependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	tmpdependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	tmpdependencies[1].srcSubpass = 0;
	tmpdependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	tmpdependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	tmpdependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	tmpdependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	tmpdependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	tmpdependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	IrradiencePushBlock pushBlock;

	std::vector<VkPushConstantRange> pushConstantRanges;
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // This means that both vertex and fragment shader using constant 
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(IrradiencePushBlock);
	pushConstantRanges.push_back(pushConstantRange);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = descSetlayouts.size();
	pipelineLayoutInfo.pSetLayouts = descSetlayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = pushConstantRanges.size();
	pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();

	if (vkCreatePipelineLayout(device.getLogicalDevice(), &pipelineLayoutInfo, nullptr, &irradiancePipelinelayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}

	VkImage cubeImage;
	VkDeviceMemory cubeMemory;
	VkImageView cubeImageView;
	VkSampler cubeSampler;

	// Pre-filtered cube map
	// Image
	VkImageCreateInfo imageCIa{};
	imageCIa.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCIa.imageType = VK_IMAGE_TYPE_2D;
	imageCIa.format = format;
	imageCIa.extent.width = dim;
	imageCIa.extent.height = dim;
	imageCIa.extent.depth = 1;
	imageCIa.mipLevels = numMips;
	imageCIa.arrayLayers = 6;
	imageCIa.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCIa.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCIa.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCIa.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	device.createImageWithInfo(imageCIa, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, IrradianceCubeImg, IrradianceCubeMemory);
	// Image view
	VkImageViewCreateInfo viewCIa{};
	viewCIa.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCIa.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	viewCIa.format = format;
	viewCIa.subresourceRange = {};
	viewCIa.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCIa.subresourceRange.levelCount = numMips;
	viewCIa.subresourceRange.layerCount = 6;
	viewCIa.image = IrradianceCubeImg;
	if (vkCreateImageView(device.getLogicalDevice(), &viewCIa, nullptr, &IrradianceCubeImgView))
	{
		throw std::runtime_error("failed to create ImageView!");
	}

	// Sampler
	VkSamplerCreateInfo samplerCI{};
	samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCI.magFilter = VK_FILTER_LINEAR;
	samplerCI.minFilter = VK_FILTER_LINEAR;
	samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.minLod = 0.0f;
	samplerCI.maxLod = static_cast<float>(numMips);
	samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	if (vkCreateSampler(device.getLogicalDevice(), &samplerCI, nullptr, &IrradianceCubeSampler))
	{
		throw std::runtime_error("failed to create Sampler!");
	}

	// offscreen cube for hdr cube textre 2d
	VkImage offscreenImage;
	VkDeviceMemory offscreenMemory;
	VkImageView offscreenImageView;

	VkFramebuffer frameBuffer;
	VkRenderPass renderpass;
	{
		// Pre-filtered cube map
		// Image
		VkImageCreateInfo imageCI{};
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.format = format;
		imageCI.extent.width = dim;
		imageCI.extent.height = dim;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 6;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		device.createImageWithInfo(imageCI, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreenImage, offscreenMemory);
		// Image view
		VkImageViewCreateInfo viewCI{};
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCI.format = format;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = 1;
		viewCI.subresourceRange.layerCount = 1;
		viewCI.image = offscreenImage;
		if (vkCreateImageView(device.getLogicalDevice(), &viewCI, nullptr, &offscreenImageView))
		{
			throw std::runtime_error("failed to create ImageView!");
		}

		VkAttachmentDescription attDesc = {};
		// Color attachment
		attDesc.format = format;
		attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;

		// Renderpass
		VkRenderPassCreateInfo renderPassCI{};
		renderPassCI.attachmentCount = 1;
		renderPassCI.pAttachments = &attDesc;
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDescription;
		renderPassCI.dependencyCount = 2;
		renderPassCI.pDependencies = tmpdependencies.data();
		renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

		if (vkCreateRenderPass(device.getLogicalDevice(), &renderPassCI, nullptr, &renderpass))
		{
			throw std::runtime_error("failed to create renderpass!");
		}

		std::vector<VkImageView> attachments = { offscreenImageView };

		VkFramebufferCreateInfo fbufCreateInfo{};
		fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbufCreateInfo.renderPass = renderpass;
		fbufCreateInfo.attachmentCount = attachments.size();
		fbufCreateInfo.pAttachments = attachments.data();
		fbufCreateInfo.width = dim;
		fbufCreateInfo.height = dim;
		fbufCreateInfo.layers = 1;

		if (vkCreateFramebuffer(device.getLogicalDevice(), &fbufCreateInfo, nullptr, &frameBuffer))
		{
			throw std::runtime_error("failed to create frameBuffer!");
		}

		VkCommandBuffer cmd = device.beginSingleTimeCommands();
		device.transitionImageLayout(cmd, offscreenImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		device.endSingleTimeCommands(cmd);
	}

	assert(irradiancePipelinelayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

	PipelineConfigInfo pipelineConfig{};
	pipelineConfig.depthStencilInfo.depthTestEnable = true;
	pipelineConfig.depthStencilInfo.depthWriteEnable = true;
	pipelineConfig.attributeDescriptions = jhb::Vertex::getAttrivuteDescriptions();
	pipelineConfig.bindingDescriptions = jhb::Vertex::getBindingDescriptions();

	Pipeline::defaultPipelineConfigInfo(pipelineConfig);
	pipelineConfig.renderPass = renderpass;
	pipelineConfig.pipelineLayout = irradiancePipelinelayout;
	irradiancePipeline = std::make_unique<Pipeline>(
		device,
		"shaders/filtercube.vert.spv",
		"shaders/irradiancecube.frag.spv",
		pipelineConfig);

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderpass;
	renderPassInfo.framebuffer = frameBuffer;

	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = { dim, dim };

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { 0.01f, 0.1f, 0.1f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };
	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	std::vector<glm::mat4> matrices = {
		// POSITIVE_X
		glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_X
		glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// POSITIVE_Y
		glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_Y
		glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// POSITIVE_Z
		glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_Z
		glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
	};

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = numMips;
	subresourceRange.layerCount = 6;

	auto cmdBuf = device.beginSingleTimeCommands();
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(dim);
	viewport.height = static_cast<float>(dim);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor{ {0, 0}, {(uint32_t)dim, (uint32_t)dim} };
	vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuf, 0, 1, &scissor);


	device.transitionImageLayout(cmdBuf, IrradianceCubeImg, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

	for (uint32_t m = 0; m < numMips; m++) {
		for (uint32_t f = 0; f < 6; f++) {
			viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
			viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
			vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

			// Render scene from cube face's point of view
			vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			// Update shader push constant block
			pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

			/*irradianceCubeGenerator.bindPipeline(cmdBuf);
			irradianceCubeGenerator.generateIrradianceCube(cmdBuf, gameObjects[0], descSets, pushBlock);*/

			irradiancePipeline->bind(cmdBuf);

			vkCmdBindDescriptorSets(
				cmdBuf,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				irradiancePipelinelayout,
				0, 1
				, &descSets[0],
				0, nullptr
			);

			vkCmdBindDescriptorSets(
				cmdBuf,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				irradiancePipelinelayout,
				1, 1
				, &descSets[1],
				0, nullptr
			);

			vkCmdPushConstants(cmdBuf, irradiancePipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(IrradiencePushBlock), &pushBlock);
			cube->model->bind(cmdBuf);
			cube->model->draw(cmdBuf, irradiancePipelinelayout, 0);
			
			vkCmdEndRenderPass(cmdBuf);

			device.transitionImageLayout(
				cmdBuf, offscreenImage, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			// Copy region for transfer from framebuffer to cube face
			VkImageCopy copyRegion = {};

			copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.srcSubresource.baseArrayLayer = 0;
			copyRegion.srcSubresource.mipLevel = 0;
			copyRegion.srcSubresource.layerCount = 1;
			copyRegion.srcOffset = { 0, 0, 0 };

			copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.dstSubresource.baseArrayLayer = f;
			copyRegion.dstSubresource.mipLevel = m;
			copyRegion.dstSubresource.layerCount = 1;
			copyRegion.dstOffset = { 0, 0, 0 };

			copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
			copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
			copyRegion.extent.depth = 1;

			vkCmdCopyImage(
				cmdBuf,
				offscreenImage,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				IrradianceCubeImg,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&copyRegion);

			// Transform framebuffer color attachment back

			device.transitionImageLayout(
				cmdBuf, offscreenImage, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			);
		}
	}

	device.transitionImageLayout(
		cmdBuf, IrradianceCubeImg,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange
	);
	device.endSingleTimeCommands(cmdBuf);
}

void jhb::PBRResourceGenerator::generatePrefilteredCube()
{
	std::vector<VkPushConstantRange> pushConstantRanges;
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // This means that both vertex and fragment shader using constant 
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(PrefileterPushBlock);
	pushConstantRanges.push_back(pushConstantRange);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = descSetlayouts.size();
	pipelineLayoutInfo.pSetLayouts = descSetlayouts.data();

	pipelineLayoutInfo.pushConstantRangeCount = pushConstantRanges.size();
	pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
	if (vkCreatePipelineLayout(device.getLogicalDevice(), &pipelineLayoutInfo, nullptr, &prefilterPipelinelayout) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create pipeline layout!");
	}


	const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
	const int32_t dim = 512;
	const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

	std::vector<VkSubpassDependency> tmpdependencies(2);
	tmpdependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	tmpdependencies[0].dstSubpass = 0;
	tmpdependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	tmpdependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	tmpdependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	tmpdependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	tmpdependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	tmpdependencies[1].srcSubpass = 0;
	tmpdependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	tmpdependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	tmpdependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	tmpdependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	tmpdependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	tmpdependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	PrefileterPushBlock pushBlock;

	VkImage cubeImage;
	VkDeviceMemory cubeMemory;
	VkImageView cubeImageView;
	VkSampler cubeSampler;

	// Pre-filtered cube map
	// Image
	VkImageCreateInfo imageCIa{};
	imageCIa.imageType = VK_IMAGE_TYPE_2D;
	imageCIa.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCIa.format = format;
	imageCIa.extent.width = dim;
	imageCIa.extent.height = dim;
	imageCIa.extent.depth = 1;
	imageCIa.mipLevels = numMips;
	imageCIa.arrayLayers = 6;
	imageCIa.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCIa.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCIa.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCIa.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	device.createImageWithInfo(imageCIa, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, preFilterCubeImg, preFilterCubeMemory);
	// Image view
	VkImageViewCreateInfo viewCIa{};
	viewCIa.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	viewCIa.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCIa.format = format;
	viewCIa.subresourceRange = {};
	viewCIa.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCIa.subresourceRange.levelCount = numMips;
	viewCIa.subresourceRange.layerCount = 6;
	viewCIa.image = preFilterCubeImg;
	if (vkCreateImageView(device.getLogicalDevice(), &viewCIa, nullptr, &preFilterCubeImgView))
	{
		throw std::runtime_error("failed to create ImageView!");
	}

	// Sampler
	VkSamplerCreateInfo samplerCI{};
	samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCI.magFilter = VK_FILTER_LINEAR;
	samplerCI.minFilter = VK_FILTER_LINEAR;
	samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.minLod = 0.0f;
	samplerCI.maxLod = static_cast<float>(numMips);
	samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	if (vkCreateSampler(device.getLogicalDevice(), &samplerCI, nullptr, &preFilterCubeSampler))
	{
		throw std::runtime_error("failed to create Sampler!");
	}

	// offscreen cube for hdr cube textre 2d
	VkImage offscreenImage;
	VkDeviceMemory offscreenMemory;
	VkImageView offscreenImageView;

	// Pre-filtered cube map
	// Image
	VkImageCreateInfo imageCI{};
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.format = format;
	imageCI.extent.width = dim;
	imageCI.extent.height = dim;
	imageCI.extent.depth = 1;
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 1;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	device.createImageWithInfo(imageCI, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreenImage, offscreenMemory);
	// Image view
	VkImageViewCreateInfo viewCI{};
	viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCI.format = format;
	viewCI.subresourceRange = {};
	viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCI.subresourceRange.levelCount = 1;
	viewCI.subresourceRange.layerCount = 1;
	viewCI.image = offscreenImage;
	if (vkCreateImageView(device.getLogicalDevice(), &viewCI, nullptr, &offscreenImageView))
	{
		throw std::runtime_error("failed to create ImageView!");
	}

	VkAttachmentDescription attDesc = {};
	// Color attachment
	attDesc.format = format;
	attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;

	// Renderpass
	VkRenderPassCreateInfo renderPassCI{};
	renderPassCI.attachmentCount = 1;
	renderPassCI.pAttachments = &attDesc;
	renderPassCI.subpassCount = 1;
	renderPassCI.pSubpasses = &subpassDescription;
	renderPassCI.dependencyCount = 2;
	renderPassCI.pDependencies = tmpdependencies.data();
	renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	VkRenderPass renderpass;
	if (vkCreateRenderPass(device.getLogicalDevice(), &renderPassCI, nullptr, &renderpass))
	{
		throw std::runtime_error("failed to create renderpass!");
	}

	std::vector<VkImageView> attachments = { offscreenImageView };

	VkFramebufferCreateInfo fbufCreateInfo{};
	fbufCreateInfo.renderPass = renderpass;
	fbufCreateInfo.attachmentCount = attachments.size();
	fbufCreateInfo.pAttachments = attachments.data();
	fbufCreateInfo.width = dim;
	fbufCreateInfo.height = dim;
	fbufCreateInfo.layers = 1;
	fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

	VkFramebuffer frameBuffer;
	if (vkCreateFramebuffer(device.getLogicalDevice(), &fbufCreateInfo, nullptr, &frameBuffer))
	{
		throw std::runtime_error("failed to create frameBuffer!");
	}

	VkCommandBuffer cmd = device.beginSingleTimeCommands();
	device.transitionImageLayout(cmd, offscreenImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	device.endSingleTimeCommands(cmd);

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderpass;
	renderPassInfo.framebuffer = frameBuffer;

	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = { dim, dim };

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { 0.01f, 0.1f, 0.1f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };
	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	std::vector<glm::mat4> matrices = {
		// POSITIVE_X
		glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_X
		glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// POSITIVE_Y
		glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_Y
		glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// POSITIVE_Z
		glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
		// NEGATIVE_Z
		glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
	};

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = numMips;
	subresourceRange.layerCount = 6;

	auto cmdBuf = device.beginSingleTimeCommands();

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(dim);
	viewport.height = static_cast<float>(dim);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor{ {0, 0}, {(uint32_t)dim, (uint32_t)dim} };
	vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

	assert(irradiancePipelinelayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

	PipelineConfigInfo pipelineConfig{};
	pipelineConfig.depthStencilInfo.depthTestEnable = true;
	pipelineConfig.depthStencilInfo.depthWriteEnable = true;
	pipelineConfig.attributeDescriptions = jhb::Vertex::getAttrivuteDescriptions();
	pipelineConfig.bindingDescriptions = jhb::Vertex::getBindingDescriptions();

	Pipeline::defaultPipelineConfigInfo(pipelineConfig);
	pipelineConfig.renderPass = renderpass;
	pipelineConfig.pipelineLayout = prefilterPipelinelayout;
	prefilterPipeline = std::make_unique<Pipeline>(
		device,
		"shaders/filtercube.vert.spv",
		"shaders/prefilterenvmap.frag.spv",
		pipelineConfig);

	prefilterPipeline->bind(cmdBuf);
	device.transitionImageLayout(cmdBuf, preFilterCubeImg, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

	for (uint32_t m = 0; m < numMips; m++) {
		pushBlock.roughness = (float)m / (float)(numMips - 1);
		for (uint32_t f = 0; f < 6; f++) {
			viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
			viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
			vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

			// Render scene from cube face's point of view
			vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			// Update shader push constant block
			pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];
			prefilterPipeline->bind(cmdBuf);

			vkCmdBindDescriptorSets(
				cmdBuf,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				prefilterPipelinelayout,
				0, 1
				, &descSets[0],
				0, nullptr
			);

			vkCmdBindDescriptorSets(
				cmdBuf,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				prefilterPipelinelayout,
				1, 1
				, &descSets[1],
				0, nullptr
			);
			vkCmdPushConstants(cmdBuf, prefilterPipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PrefileterPushBlock), &pushBlock);
			cube->model->bind(cmdBuf);
			cube->model->draw(cmdBuf, prefilterPipelinelayout, 0);

			vkCmdEndRenderPass(cmdBuf);

			device.transitionImageLayout(
				cmdBuf, offscreenImage, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			// Copy region for transfer from framebuffer to cube fdce
			VkImageCopy copyRegion = {};

			copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.srcSubresource.baseArrayLayer = 0;
			copyRegion.srcSubresource.mipLevel = 0;
			copyRegion.srcSubresource.layerCount = 1;
			copyRegion.srcOffset = { 0, 0, 0 };

			copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.dstSubresource.baseArrayLayer = f;
			copyRegion.dstSubresource.mipLevel = m;
			copyRegion.dstSubresource.layerCount = 1;
			copyRegion.dstOffset = { 0, 0, 0 };

			copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
			copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
			copyRegion.extent.depth = 1;

			vkCmdCopyImage(
				cmdBuf,
				offscreenImage,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				preFilterCubeImg,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&copyRegion);

			// Transform framebuffer color attachment back

			device.transitionImageLayout(
				cmdBuf, offscreenImage, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			);
		}
	}

	device.transitionImageLayout(
		cmdBuf, preFilterCubeImg,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange
	);
	device.endSingleTimeCommands(cmdBuf);
}
