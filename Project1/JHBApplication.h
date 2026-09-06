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

#ifdef ANDROID
#include <android_native_app_glue.h>
#include <atomic>
#endif

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

#ifdef ANDROID
		void setAndroidApp(struct android_app* app)
		{
			androidApp = app;
			// Window는 android_main에서 SetPendingNativeWindow → Window 생성자 경로로
			// 이미 설정됐다. 같은 포인터로 다시 SetNativeWindow를 호출하면
			// framebufferResized = true 가 재설정되어 첫 프레임에서 불필요한
			// 스왑체인 재생성이 일어나므로, 창 포인터가 실제로 변경된 경우에만 호출한다.
			if (window.GetNativeWindow() != app->window)
			{
				window.SetNativeWindow(app->window);
			}
		}

		// 앱 스레드(Run())에 종료를 요청한다.
		// android_main 이벤트 스레드에서 APP_CMD_DESTROY/APP_CMD_TERM_WINDOW 수신 시 호출.
		void stopAndroidApp() { androidAppRunning.store(false); }

		// Main.cpp handleInputEvent에서 터치를 전달하기 위한 포인터
		// 앱 스레드(Run)에서 쓰고 이벤트 스레드(handleInputEvent)에서 읽으므로 atomic 사용
		std::atomic<class InputController*> inputController{nullptr};
#endif

	private:
		void init();
		bool pickingPhase(VkCommandBuffer commandBuffer, GlobalUbo& ubo, int frameIndex, int x, int y);

	private:
		// init top to bottom
		Window window{ 800, 600, "TriangleApp!" };
		Device device{ window };
        std::unique_ptr<class Scene> GlobalScene;

		std::vector<VkSubpassDependency> subdependencies = { {VK_SUBPASS_EXTERNAL,0,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, 0
			}, {VK_SUBPASS_EXTERNAL, 0,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
			0
		} };
		Renderer renderer{ window, device };

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

		VkImage fontImage;
		VkImageView fontView;
		VkSampler Sampler;
		VkDeviceMemory fontMemory;

		std::unique_ptr<class ComputerShadeSystem> computeShaderSystem;
#ifndef ANDROID
		std::unique_ptr<class ImguiRenderSystem> imguiRenderSystem;
#endif
		std::unique_ptr<class MousePickingRenderSystem> mousePickingRenderSystem;
		std::unique_ptr<class ShadowRenderSystem> shadowMapRenderSystem;
		std::unique_ptr<class PBRRendererSystem> pbrRenderSystem;
		std::unique_ptr<class PBRResourceGenerator> pbrSourceGenerator;
		std::unique_ptr<class PointLightSystem> pointLightSystem;
		std::unique_ptr<class SkyBoxRenderSystem> skyboxRenderSystem;



	private:
		double px, py, pz;
		bool isComputeFrustumCulling;

#ifdef ANDROID
		struct android_app* androidApp = nullptr;
		// android_main 이벤트 스레드에서 stopAndroidApp()으로 false를 설정하면
		// Run()이 루프를 종료한다. 두 스레드에서 접근하므로 atomic 사용.
		std::atomic<bool> androidAppRunning{true};
#endif
	};
}
