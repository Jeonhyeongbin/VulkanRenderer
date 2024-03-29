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
	class SimpleRenderSystem {
	public:
		SimpleRenderSystem(Device& device, VkRenderPass renderPass,const std::vector<std::unique_ptr<jhb::DescriptorSetLayout>>& descSetLayOuts);
		~SimpleRenderSystem();

		SimpleRenderSystem(const SimpleRenderSystem&) = delete;
		SimpleRenderSystem(SimpleRenderSystem&&) = delete;
		SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

		void renderGameObjects(FrameInfo& frameInfo);
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