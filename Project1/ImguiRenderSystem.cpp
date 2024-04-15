#include "ImguiRenderSystem.h"
#include "TriangleApplication.h"
#include "Model.h"
#include "FrameInfo.h"
#include <memory>
#include <array>




namespace jhb {

	ImguiRenderSystem::ImguiRenderSystem(Device& device)  {
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGui_ImplVulkan_CreateFontsTexture();
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

		// todo imgui�� �����ӹ��� �����
		/*VkImageView attachment[1];
		VkFramebufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.renderPass = device.imguiRenderPass;
		info.attachmentCount = 1;
		info.pAttachments = attachment;
		info.width = extent.width;
		info.height = extent.height;
		info.layers = 1;
		for (uint32_t i = 0; i < framebuffers.size(); i++)
		{
			ImGui_ImplVulkanH_Frame* fd = &wd->Frames[i];
			attachment[0] = fd->BackbufferView;
			err = vkCreateFramebuffer(device, &info, allocator, &fd->Framebuffer);
			check_vk_result(err);
		}*/
	}

	ImguiRenderSystem::~ImguiRenderSystem()
	{
	}

	void ImguiRenderSystem::newFrame()
	{
		// Start the Dear ImGui frame
		ImGui::CreateContext();
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		//// Init imGui windows and elements
		//// Debug window
		ImGui::SetWindowPos(ImVec2(20.f, 20.f), ImGuiCond_Appearing);
		ImGui::SetWindowSize(ImVec2(300, 300), ImGuiCond_Always);
		ImGuiIO& io = ImGui::GetIO();

		io.DisplaySize = ImVec2((float)800, (float)600);
		bool checkbox = true;
		//// Example settings window
		bool istool = true;
		ImGui::Begin("settings", &istool, ImGuiWindowFlags_MenuBar);

		ImGui::SliderFloat("roughness", &roughness, 0.1f, 1.0f);
		ImGui::SliderFloat("metalic", &metalic, 0.1f, 1.0f);
		ImGui::End();

		//SRS - ShowDemoWindow() sets its own initial position and size, cannot override here

		////ImGui::Image(descriptorSet, ImVec2(600.f, 800.f));
		// Render to generate draw buffers
		ImGui::Render();
	}
}