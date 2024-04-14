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
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		//SRS - Set ImGui font and style scale factors to handle retina and other HiDPI displays
		ImGuiIO& io = ImGui::GetIO();
		io.FontGlobalScale = 1.f;
		ImGui::StyleColorsClassic();
		//style.ScaleAllSizes(1.f);

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
	}

	ImguiRenderSystem::~ImguiRenderSystem()
	{
	}

	void ImguiRenderSystem::createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag)
	{
		assert(pipelineLayout != nullptr && "Cannot Create pipeline before pipeline layout!!");

		PipelineConfigInfo pipelineConfig{};
		pipelineConfig.depthStencilInfo.depthTestEnable = false;
		pipelineConfig.depthStencilInfo.depthWriteEnable = false;

		Pipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.colorBlendAttachment.blendEnable = VK_TRUE;
		pipelineConfig.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		pipelineConfig.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		pipelineConfig.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		pipelineConfig.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		pipelineConfig.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		pipelineConfig.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		pipelineConfig.colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo blend_info = {};
		blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blend_info.attachmentCount = 1;
		blend_info.pAttachments = &pipelineConfig.colorBlendAttachment;

		pipelineConfig.colorBlendInfo = blend_info;
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

		pipelineConfig.rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
		pipeline->bind(commandBuffer);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(ImGui::GetIO().DisplaySize.x);
		viewport.height = static_cast<float>(ImGui::GetIO().DisplaySize.y);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);


		PushConstBlock pushConstBlock;
		// UI scale and translate via push constants
		pushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
		pushConstBlock.translate = glm::vec2(-1.0f - io.DisplaySize.x * (2.0f / io.DisplaySize.x), -1.f - io.DisplaySize.y * (2.0f / io.DisplaySize.y));
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
					VkRect2D scissorRect;
					scissorRect.offset.x = max((int32_t)(pcmd->ClipRect.x), 0);
					scissorRect.offset.y = max((int32_t)(pcmd->ClipRect.y), 0);
					scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
					scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
					vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);
					vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
					indexOffset += pcmd->ElemCount;
				}
				vertexOffset += cmd_list->VtxBuffer.Size;
			}
		}
	}

	void ImguiRenderSystem::newFrame(VkDescriptorSet descriptorSet)
	{
		ImGui::NewFrame();
		ImGui::Image(descriptorSet,ImVec2(30.f,30.f));
		//// Init imGui windows and elements
		//// Debug window
		ImGui::SetWindowPos(ImVec2(20.f, 20.f), ImGuiCond_Appearing);
		ImGui::SetWindowSize(ImVec2(300, 300), ImGuiCond_Always);
		ImGui::TextUnformatted("aa");
		ImGui::TextUnformatted("bb");
		ImGuiIO& io = ImGui::GetIO();

		io.DisplaySize = ImVec2((float)800, (float)600);
		bool checkbox = true;
		float lightSpeed = 0.f;
		//// Example settings window
		ImGui::SetNextWindowPos(ImVec2(-10.f, -10.f), ImGuiCond_Appearing);
		ImGui::SetNextWindowSize(ImVec2(10.f, 10.f), ImGuiCond_FirstUseEver);
		bool istool = true;
		ImGui::Begin("Example settings", &istool, ImGuiWindowFlags_MenuBar);

		//if (ImGui::BeginMenuBar())
		//{
		//	if (ImGui::BeginMenu("File"))
		//	{
		//		if (ImGui::MenuItem("Open..", "Ctrl+O")) { /* Do stuff */ }
		//		if (ImGui::MenuItem("Save", "Ctrl+S")) { /* Do stuff */ }
		//		ImGui::EndMenu();
		//	}
		//	ImGui::EndMenuBar();
		//}

		ImGui::Checkbox("Display background", &checkbox);
		ImGui::Checkbox("Animate light", &checkbox);
		ImGui::SliderFloat("Light speed", &lightSpeed, 0.1f, 1.0f);
		ImGui::ShowStyleSelector("UI style");
		ImGui::Image(descriptorSet, ImVec2(40.f, 40.f));
		ImGui::Text("Camera");
		ImGui::End();

		//SRS - ShowDemoWindow() sets its own initial position and size, cannot override here
		ImGui::ShowDemoWindow();
		////ImGui::Image(descriptorSet, ImVec2(600.f, 800.f));
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
			if (vertexBuffer)
			{
				vkDestroyBuffer(device.getLogicalDevice(), vertexBuffer ,nullptr);
			}
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
			if (indexBuffer)
			{
				vkDestroyBuffer(device.getLogicalDevice(), indexBuffer, nullptr);
			}
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