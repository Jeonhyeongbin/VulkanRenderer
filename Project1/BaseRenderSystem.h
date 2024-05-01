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
#define _USE_MATH_DEFINES
#include <math.h>

namespace jhb {
	struct PBRPushConstantData {
		//glm::mat4 ModelMatrix{ 1.f };
	};

	struct SimplePushConstantData {
		glm::mat4 ModelMatrix{ 1.f };
		glm::mat4 normalMatrix{ 1.f };
	};

	struct gltfPushConstantData {
		glm::mat4 gltfMatrix{1.f};
	};

	struct PrefileterPushBlock {
		glm::mat4 mvp;
		float roughness;
		uint32_t numSamples = 32u;
	};

	struct IrradiencePushBlock {
		glm::mat4 mvp;
		// Sampling deltas
		float deltaPhi = (2.0f * float(M_PI)) / 180.0f;
		float deltaTheta = (0.5f * float(M_PI)) / 64.0f;
	};

	class BaseRenderSystem {
	public:
		BaseRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descSetLayOuts, const std::vector<VkPushConstantRange>& pushConstanRange);
		virtual ~BaseRenderSystem();

		BaseRenderSystem(const BaseRenderSystem&) = delete;
		BaseRenderSystem(BaseRenderSystem&&) = delete;
		BaseRenderSystem& operator=(const BaseRenderSystem&) = delete;

		virtual void renderGameObjects(FrameInfo& frameInfo, Buffer* instanceBuffer = nullptr);
	private:
		void createPipeLineLayout(const std::vector<VkDescriptorSetLayout>& descriptorSetLayOuts, const std::vector<VkPushConstantRange>& pushConstanRange);

		// render pass only used to create pipeline
		// render system doest not store render pass, beacuase render system's life cycle is not tie to render pass
		virtual void createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag) abstract;
	protected:
		// init top to bottom
		Device& device;

		std::unique_ptr<Pipeline> pipeline;
		VkPipelineLayout pipelineLayout;
	};
}