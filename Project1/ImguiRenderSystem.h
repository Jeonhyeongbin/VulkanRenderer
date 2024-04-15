#pragma once
#define GLM_FORCE_RADIANS // not use degree;
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "BaseRenderSystem.h"
#include "Pipeline.h"
#include "Device.h"
#include "SwapChain.h"
#include "Model.h"
#include "Camera.h"
#include "Descriptors.h"
#include "FrameInfo.h"
#include "External/Imgui/imgui.h"
#include "External/Imgui/imgui_impl_vulkan.h"
#include "External/Imgui/imgui_impl_glfw.h"
#include <stdint.h>

namespace jhb {
	class ImguiRenderSystem : public BaseRenderSystem {
	public:
		// UI params are set via push constants
		struct PushConstBlock {
			glm::vec2 scale;
			glm::vec2 translate;
		} pushConstBlock;
	public:
		ImguiRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange);
		~ImguiRenderSystem();

		ImguiRenderSystem(const ImguiRenderSystem&) = delete;
		ImguiRenderSystem(ImguiRenderSystem&&) = delete;
		ImguiRenderSystem& operator=(const ImguiRenderSystem&) = delete;

		void render(VkCommandBuffer commandBuffer);
		void newFrame();
		void updateBuffer();
	private:
		// render pass only used to create pipeline
		// render system doest not store render pass, beacuase render system's life cycle is not tie to render pass
		void createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag) override;



	public:
		VkSampler sampler;
		VkImageView fontView = VK_NULL_HANDLE;
	private:
		VkDeviceMemory fontMemory = VK_NULL_HANDLE;
		VkImage fontImage = VK_NULL_HANDLE;
		ImGuiStyle vulkanStyle;
		int selectedStyle = 0;

		VkBuffer vertexBuffer;
		VkBuffer indexBuffer;
		VkDeviceMemory vertexMemory;
		VkDeviceMemory indexMemory;
		int32_t vertexCount = 0;
		int32_t indexCount = 0;

		void* indexMapped;
		void* vertexMapped;


		VkRenderPass imguiRenderPass;
		VkCommandPool imguiCommandPool;
		std::vector<VkCommandBuffer> imguiCommandBuffer;
		std::vector<VkFramebuffer> imguiFrameBuffer;

		bool swapChainRebuild = false;
	};
}