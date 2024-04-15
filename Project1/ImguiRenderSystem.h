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
	class ImguiRenderSystem {
	public:
		ImguiRenderSystem(Device& device, const SwapChain& swapchain);
		~ImguiRenderSystem();

		ImguiRenderSystem(const ImguiRenderSystem&) = delete;
		ImguiRenderSystem(ImguiRenderSystem&&) = delete;
		ImguiRenderSystem& operator=(const ImguiRenderSystem&) = delete;
		void newFrame();

	public:
		float metalic = 0.1f;
		float roughness= 0.1f;
	private:
		ImGuiStyle vulkanStyle;
	public:
		std::vector<VkFramebuffer> framebuffers{SwapChain::MAX_FRAMES_IN_FLIGHT};
	};
}