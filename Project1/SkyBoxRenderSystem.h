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
#include "GameObject.h"
#include "Camera.h"
#include "Descriptors.h"
#include "FrameInfo.h"

#include <stdint.h>

namespace jhb {
	class SkyBoxRenderSystem : public BaseRenderSystem {
	public:
		SkyBoxRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange);
		~SkyBoxRenderSystem();

		SkyBoxRenderSystem(const SkyBoxRenderSystem&) = delete;
		SkyBoxRenderSystem(SkyBoxRenderSystem&&) = delete;
		SkyBoxRenderSystem& operator=(const SkyBoxRenderSystem&) = delete;

		void bindPipeline(VkCommandBuffer cmd)
		{
			pipeline->bind(cmd);
		}

		void renderSkyBox(FrameInfo& frameInfo);
	private:

		// render pass only used to create pipeline
		// render system doest not store render pass, beacuase render system's life cycle is not tie to render pass
		void createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag) override;
	};
}