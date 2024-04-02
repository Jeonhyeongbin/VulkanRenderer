#pragma once
#define GLM_FORCE_RADIANS // not use degree;
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "Pipeline.h"
#include "Device.h"
#include "SwapChain.h"
#include "Model.h"
#include "GameObject.h"
#include "Camera.h"
#include "Descriptors.h"
#include "FrameInfo.h"

#include <stdint.h>

namespace jhb {
	class SkyBoxRenderSystem {
	public:
		SkyBoxRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<std::unique_ptr<jhb::DescriptorSetLayout>>& descSetLayOuts);
		~SkyBoxRenderSystem();

		SkyBoxRenderSystem(const SkyBoxRenderSystem&) = delete;
		SkyBoxRenderSystem(SkyBoxRenderSystem&&) = delete;
		SkyBoxRenderSystem& operator=(const SkyBoxRenderSystem&) = delete;

		void renderSkyBox(FrameInfo& frameInfo);
		void loadCubemap(const std::string& filename, VkFormat format);
	private:
		void createPipeLineLayout(const std::vector<std::unique_ptr<jhb::DescriptorSetLayout>>& descriptorSetLayOuts);

		// render pass only used to create pipeline
		// render system doest not store render pass, beacuase render system's life cycle is not tie to render pass
		void createPipeline(VkRenderPass renderPass);

		// init top to bottom
		Device& device;

		std::unique_ptr<Pipeline> pipeline;
		VkPipelineLayout pipelineLayout;
	};
}