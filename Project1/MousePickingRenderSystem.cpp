#include "MousePickingRenderSystem.h"
#include "JHBApplication.h"
#include "Model.h"
#include "Pipeline.h"
#include "GameObjectManager.h"

jhb::MousePickingRenderSystem::MousePickingRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag)
	: BaseRenderSystem(device, globalSetLayOut, { VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(gltfPushConstantData)} })
{
	createPipeline(renderPass,vert, frag);
}

jhb::MousePickingRenderSystem::MousePickingRenderSystem(Device& device, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag)
	: BaseRenderSystem(device)
{
	createRenderPass();
	createOffscreenFrameBuffers();

	std::vector<VkPushConstantRange> pushConstantRanges;
	VkPushConstantRange pushVertexConstantRange{};
	pushVertexConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushVertexConstantRange.offset = 0;
	pushConstantRanges.push_back(pushVertexConstantRange);
	pushConstantRanges[0].size = sizeof(gltfPushConstantData);
	VkPushConstantRange pushFragConstantRange{};
	pushFragConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pushFragConstantRange.offset = sizeof(glm::mat4);
	pushConstantRanges.push_back(pushFragConstantRange);
	pushConstantRanges[1].size = sizeof(uint32_t);

	BaseRenderSystem::createPipeLineLayout(globalSetLayOut, pushConstantRanges);
	createPipeline(pickingRenderpass, vert, frag);
}

jhb::MousePickingRenderSystem::~MousePickingRenderSystem()
{

}

void jhb::MousePickingRenderSystem::renderMousePickedObjToOffscreen(VkCommandBuffer cmd, const std::vector<VkDescriptorSet>& descriptorsets,
	int frameIndex, Buffer* pickingUbo)
{
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipelineLayout,
		0, 1
		, &descriptorsets[0],
		0, nullptr
	);

	// update object id per object
	for (auto& kv : GameObjectManager::GetSingleton().gameObjects)
	{
		auto& obj = kv.second;

		if (obj.model == nullptr)
		{
			continue;
		}

		if (kv.first == 2)
		{
			obj.model->bind(cmd);
			if (pickingUbo)
			{
				uint32_t objId = obj.getId()+1;
				vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(uint32_t), &objId);
				obj.model->drawInPickPhase(cmd, pipelineLayout, pipeline->getPipeline(), frameIndex);
			}
			else {
				obj.model->draw(cmd, pipelineLayout, frameIndex);
			}
		}
	}
}

void jhb::MousePickingRenderSystem::createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag)
{
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
		vert,
		frag,
		pipelineConfig);
}

void jhb::MousePickingRenderSystem::createRenderPass()
{
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

	VkAttachmentDescription attDesc = {};
	// Color attachment
	attDesc.format = VK_FORMAT_R32G32B32A32_UINT;
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

	if (vkCreateRenderPass(device.getLogicalDevice(), &renderPassCI, nullptr, &pickingRenderpass))
	{
		throw std::runtime_error("failed to create renderpass!");
	}
}

void jhb::MousePickingRenderSystem::createOffscreenFrameBuffers()
{
	for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		// Image
		VkImageCreateInfo imageCI{};
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.format = VK_FORMAT_R32G32B32A32_UINT;
		imageCI.extent.width = device.getWindow().getExtent().width;
		imageCI.extent.height = device.getWindow().getExtent().height;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		device.createImageWithInfo(imageCI, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreenImage[i], offscreenMemory[i]);
		// Image view
		VkImageViewCreateInfo viewCI{};
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCI.format = VK_FORMAT_R32G32B32A32_UINT;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = 1;
		viewCI.subresourceRange.layerCount = 1;
		viewCI.image = offscreenImage[i];
		if (vkCreateImageView(device.getLogicalDevice(), &viewCI, nullptr, &offscreenImageView[i]))
		{
			throw std::runtime_error("failed to create ImageView!");
		}

		std::vector<VkImageView> attachments = { offscreenImageView[i] };

		VkFramebufferCreateInfo fbufCreateInfo{};
		fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbufCreateInfo.renderPass = pickingRenderpass;
		fbufCreateInfo.attachmentCount = attachments.size();
		fbufCreateInfo.pAttachments = attachments.data();
		fbufCreateInfo.width = device.getWindow().getExtent().width;
		fbufCreateInfo.height = device.getWindow().getExtent().height;
		fbufCreateInfo.layers = 1;

		if (vkCreateFramebuffer(device.getLogicalDevice(), &fbufCreateInfo, nullptr, &offscreenFrameBuffer[i]))
		{
			throw std::runtime_error("failed to create frameBuffer!");
		}
	}
}

void jhb::MousePickingRenderSystem::destroyOffscreenFrameBuffer()
{
	for (auto& frameBuffer : offscreenFrameBuffer)
	{
		vkDestroyFramebuffer(device.getLogicalDevice(), frameBuffer, nullptr);
	}
}
