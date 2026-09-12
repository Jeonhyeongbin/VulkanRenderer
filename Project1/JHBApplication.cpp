#include "JHBApplication.h"
#include "PBRPass.h"
#include "SkyboxPass.h"
#ifndef ANDROID
#include "ImGuiPass.h"
#include "ImGuiContext.h"
#endif
#include "InputController.h"
#include "FrameInfo.h"
#include "Descriptors.h"
#include "PointLightPass.h"
#include "Model.h"
#ifndef ANDROID
#include "External/Imgui/imgui.h"
#endif
#include "BRDFLutPass.h"
#include "IrradiancePass.h"
#include "PrefilterPass.h"
#include "PickingPass.h"
#include "ShadowPass.h"
#include "ComputerShadeSystem.h"
#include "GameObjectManager.h"
#include "Scene.h"

#define _USE_MATH_DEFINESimgui
#include <math.h>
#include <memory>
#include <numeric>
#ifdef ANDROID
#include <thread>
#include <chrono>
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "VulkanRenderer", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer", __VA_ARGS__)
#endif

namespace jhb {
	JHBApplication::JHBApplication()
	{
		uboBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
		CubeBoxDescriptorSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
		globalDescriptorSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);


		//computeShaderSystem = std::make_unique<ComputerShadeSystem>(device);
		GlobalScene = std::make_unique<Scene>(device);

		init();
	}

	JHBApplication::~JHBApplication()
	{
#ifndef ANDROID
		imguiContext.reset();
#endif
	}

	void JHBApplication::Run()
	{
		auto viewerObject = GameObject::createGameObject();
		viewerObject.transform.translation.x =  0.0f;   // 좌우 정중앙
		viewerObject.transform.translation.y = -12.0f;  // 위쪽 (Y음수 = 위)
		viewerObject.transform.translation.z =  0.0f;   // 앞뒤 정중앙
		viewerObject.transform.rotation.x    =  glm::radians(35.0f); // 35도 아래를 향함
#ifndef ANDROID
		InputController cameraController{device.getWindow().GetGLFWwindow(), viewerObject};
#else
		InputController cameraController{viewerObject};
		inputController.store(&cameraController); // Main.cpp handleInputEvent에서 사용 (atomic)
#endif
		double x = 0, y = 0;
		auto currentTime = std::chrono::high_resolution_clock::now();

#ifndef ANDROID
		auto forwardDir = cameraController.move(&window.GetGLFWwindow(), 0, viewerObject);
#else
		auto forwardDir = cameraController.move(0, viewerObject);
#endif
		window.getCamera()->setViewDirection(viewerObject.transform.translation, forwardDir);
		float aspect = renderer.getAspectRatio();
		window.getCamera()->setPerspectiveProjection(aspect, 0.1f, 200.f);


#ifdef ANDROID
		// 30fps 캡용 타이머 초기화
		constexpr long long TARGET_FRAME_US = 33333LL; // 1/30초 = 33.33ms
		auto lastFrameEnd = std::chrono::high_resolution_clock::now();

		while (androidAppRunning.load())
		{
			// 이벤트 처리는 android_main 이벤트 스레드가 전담한다.
			// 이 스레드(앱 스레드)에서 ALooper_pollAll을 호출하면 안 된다.
			// (ALooper는 android_main 스레드가 소유)
			if (androidApp->destroyRequested) break;

			// ── 30fps 프레임 캡 (발열 감소) ──────────────────────────────────
			// 이전 프레임 끝에서 33.3ms가 지나지 않았으면 sleep으로 CPU를 쉬게 한다.
			// FIFO VSync로 GPU는 이미 60fps 제한이 걸리지만 CPU는 계속 스핀하므로
			// 명시적 sleep을 줘야 발열이 줄어든다.
			{
				auto now = std::chrono::high_resolution_clock::now();
				long long elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(now - lastFrameEnd).count();
				if (elapsedUs < TARGET_FRAME_US) {
					std::this_thread::sleep_for(std::chrono::microseconds(TARGET_FRAME_US - elapsedUs));
				}
				lastFrameEnd = std::chrono::high_resolution_clock::now();
			}
			// ────────────────────────────────────────────────────────────────

			// 터치 입력에서 커서 위치 업데이트
			x = cameraController.getTouchX();
			y = cameraController.getTouchY();
#else
		while (!glfwWindowShouldClose(&window.GetGLFWwindow()))
		{
			glfwPollEvents(); //may block
			glfwGetCursorPos(&window.GetGLFWwindow(), &x, &y);
#endif
			auto newTime = std::chrono::high_resolution_clock::now();
			float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
			currentTime = newTime;

			auto commandBuffer = renderer.beginFrame();
			if (commandBuffer == nullptr) // begine frame return null pointer if swap chain need recreated
			{
				renderer.setWindowExtent(window.getExtent());
				window.resetWindowResizedFlag();
				continue;
			}

			int frameIndex = renderer.getFrameIndex();
			// Android에서 스왑체인 이미지는 3개 이상일 수 있다.
			// 스왑체인 기반 프레임버퍼(DeferredPBR) 선택은 실제 획득한 이미지 인덱스를 사용해야 한다.
			// UBO/오프스크린 버퍼처럼 MAX_FRAMES_IN_FLIGHT(2) 기반 리소스는 frameIndex를 그대로 쓴다.
			int imageIndex = renderer.getImageIndex();
			//renderer.excuteComputeDispatch(&(computeShaderSystem->computeCommandBuffers[frameIndex]));

			FrameInfo frameInfo{
				frameIndex,
				frameTime,
				commandBuffer,
				*window.getCamera(),
				globalDescriptorSets[frameIndex],
				CubeBoxDescriptorSets[frameIndex],
			};

			// update part : resources
			GlobalUbo ubo{};
			ubo.projection = window.getCamera()->getProjection();
			ubo.view = window.getCamera()->getView();
			ubo.inverseView = window.getCamera()->getInverseView();
			ubo.exposure = 1.f;
			ubo.gamma = 1.f;
			ubo.pointLights[0].color.r = 40.f;
			ubo.pointLights[0].color.g = 40.f;
			ubo.pointLights[0].color.b = 40.f;
			ubo.pointLights[0].color.a = 30.f;
			
			//computeShaderSystem->UpdateUniform(frameIndex, ubo.view, ubo.projection);
			

			// Scene owns lights; the pass only draws their markers.
            ubo.numLights = 0;
            for (const auto& light : GlobalScene->pointLights())
            {
                if (ubo.numLights == MaxLights)
                {
                    break;
                }
                auto& target = ubo.pointLights[ubo.numLights++];
                target.position = glm::vec4(light.position, 1.0f);
                target.color = glm::vec4(light.color, light.intensity);
            }
			uboBuffers[frameIndex]->writeToBuffer(&ubo); // wrtie to using frame buffer index
			uboBuffers[frameIndex]->flush(); //not using coherent_bit flag, so must to flush memory manually
			// and now we need tell to pipeline object where this buffer is and how data within it's structure
			// so using descriptor

			if (!pickingPhase(commandBuffer, ubo, frameIndex, x, y))
			{
				// camera controll phase
				if (window.objectId ==0 && window.GetMousePressed())
				{
					window.mouseMove(x, y, frameTime, viewerObject);
				}
			}

#ifndef ANDROID
			auto forwardDir = cameraController.move(&window.GetGLFWwindow(), frameTime, viewerObject);
#else
			auto forwardDir = cameraController.move(frameTime, viewerObject);
#endif
			window.getCamera()->setViewDirection(viewerObject.transform.translation, forwardDir);
			float aspect = renderer.getAspectRatio();
			window.getCamera()->setPerspectiveProjection(aspect, 0.1f, 200.f);
			// render part : vkcmd
			// this is why beginFram and beginswapchian renderpass are not combined;
			// because main application control over this multiple render pass like reflections, shadows, post-processing effects


			// TODO(migration): compose ShadowPass -> PBRPass -> SkyboxPass -> PointLightPass -> PresentPass here.
			// Scene/material preparation must be migrated before this path can render.
		
#ifndef ANDROID
			imguiContext->configureTarget(renderer.GetSwapChain().getSwapChainImageFormat(),
                static_cast<uint32_t>(renderer.GetSwapChain().getImageCount()));
            imguiContext->beginFrame();
            ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Appearing);
            ImGui::SetNextWindowSize(ImVec2(300.0f, 300.0f), ImGuiCond_FirstUseEver);
            ImGui::Begin("settings", nullptr, ImGuiWindowFlags_MenuBar);
            ImGui::SliderFloat("roughness", &uiRoughness, 0.1f, 1.0f);
            ImGui::SliderFloat("metalic", &uiMetallic, 0.1f, 1.0f);
            ImGui::End();
            imguiContext->endFrame();
            // TODO(migration): composite onto an initialized, format-matched LDR graph target:
            // imguiPass->addPass(graph, uiColor, imguiContext->drawFunction());
            // PresentPass reads uiColor afterwards; render the graph before endFrame().

#endif
			renderer.endFrame();
		}

		// Run() 루프 종료 후 dangling pointer 방지
		// (cameraController는 이 함수가 반환되면 소멸됨)
#ifdef ANDROID
		inputController.store(nullptr);
#endif
		vkDeviceWaitIdle(device.getLogicalDevice());
	}

	void JHBApplication::init()
	{
		globalPools[0] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT).build();
		// ubo
		globalPools[1] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT).build(); // skybox


		// for gltf model color map and normal map and emissive, occlusion, metallicRoughness Textures
		globalPools[3] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT*35).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT * 10*25).build();



		for (int i = 0; i < uboBuffers.size(); i++)
		{
			uboBuffers[i] = std::make_unique<Buffer>(
				device,
				sizeof(GlobalUbo),
				1,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			);
			uboBuffers[i]->map();
		}

		// because of simultenous
		// for example, while frame0 rendering and frame1 using ubo either,
		// so just copy instance makes you safe from multithread env

		// frame0 
		//   write date -> globalUbo
		//	 "Bind" (globalUbo)
		//	 start Rendering
		// frame01
		//   write date -> globalUbo
		//	 "Bind" (globalUbo)
		//	 start Rendering
		// can do all this without having to wait for frame0 to finish rendering

		// two descriptor sets
		// each descriptor set contain two UNIFORM_BUFFER descriptor
		descSetLayouts.push_back(DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS).
			build());

		// for sky box
		descSetLayouts.push_back(DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).build());

		// for pbr resource
		descSetLayouts.push_back(DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).
			addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).
			addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).build());

		// for gltf normal map and color map and emissive, occlusion, metallicRoughness Textures
		descSetLayouts.push_back(DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build());

		pickingPass = std::make_unique<PickingPass>();
#ifndef ANDROID
		imguiPass = std::make_unique<ImGuiPass>();
        imguiContext = std::make_unique<ImGuiContext>(device,
            renderer.GetSwapChain().getSwapChainImageFormat(),
            static_cast<uint32_t>(renderer.GetSwapChain().getImageCount()));
#endif

		pointLightPass = std::make_unique<PointLightPass>();
        GlobalScene->pointLights().push_back({{0.0f, -10.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, 5.0f, 0.1f});

		skyboxPass = std::make_unique<SkyboxPass>();

		shadowPass = std::make_unique<ShadowPass>();

		// for uniform buffer
		for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
		{
			auto bufferInfo = uboBuffers[i]->descriptorInfo();

			DescriptorWriter(*descSetLayouts[0], *globalPools[0]).writeBuffer(0, &bufferInfo).build(globalDescriptorSets[i]);

		}

		VkDescriptorImageInfo skyBoximageInfo{};
		skyBoximageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		skyBoximageInfo.imageView = GlobalScene->environmentCube()->getTexture(0).view;
		skyBoximageInfo.sampler = GlobalScene->environmentCube()->getTexture(0).sampler;

		for (int i = 0; i < CubeBoxDescriptorSets.size(); i++)
		{
			DescriptorWriter(*descSetLayouts[1], *globalPools[1]).writeImage(0, &skyBoximageInfo).build(CubeBoxDescriptorSets[i]);
		}

		brdfLutPass = std::make_unique<BRDFLutPass>();
        irradiancePass = std::make_unique<IrradiancePass>();
        prefilterPass = std::make_unique<PrefilterPass>();
        // TODO(migration): create IBL resources and declare BRDF -> irradiance -> prefilter
        // in an initialization graph. PBR/lighting consume their outputs through .read().

		// for gltf color map and normal map and emissive, occlusion, metallicRoughness Textures
		// this time, only need damaged helmet materials info
		auto& gltfModels = GameObjectManager::GetSingleton().gameObjects;
		for (auto& gltfModel : gltfModels)
		{
			uint32_t id = gltfModel.first;
			for (auto& material : gltfModel.second.model->materials)
			{
				std::vector<VkDescriptorImageInfo> imageinfos = { gltfModel.second.model->getTexture(material.baseColorTextureIndex).descriptor, gltfModel.second.model->getTexture(material.normalTextureIndex).descriptor
				,gltfModel.second.model->getTexture(material.occlusionTextureIndex).descriptor, gltfModel.second.model->getTexture(material.emissiveTextureIndex).descriptor, gltfModel.second.model->getTexture(material.metallicRoughnessTextureIndex).descriptor
				};
				for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
				{
					DescriptorWriter(*descSetLayouts[3], *globalPools[3]).writeImage(0, &imageinfos[0]).writeImage(1, &imageinfos[1])
						.writeImage(2, &imageinfos[2]).writeImage(3, &imageinfos[3]).writeImage(4, &imageinfos[4])
						.build(material.descriptorSets[i]);
				}
			}
		}

		// Shadow cube descriptors are supplied by graph reads in PBR/lighting.
	}

	bool JHBApplication::pickingPhase(VkCommandBuffer commandBuffer, GlobalUbo& ubo, int frameIndex, int x, int y)
	{
		// TODO(migration): declare PickingPass when requested, then copy its ID pixel
        // after graph rendering and consume readback only after the submission fence.
        // Selection manipulation below only consumes an already resolved window.objectId.

		// picking only apply to pbrobjects
		if (window.objectId > 0)
		{
			auto& pickedObject = GameObjectManager::GetSingleton().gameObjects[window.objectId - 1];
			{
				if (pickedObject.model && pickedObject.getId()>= 2)
				{
					// should transfer rotation axis to object space;
					uint32_t obj_first_id = pickedObject.model->firstid;
					pickedObject.transform.rotation = glm::rotate(glm::mat4{ 1.f }, (float)((px - x) * (0.001)), glm::vec3{ 1, 0, 0 }) * glm::vec4(pickedObject.transform.rotation, 1);
					px = x, py = y;

					std::vector<glm::vec3> tmplist(pickedObject.model->instanceCount);
					for (int i = 0; i < pickedObject.model->instanceCount; i++)
					{
						tmplist[i] = GameObjectManager::GetSingleton().gameObjects[obj_first_id + i].transform.translation;
					}

					std::vector<glm::vec3> tmprot(pickedObject.model->instanceCount);
					for (int i = 0; i < pickedObject.model->instanceCount; i++)
					{
						tmprot[i] = GameObjectManager::GetSingleton().gameObjects[obj_first_id + i].transform.rotation;
					}

					tmprot[pickedObject.getId() - pickedObject.model->firstid] = pickedObject.transform.rotation;
					{
						pickedObject.model->updateInstanceBuffer(pickedObject.model->instanceCount, tmplist, tmprot);
					}
				}
			}
			return true;
		}

		// if not objectpicking state then store currnet mouse coordinate for next pickingphase
		px = x;
		py = y;

		// and return false boolean for processing camera phase
		return false;
	}
}
