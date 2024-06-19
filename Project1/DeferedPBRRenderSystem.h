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
	class DeferedPBRRenderSystem : public BaseRenderSystem {
		struct Texture {
			VkImage image;
			VkDeviceMemory memory;
			VkImageView view;
			VkSampler sampler;
			VkFormat format;
		};

		struct UniformData {
			glm::mat4 projection;
			glm::mat4 view;
			glm::mat4 model;
			glm::vec4 lightPos;
		} uniformData;

		struct OffscreenConstant {
			glm::mat4 modelMat;
			glm::mat4 lightView;
		} offscreenBuffer;

	public:
		DeferedPBRRenderSystem(Device& device, std::vector<VkDescriptorSetLayout> descSetlayouts, const std::vector<VkImageView>& swapchainImageViews, VkFormat swapchainFormat);
		~DeferedPBRRenderSystem();

		DeferedPBRRenderSystem(const DeferedPBRRenderSystem&) = delete;
		DeferedPBRRenderSystem(DeferedPBRRenderSystem&&) = delete;
		DeferedPBRRenderSystem& operator=(const DeferedPBRRenderSystem&) = delete;

		virtual void renderGameObjects(FrameInfo& frameInfo) override;
	public:
		VkFramebuffer getFrameBuffer(int idx) { return frameBuffers[idx]; }
		VkRenderPass getRenderPass() { return offScreenRenderPass; }
		void createFrameBuffers(const std::vector<VkImageView>& swapchainImageViews, bool shouldRecreate = false);
	private:
		// render pass only used to create pipeline
		// render system doest not store render pass, beacuase render system's life cycle is not tie to render pass
		virtual void createPipeline(VkRenderPass , const std::string& vert, const std::string& frag) override;
		void createGBuffers();
		void createAttachment(VkFormat format,
			VkImageUsageFlagBits usage,
			Texture* attachment);
		void createRenderPass(VkFormat);

		void createDamagedHelmet();
		void createFloor();
		void createVertexAttributeAndBindingDesc(PipelineConfigInfo&);
		void createLightingPipelineAndPipelinelayout(const std::vector<VkDescriptorSetLayout>&);
		void removeVkResources();

		std::vector<VkDescriptorSetLayout> initializeOffScreenDescriptor();
		std::shared_ptr<Model> loadGLTFFile(const std::string& filename);

	private:
		std::unique_ptr<Pipeline> lightingPipeline = nullptr; // pipeline for second subpass
		VkPipelineLayout lightingPipelinelayout;

	private:
		Texture PositionAttachment;
		Texture AlbedoAttachment;
		Texture NormalAttachment;
		Texture DepthAttachment;
		Texture MaterialAttachment;
		Texture EmmisiveAttachment;

		std::vector<VkFramebuffer> frameBuffers;

		VkRenderPass offScreenRenderPass;
		const VkExtent2D offscreenImageSize{ 1024, 1024 };

		std::unique_ptr<Buffer> uboBuffer;
		VkDescriptorSet gbufferDescriptorSet;

		std::unique_ptr<DescriptorPool> gbufferDescriptorPool;
		std::unique_ptr<DescriptorSetLayout> gbufferDescriptorSetLayout;
		glm::vec3 _lightpos;
	public:
		GameObject::Map pbrObjects;
		static uint32_t id;
	};
}