#include "ImguiRenderSystem.h"
#include "TriangleApplication.h"
#include "Model.h"
#include "FrameInfo.h"
#include <memory>
#include <array>


namespace jhb {

	ImguiRenderSystem::ImguiRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange) : 
		BaseRenderSystem(device, renderPass, globalSetLayOut, pushConstanRange) {
		createPipeline(renderPass, vert, frag);
		ImGui::CreateContext();

		//SRS - Set ImGui font and style scale factors to handle retina and other HiDPI displays
		ImGuiIO& io = ImGui::GetIO();
		io.FontGlobalScale = 10.f;
		ImGuiStyle& style = ImGui::GetStyle();
		style.ScaleAllSizes(10.f);

		vulkanStyle = ImGui::GetStyle();
		vulkanStyle.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
		vulkanStyle.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
		vulkanStyle.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
		vulkanStyle.Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
		vulkanStyle.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

		ImGui::StyleColorsDark();
		VkExtent2D extent = device.getWindow().getExtent();
		// Dimensions
		io.DisplaySize = ImVec2(extent.width, extent.height);
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);


		int texWidth, texHeight;
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		// Create font texture
		unsigned char* fontData;
		io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
		VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

		device.createBuffer(uploadSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = static_cast<uint32_t>(texWidth);
		imageInfo.extent.height = static_cast<uint32_t>(texHeight);
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = 0; // Optional
		device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, fontImage, fontMemory);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;

		VkCommandBuffer commandBuffer = device.beginSingleTimeCommands();
		device.transitionImageLayout(commandBuffer, fontImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
		device.endSingleTimeCommands(commandBuffer);

		device.copyBufferToImage(stagingBuffer, fontImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1);

		commandBuffer = device.beginSingleTimeCommands();
		device.transitionImageLayout(commandBuffer, fontImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);
		device.endSingleTimeCommands(commandBuffer);

		// create image view and image sampler
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = fontImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device.getLogicalDevice(), &viewInfo, nullptr, &fontView) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture image view!");
		}

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;

		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(device.getPhysicalDevice(), &properties);
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 0;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 1;

		if (vkCreateSampler(device.getLogicalDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture sampler!");
		}
	}

	ImguiRenderSystem::~ImguiRenderSystem()
	{
		vkDestroyPipelineLayout(device.getLogicalDevice(), pipelineLayout, nullptr);
	}

	void ImguiRenderSystem::createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag)
	{
		assert(pipelineLayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

		PipelineConfigInfo pipelineConfig{};
		pipelineConfig.depthStencilInfo.depthTestEnable = false;
		pipelineConfig.depthStencilInfo.depthWriteEnable = false;

		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.colorBlendAttachment.blendEnable = VK_TRUE;
		pipelineConfig.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		pipelineConfig.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		pipelineConfig.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		pipelineConfig.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

		VkVertexInputBindingDescription bindingdesc{};

		bindingdesc.binding = 0;
		bindingdesc.stride = sizeof(ImDrawVert);
		bindingdesc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

		pipelineConfig.bindingDescriptions.clear();
		pipelineConfig.bindingDescriptions.push_back(bindingdesc);

		std::vector<VkVertexInputAttributeDescription> attrdesc(3);
		attrdesc[0].binding = 0;
		attrdesc[0].location = 0;
		attrdesc[0].format = VK_FORMAT_R32G32_SFLOAT;
		attrdesc[0].offset = offsetof(ImDrawVert, ImDrawVert::pos);
		attrdesc[1].binding = 0;
		attrdesc[1].location = 1;
		attrdesc[1].format = VK_FORMAT_R32G32_SFLOAT;
		attrdesc[1].offset = offsetof(ImDrawVert, ImDrawVert::uv);
		attrdesc[2].binding = 0;
		attrdesc[2].location = 2;
		attrdesc[2].format = VK_FORMAT_R8G8B8A8_UNORM;
		attrdesc[2].offset = offsetof(ImDrawVert, ImDrawVert::col);


		pipelineConfig.attributeDescriptions = attrdesc;
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<Pipeline>(
			device,
			vert,
			frag,
			pipelineConfig);
	}

	
	void ImguiRenderSystem::render(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet)
	{
		ImGuiIO& io = ImGui::GetIO();
		pipeline->bind(commandBuffer);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
		PushConstBlock pushConstBlock;
		// UI scale and translate via push constants
		pushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
		pushConstBlock.translate = glm::vec2(-1.0f);
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &pushConstBlock);

		// Render commands
		ImDrawData* imDrawData = ImGui::GetDrawData();
		int32_t vertexOffset = 0;
		int32_t indexOffset = 0;

		if (imDrawData->CmdListsCount > 0) {
			VkBuffer buffer[] = { vertexBuffer };
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffer, offsets);
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

			for (int32_t i = 0; i < imDrawData->CmdListsCount; i++)
			{
				const ImDrawList* cmd_list = imDrawData->CmdLists[i];
				for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
				{
					const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
					vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
					indexOffset += pcmd->ElemCount;
				}
				vertexOffset += cmd_list->VtxBuffer.Size;
			}
		}
		ImGui::EndFrame();
	}
	void ImguiRenderSystem::newFrame()
	{
		ImGui::EndFrame();
		ImGui::NewFrame();

		// Init imGui windows and elements

		// Debug window
		ImGui::SetWindowPos(ImVec2(0, 0.f),1);
		ImGui::SetWindowSize(ImVec2(600, 800), 1);
		ImGui::TextUnformatted("aa");
		ImGui::TextUnformatted("bb");

		bool checkbox = true;
		float lightSpeed = 0.f;
		// Example settings window
		ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), 0);
		ImGui::SetNextWindowSize(ImVec2(600.f, 800.f), 0);
		ImGui::Begin("Example settings");
		ImGui::Checkbox("Display background", &checkbox);
		ImGui::Checkbox("Animate light", &checkbox);
		ImGui::SliderFloat("Light speed", &lightSpeed, 0.1f, 1.0f);
		//ImGui::ShowStyleSelector("UI style");


		ImGui::End();

		//SRS - ShowDemoWindow() sets its own initial position and size, cannot override here
		ImGui::ShowDemoWindow();

		// Render to generate draw buffers
		ImGui::Render();
	}

	void ImguiRenderSystem::updateBuffer()
	{
		ImDrawData* imDrawData = ImGui::GetDrawData();

		// Note: Alignment is done inside buffer creation
		VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
		VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

		if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
			return;
		}
		
		// Update buffers only if vertex or index count has been changed compared to current buffer size
		
		// Vertex buffer
		if ((vertexBuffer == VK_NULL_HANDLE) || (vertexCount != imDrawData->TotalVtxCount)) {
			if (vertexMemory)
			{
				vkUnmapMemory(device.getLogicalDevice(), vertexMemory);
			}
			//if (vertexBuffer)
			//{
			//	vkDestroyBuffer(device.getLogicalDevice(), vertexBuffer ,nullptr);
			//}
			//if (vertexMemory)
			//{
			//	vkFreeMemory(device.getLogicalDevice(), vertexMemory, nullptr);
			//}
			vertexBufferSize = (vertexBufferSize + 64 - 1) & ~(64 - 1);
			device.createBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, vertexBuffer, vertexMemory);
			vkMapMemory(device.getLogicalDevice(), vertexMemory, 0, vertexBufferSize, 0, &vertexMapped);
			vertexCount = imDrawData->TotalVtxCount;
		}

		// Index buffer
		if ((indexMemory == VK_NULL_HANDLE) || (indexCount < imDrawData->TotalIdxCount)) {
			if (indexMemory)
			{
				vkUnmapMemory(device.getLogicalDevice(), indexMemory);
			}
			//if (indexBuffer)
			//{
			//	vkDestroyBuffer(device.getLogicalDevice(), indexBuffer, nullptr);
			//}
			//if (indexMemory)
			//{
			//	vkFreeMemory(device.getLogicalDevice(), indexMemory, nullptr);
			//}
			indexBufferSize = (indexBufferSize + 64 - 1) & ~(64 - 1);
			device.createBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, indexBuffer, indexMemory);
			vkMapMemory(device.getLogicalDevice(), indexMemory, 0, indexBufferSize, 0, &indexMapped);
			indexCount = imDrawData->TotalIdxCount;
		}

		ImDrawVert* vtxDst = (ImDrawVert*)vertexMapped;
		ImDrawIdx* idxDst = (ImDrawIdx*)indexMapped;
		for (int n = 0; n < imDrawData->CmdListsCount; n++) {
			const ImDrawList* cmd_list = imDrawData->CmdLists[n];
			memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			vtxDst += cmd_list->VtxBuffer.Size;
			idxDst += cmd_list->IdxBuffer.Size;
		}

		VkMappedMemoryRange mappedRange = {};
		mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		mappedRange.memory = vertexMemory;
		mappedRange.offset = 0;
		mappedRange.size = VK_WHOLE_SIZE;
		vkFlushMappedMemoryRanges(device.getLogicalDevice(), 1, &mappedRange);
		VkMappedMemoryRange mappedRangea = {};
		mappedRangea.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		mappedRangea.memory = indexMemory;
		mappedRangea.offset = 0;
		mappedRangea.size = VK_WHOLE_SIZE;
		vkFlushMappedMemoryRanges(device.getLogicalDevice(), 1, &mappedRangea);
		
	}
}