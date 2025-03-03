#include "JHBApplication.h"
#include "PBRRenderSystem.h"
#include "SkyBoxRenderSystem.h"
#include "ImguiRenderSystem.h"
#include "InputController.h"
#include "FrameInfo.h"
#include "Descriptors.h"
#include "PointLightSystem.h"
#include "Model.h"
#include "External/Imgui/imgui.h"
#include "PBRResourceGenerator.h"
#include "MousePickingRenderSystem.h"
#include "ShadowRenderSystem.h"
#include "DeferedPBRRenderSystem.h"
#include "ComputerShadeSystem.h"
#include "GameObjectManager.h"
#include "Scene.h"

#define _USE_MATH_DEFINESimgui
#include <math.h>
#include <memory>
#include <numeric>

namespace jhb {
	JHBApplication::JHBApplication()
	{
		uboBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
		CubeBoxDescriptorSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
		globalDescriptorSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
		pbrResourceDescriptorSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
		pickingObjUboDescriptorSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

		//computeShaderSystem = std::make_unique<ComputerShadeSystem>(device);
		GlobalScene = new jhb::Scene();

		init();
	}

	JHBApplication::~JHBApplication()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void JHBApplication::Run()
	{	
		auto viewerObject = GameObject::createGameObject();
		viewerObject.transform.translation.z = -2.5f;
		InputController cameraController{device.getWindow().GetGLFWwindow(), viewerObject};
		double x, y;
		auto currentTime = std::chrono::high_resolution_clock::now();

		auto forwardDir = cameraController.move(&window.GetGLFWwindow(), 0, viewerObject);
		window.getCamera()->setViewDirection(viewerObject.transform.translation, forwardDir);
		float aspect = renderer.getAspectRatio();
		window.getCamera()->setPerspectiveProjection(aspect, 0.1f, 200.f);
		
		//computeShaderSystem->SetupDescriptor(deferedPbrRenderSystem->pbrObjects);

		while (!glfwWindowShouldClose(&window.GetGLFWwindow()))
		{
			glfwPollEvents(); //may block
			glfwGetCursorPos(&window.GetGLFWwindow(), &x, &y);
			auto newTime = std::chrono::high_resolution_clock::now();
			float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
			currentTime = newTime;

			auto commandBuffer = renderer.beginFrame();
			if (commandBuffer == nullptr) // begine frame return null pointer if swap chain need recreated
			{
				mousePickingRenderSystem->destroyOffscreenFrameBuffer();
				mousePickingRenderSystem->createOffscreenFrameBuffers();

				renderer.setWindowExtent(window.getExtent());
				imguiRenderSystem->recreateFrameBuffer(device, renderer.GetSwapChain(), window.getExtent());
				deferedPbrRenderSystem->createFrameBuffers(renderer.getSwapChainImageViews(), true);
				window.resetWindowResizedFlag();
				continue;
			}

			int frameIndex = renderer.getFrameIndex();
			//renderer.excuteComputeDispatch(&(computeShaderSystem->computeCommandBuffers[frameIndex]));

			FrameInfo frameInfo{
				frameIndex,
				frameTime,
				commandBuffer,
				*window.getCamera(),
				globalDescriptorSets[frameIndex],
				pbrResourceDescriptorSets[frameIndex],
				CubeBoxDescriptorSets[frameIndex],
				shadowMapDescriptorSet,
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
			

			pointLightSystem->update(frameInfo, ubo);
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

			auto forwardDir = cameraController.move(&window.GetGLFWwindow(), frameTime, viewerObject);
			window.getCamera()->setViewDirection(viewerObject.transform.translation, forwardDir);
			float aspect = renderer.getAspectRatio();
			window.getCamera()->setPerspectiveProjection(aspect, 0.1f, 200.f);
			// render part : vkcmd
			// this is why beginFram and beginswapchian renderpass are not combined;
			// because main application control over this multiple render pass like reflections, shadows, post-processing effects

			shadowMapRenderSystem->updateShadowMap(commandBuffer, GameObjectManager::GetSingleton().gameObjects, frameIndex);

			renderer.beginSwapChainRenderPass(commandBuffer, deferedPbrRenderSystem->getRenderPass(), deferedPbrRenderSystem->getFrameBuffer(frameIndex), window.getExtent(), 8);
			/*
			pbrRenderSystem->renderGameObjects(frameInfo);
			pointLightSystem->renderGameObjects(frameInfo);
			*/
			deferedPbrRenderSystem->renderGameObjects(frameInfo);
			renderer.endSwapChainRenderPass(commandBuffer);
		
			renderer.beginSwapChainRenderPass(commandBuffer, device.imguiRenderPass, imguiRenderSystem->framebuffers[frameIndex], window.getExtent());
			imguiRenderSystem->newFrame();
			ImDrawData* draw_data = ImGui::GetDrawData();
			ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);
			renderer.endSwapChainRenderPass(commandBuffer);
			renderer.endFrame();
		}

		vkDeviceWaitIdle(device.getLogicalDevice());
	}

	void JHBApplication::init()
	{
		globalPools[0] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT).build();
		// ubo
		globalPools[1] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT).build(); // skybox

		globalPools[2] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT * 3).build();	// prefiter, brud, irradiane

		// for gltf model color map and normal map and emissive, occlusion, metallicRoughness Textures
		globalPools[3] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT*35).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT * 10*25).build();

		// for picking object index storage
		globalPools[4] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT).build();

		globalPools[5] = DescriptorPool::Builder(device).setMaxSets(1).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1).build();

		for (int i = 0; i < uboBuffers.size(); i++)
		{
			uboBuffers[i] = std::make_unique<Buffer>(
				device,
				sizeof(GlobalUbo),
				1,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			);

			uboPickingIndexBuffer[i] = std::make_unique<Buffer>(
				device,
				sizeof(PickingUbo),
				1,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			);

			uboBuffers[i]->map();
			uboPickingIndexBuffer[i]->map();
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

		// for mouse picking object index ubo buffers;
		descSetLayouts.push_back(DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS).
			build());

		// for using shadow map
		descSetLayouts.push_back(DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS).
			build());

		mousePickingRenderSystem = std::make_unique<MousePickingRenderSystem>(device, std::vector{ descSetLayouts[0]->getDescriptorSetLayout(), descSetLayouts[4]->getDescriptorSetLayout() }, "shaders/pbr.vert.spv", "shaders/picking.frag.spv");
		imguiRenderSystem = std::make_unique<ImguiRenderSystem>(device, renderer.GetSwapChain());

		deferedPbrRenderSystem = std::make_unique<DeferedPBRRenderSystem>(device, std::vector{ descSetLayouts[0]->getDescriptorSetLayout(), descSetLayouts[3]->getDescriptorSetLayout(), descSetLayouts[2]->getDescriptorSetLayout()
		, descSetLayouts[5]->getDescriptorSetLayout(), descSetLayouts[1]->getDescriptorSetLayout()}, renderer.getSwapChainImageViews(), renderer.GetSwapChain().getSwapChainImageFormat());
		pointLightSystem = std::make_unique<PointLightSystem>(device, renderer.getSwapChainRenderPass(), std::vector { descSetLayouts[0]->getDescriptorSetLayout()}, "shaders/point_light.vert.spv",
			"shaders/point_light.frag.spv");

		skyboxRenderSystem = std::make_unique<SkyBoxRenderSystem>(device, renderer.getSwapChainRenderPass(), std::vector { descSetLayouts[0]->getDescriptorSetLayout(), descSetLayouts[1]->getDescriptorSetLayout() }, "shaders/skybox.vert.spv",
			"shaders/skybox.frag.spv");

		shadowMapRenderSystem = std::make_unique<ShadowRenderSystem>(device, "shaders/shadowOffscreen.vert.spv", "shaders/shadowOffscreen.frag.spv");
		shadowMapRenderSystem->updateUniformBuffer(pointLightSystem->getLightobjects()[0].transform.translation); // put the light objects poistion

		// for uniform buffer
		for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
		{
			auto bufferInfo = uboBuffers[i]->descriptorInfo();
			auto pickingBufferInfo = uboPickingIndexBuffer[i]->descriptorInfo();
			DescriptorWriter(*descSetLayouts[0], *globalPools[0]).writeBuffer(0, &bufferInfo).build(globalDescriptorSets[i]);
			DescriptorWriter(*descSetLayouts[4], *globalPools[4]).writeBuffer(0, &pickingBufferInfo).build(pickingObjUboDescriptorSets[i]);
		}

		VkDescriptorImageInfo skyBoximageInfo{};
		skyBoximageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		skyBoximageInfo.imageView = GameObjectManager::GetSingleton().gameObjects[1].model->getTexture(0).view;
		skyBoximageInfo.sampler = GameObjectManager::GetSingleton().gameObjects[1].model->getTexture(0).sampler;

		for (int i = 0; i < CubeBoxDescriptorSets.size(); i++)
		{
			DescriptorWriter(*descSetLayouts[1], *globalPools[1]).writeImage(0, &skyBoximageInfo).build(CubeBoxDescriptorSets[i]);
		}

		// should create pbr resource images using pipeline once
		pbrSourceGenerator = std::make_unique<PBRResourceGenerator>(device, std::vector { descSetLayouts[0]->getDescriptorSetLayout(), descSetLayouts[1]->getDescriptorSetLayout() }, std::vector { globalDescriptorSets[0], CubeBoxDescriptorSets[0]});
		pbrSourceGenerator->createPBRResource();

		VkDescriptorImageInfo brdfImgInfo{};
		brdfImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		brdfImgInfo.imageView = pbrSourceGenerator->lutBrdfView;
		brdfImgInfo.sampler = pbrSourceGenerator->lutBrdfSampler;
		VkDescriptorImageInfo irradianceImgInfo{};
		irradianceImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		irradianceImgInfo.imageView = pbrSourceGenerator->IrradianceCubeImgView;
		irradianceImgInfo.sampler = pbrSourceGenerator->IrradianceCubeSampler;
		VkDescriptorImageInfo prefilterImgInfo{};
		prefilterImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		prefilterImgInfo.imageView = pbrSourceGenerator->preFilterCubeImgView;
		prefilterImgInfo.sampler = pbrSourceGenerator->preFilterCubeSampler;

		std::vector<VkDescriptorImageInfo> descImageInfos = { brdfImgInfo, irradianceImgInfo, prefilterImgInfo };
		// for image sampler descriptor pool
		for (int i = 0; i < pbrResourceDescriptorSets.size(); i++)
		{
			DescriptorWriter(*descSetLayouts[2], *globalPools[2]).writeImage(0, &descImageInfos[0]).writeImage(1, &descImageInfos[1])
				.writeImage(2, &descImageInfos[2]).build(pbrResourceDescriptorSets[i]);
		}

		// for gltf color map and normal map and emissive, occlusion, metallicRoughness Textures
		// this time, only need damaged helmet materials info
		auto& gltfModels = GameObjectManager::GetSingleton().gameObjects;
		for (auto& gltfModel : gltfModels)
		{
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

		VkDescriptorImageInfo shadowMapImageInfo{};
		shadowMapImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		shadowMapImageInfo.imageView = shadowMapRenderSystem->GetShadowMap().view;
		shadowMapImageInfo.sampler = shadowMapRenderSystem->GetShadowMap().sampler;

		DescriptorWriter(*descSetLayouts[5], *globalPools[5]).writeImage(0, &shadowMapImageInfo)
			.build(shadowMapDescriptorSet);
	}

	bool JHBApplication::pickingPhase(VkCommandBuffer commandBuffer, GlobalUbo& ubo, int frameIndex, int x, int y)
	{
		void* data;
		float tmp;
		if (window.GetMousePressed() == true && window.objectId <0)
		{
			renderer.beginSwapChainRenderPass(commandBuffer, mousePickingRenderSystem->pickingRenderpass, mousePickingRenderSystem->offscreenFrameBuffer[frameIndex], window.getExtent());
			mousePickingRenderSystem->renderMousePickedObjToOffscreen(commandBuffer,  {globalDescriptorSets[frameIndex], pickingObjUboDescriptorSets[frameIndex]}, frameIndex, uboPickingIndexBuffer[frameIndex].get());
			renderer.endSwapChainRenderPass(commandBuffer);

			// check object id from a pixel whicch located in mouse pointer coordinate
			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			VkDeviceSize imageSize = window.getExtent().width * window.getExtent().height * 16; // 4byte per pixel
			device.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, stagingBuffer, stagingBufferMemory);

			auto offscreenCmd = device.beginSingleTimeCommands();
			device.transitionImageLayout(offscreenCmd, mousePickingRenderSystem->offscreenImage[frameIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			device.endSingleTimeCommands(offscreenCmd);
			device.copyImageToBuffer(stagingBuffer, mousePickingRenderSystem->offscreenImage[frameIndex], window.getExtent().width, window.getExtent().height);

			vkMapMemory(device.getLogicalDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
			int offset = (window.getExtent().width * ((int)y - 1) + (int)x) * 4;
			window.objectId = *((uint32_t*)data + offset);
			vkUnmapMemory(device.getLogicalDevice(), stagingBufferMemory);
			if (window.objectId <= 0)
			{
				return false;
			}
		}

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
