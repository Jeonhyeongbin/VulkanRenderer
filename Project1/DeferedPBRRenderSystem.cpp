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

	VkRenderPass DeferedPBRRenderSystem::createOffscreenRenderPass()
	{
		const VkFormat offscreenImageFormat{ VK_FORMAT_R32_SFLOAT };

		VkAttachmentDescription osAttachments[2] = {};

		VkFormat validDepthFormat = device.findSupportedFormat({ VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D16_UNORM
			}, VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		osAttachments[0].format = offscreenImageFormat;
		osAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		osAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		osAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		osAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		osAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		osAttachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		osAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// Depth attachment
		osAttachments[1].format = validDepthFormat;
		osAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		osAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		osAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		osAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		osAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		osAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		osAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorReference = {};
		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 1;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorReference;
		subpass.pDepthStencilAttachment = &depthReference;

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		//cause of this subpass dependencies, don't need to synchronization between offscreen renderpass and pbr render pass
		// or dont need to create multiple shadowmap framebuffer!

		VkRenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.attachmentCount = 2;
		renderPassCreateInfo.pAttachments = osAttachments;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpass;

		if (vkCreateRenderPass(device.getLogicalDevice(), &renderPassCreateInfo, nullptr, &offScreenRenderPass))
		{
			throw std::runtime_error("failed to create offscreen RenderPass!");
		}

		return offScreenRenderPass;
	}

	std::vector<VkDescriptorSetLayout> DeferedPBRRenderSystem::initializeOffScreenDescriptor()
	{
		descriptorPool = DescriptorPool::Builder(device).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1).build();

		uboBuffer = std::make_unique<Buffer>(device, sizeof(UniformData), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		uboBuffer->map();

		descriptorSetLayout = DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS).
			build();

		auto ubo = uboBuffer->descriptorInfo();
		DescriptorWriter(*descriptorSetLayout, *descriptorPool).writeBuffer(0, &ubo).build(descriptorSet);
		return { descriptorSetLayout->getDescriptorSetLayout() };
	}

	void DeferedPBRRenderSystem::renderGameObjects(FrameInfo& frameInfo)
	{

	}
};