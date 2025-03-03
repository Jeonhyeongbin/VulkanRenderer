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
	struct PointLightPushConstants {
		glm::vec4 position{};
		glm::vec4 color{};
		float radius;
	};

	class PointLightSystem : public BaseRenderSystem {
	public:
		PointLightSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag);
		~PointLightSystem();

		PointLightSystem(const PointLightSystem&) = delete;
		PointLightSystem(PointLightSystem&&) = delete;
		PointLightSystem& operator=(const PointLightSystem&) = delete;

		void update(FrameInfo& frameInfo, GlobalUbo& ubo);
		virtual void renderGameObjects(FrameInfo& frameInfo) override;
		GameObject::Map& getLightobjects() { return lightObjects; }
	private:
		void createLights();
		// render pass only used to create pipeline
		// render system doest not store render pass, beacuase render system's life cycle is not tie to render pass
		virtual void createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag) override;

	private:
		GameObject::Map lightObjects;
		static uint32_t id;
	};
}