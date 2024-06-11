#pragma once
#define GLM_FORCE_RADIANS // not use degree;
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "Window.h"
#include "Device.h"
#include "SwapChain.h"
#include "Model.h"
#include "GameObject.h"
#include "Renderer.h"
#include "Buffer.h"
#include "Descriptors.h"
#include "FrameInfo.h"

#include <stdint.h>
#include <chrono>
#include <array>

namespace jhb {
	class JHBApplication {
	public:
		JHBApplication();
		~JHBApplication();

		JHBApplication(const JHBApplication&) = delete;
		JHBApplication(JHBApplication&&) = delete;
		JHBApplication& operator=(const JHBApplication&) = delete;

		void Run();

	private:
		void loadGameObjects();
		void createCube();
		void create2DModelForBRDFLUT();
		void createFloor(VkRenderPass, VkPipelineLayout);
		void updateInstance();

		void loadGLTFFile(const std::string& filename);

		void initDescriptorSets();

		void createOffscreenFrameBuffer();
		void pickingPhaseInit(const std::vector<VkPushConstantRange>& pushConstantRanges, const std::vector<VkDescriptorSetLayout>& desclayouts);
		bool pickingPhase(VkCommandBuffer commandBuffer, GlobalUbo& ubo, int frameIndex, int x, int y);
		void generateBRDFLUT();
		void generateIrradianceCube(std::vector<VkDescriptorSetLayout> desclayouts, std::vector<VkDescriptorSet> descSets);
		void generatePrefilteredCube(std::vector<VkDescriptorSetLayout> desclayouts, std::vector<VkDescriptorSet> descSets);

	private:
		// init top to bottom
		Window window{ 800, 600, "TriangleApp!" };
		Device device{ window };

		std::vector<VkSubpassDependency> subdependencies = { {VK_SUBPASS_EXTERNAL,0,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, 0
			}, {VK_SUBPASS_EXTERNAL, 0,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
			0
		} };
		Renderer renderer{ window, device, subdependencies, true, VK_FORMAT_R16G16B16A16_SFLOAT, 2 };

		std::array<std::unique_ptr<DescriptorPool>, 6> globalPools{};
		std::vector<std::unique_ptr<Buffer>> uboBuffers{};
		std::vector<std::unique_ptr<Buffer>> uboPickingIndexBuffer{SwapChain::MAX_FRAMES_IN_FLIGHT};
	private:
		std::vector<std::unique_ptr<jhb::DescriptorSetLayout>> descSetLayouts;
		std::vector<VkDescriptorSet> vkDescSets;

		std::vector<VkDescriptorSet> CubeBoxDescriptorSets{}; // skybox
		std::vector<VkDescriptorSet> globalDescriptorSets{}; // global uniform buffer
		std::vector<VkDescriptorSet> pbrResourceDescriptorSets{}; // pbr resource
		std::vector<VkDescriptorSet> pickingObjUboDescriptorSets{}; // global uniform buffer
		VkDescriptorSet shadowMapDescriptorSet;

		GameObject::Map gameObjects;

		VkImage preFilterCubeImg;
		VkImage IrradianceCubeImg;
		VkImageView preFilterCubeImgView;
		VkImageView IrradianceCubeImgView;
		VkDeviceMemory preFilterCubeMemory;
		VkDeviceMemory IrradianceCubeMemory;
		VkSampler preFilterCubeSampler;
		VkSampler IrradianceCubeSampler;
		VkImage lutBrdfImg;
		VkImageView lutBrdfView;
		VkSampler lutBrdfSampler;
		VkDeviceMemory lutBrdfMemory;

		VkImage fontImage;
		VkImageView fontView;
		VkSampler Sampler;
		VkDeviceMemory fontMemory;

		// offscreen with object index info pixels;
		std::vector<VkImage> offscreenImage{SwapChain::MAX_FRAMES_IN_FLIGHT};
		std::vector<VkDeviceMemory> offscreenMemory{SwapChain::MAX_FRAMES_IN_FLIGHT};
		std::vector<VkImageView> offscreenImageView{SwapChain::MAX_FRAMES_IN_FLIGHT};
		std::vector<VkFramebuffer> offscreenFrameBuffer{SwapChain::MAX_FRAMES_IN_FLIGHT};
		VkRenderPass pickingRenderpass;

		std::unique_ptr<class ImguiRenderSystem> imguiRenderSystem;
		std::unique_ptr<class MousePickingRenderSystem> mousePickingRenderSystem;
		std::unique_ptr<class ShadowRenderSystem> shadowMapRenderSystem;

	private:
		double px, py, pz;
	};
}