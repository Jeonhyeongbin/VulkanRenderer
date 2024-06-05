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
	class PBRRendererSystem : public BaseRenderSystem {
	public:
		PBRRendererSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange);
		PBRRendererSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange
			, std::vector<Material>& materials);
		~PBRRendererSystem();

		PBRRendererSystem(const PBRRendererSystem&) = delete;
		PBRRendererSystem(PBRRendererSystem&&) = delete;
		PBRRendererSystem& operator=(const PBRRendererSystem&) = delete;

		void renderGameObjects(FrameInfo& frameInfo,Buffer* instanceBuffer = nullptr) override;

		VkPipelineLayout getPipelineLayout() { return pipelineLayout; }
	private:
		// render pass only used to create pipeline
		// render system doest not store render pass, beacuase render system's life cycle is not tie to render pass
		void createPipelinePerMaterial(VkRenderPass renderPass, const std::string& vert, const std::string& frag, std::vector<Material>& materials);
		void createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag) override;
	};
}