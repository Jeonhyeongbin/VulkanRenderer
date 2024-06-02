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
#include "FrameInfo.h"

#include <stdint.h>

namespace jhb {
	class ShadowRenderSystem : public BaseRenderSystem {
		struct Texture {
			VkImage image;
			VkDeviceMemory memory;
			VkImageView view;
		};

	public:
		ShadowRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange);
		~ShadowRenderSystem();

		ShadowRenderSystem(const ShadowRenderSystem&) = delete;
		ShadowRenderSystem(ShadowRenderSystem&&) = delete;
		ShadowRenderSystem& operator=(const ShadowRenderSystem&) = delete;

		void update(FrameInfo& frameInfo, GlobalUbo& ubo);
		virtual void renderGameObjects(FrameInfo& frameInfo, Buffer* instanceBuffer = nullptr) override;
	private:
		// render pass only used to create pipeline
		// render system doest not store render pass, beacuase render system's life cycle is not tie to render pass
		virtual void createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag) override;
		void createOffscreenFrameBuffer();
		void createOffscreenRenderPass();
		void createShadowCubeMap();

	private:
		Texture shadowMap;
		Texture offScreen;
		std::array<VkImageView, 6> shadowmapCubeFaces;
		VkRenderPass offScreenRenderPass;
		VkRenderPass renderPass;
	};
}