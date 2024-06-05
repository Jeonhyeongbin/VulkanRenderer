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
			VkSampler sampler;
		};

		struct UniformData {
			glm::mat4 projection;
			glm::mat4 view;
			glm::mat4 model;
			glm::vec4 lightPos;
		} uniformData;

	public:
		ShadowRenderSystem(Device& device, const std::string& vert, const std::string& frag);
		~ShadowRenderSystem();

		ShadowRenderSystem(const ShadowRenderSystem&) = delete;
		ShadowRenderSystem(ShadowRenderSystem&&) = delete;
		ShadowRenderSystem& operator=(const ShadowRenderSystem&) = delete;

		virtual void renderGameObjects(FrameInfo& frameInfo, Buffer* instanceBuffer = nullptr) override;

		void updateShadowMap(VkCommandBuffer cmd, GameObject& gameobj, VkDescriptorSet discriptorSet);
		void updateUniformBuffer(glm::vec3 pos);

	private:
		// render pass only used to create pipeline
		// render system doest not store render pass, beacuase render system's life cycle is not tie to render pass
		virtual void createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag) override;
		void createOffscreenFrameBuffer();
		VkRenderPass createOffscreenRenderPass();
		void createShadowCubeMap();
		std::vector<VkDescriptorSetLayout> initializeOffScreenDescriptor();

		Texture& GetShadowMap() { return shadowMap; }

	private:
		Texture shadowMap;
		Texture offScreen;

		std::array<VkImageView, 6> shadowmapCubeFaces;
		std::vector<VkFramebuffer> FramebuffersPerCubeFaces{6};

		VkRenderPass offScreenRenderPass;
		const VkExtent2D offscreenImageSize{ 1024, 1024 };

		std::unique_ptr<Buffer> uboBuffer;
		VkDescriptorSet descriptorSet;

		std::unique_ptr<DescriptorPool> descriptorPool;
		std::unique_ptr<DescriptorSetLayout> descriptorSetLayout;
	};
}