#include "TriangleApplication.h"
#include "SimpleRenderSystem.h"
#include "SkyBoxRenderSystem.h"
#include "InputController.h"
#include "FrameInfo.h"
#include "Descriptors.h"
#include "PointLightSystem.h"
#include "Model.h"


#include <memory>
#include <numeric>

namespace jhb {
	HelloTriangleApplication::HelloTriangleApplication() 
	{
		globalPools[0] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT).build();
		globalPools[1] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT).build();
		globalPools[2] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT).build();
		// two descriptor sets
		// each descriptor set contain two UNIFORM_BUFFER descriptor
		createCube();
		loadGameObjects();
	}

	HelloTriangleApplication::~HelloTriangleApplication()
	{
	}

	void HelloTriangleApplication::Run()
	{
		std::vector<std::unique_ptr<Buffer>> uboBuffers(SwapChain::MAX_FRAMES_IN_FLIGHT);
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
		std::vector < std::unique_ptr<jhb::DescriptorSetLayout>> descSetLayouts;
		descSetLayouts.push_back(DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS).
			build());
		descSetLayouts.push_back(DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).build());
		// this global set is used by all shaders
		descSetLayouts.push_back(DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).build());

		// for uniform buffer descriptor pool
		std::vector<VkDescriptorSet> globalDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
		for (int i = 0; i < globalDescriptorSets.size(); i++)
		{
			auto bufferInfo = uboBuffers[i]->descriptorInfo();
			DescriptorWriter(*descSetLayouts[0], *globalPools[0]).writeBuffer(0, &bufferInfo).build(globalDescriptorSets[i]);
		}

		std::vector<VkDescriptorSet> globalImageSamplerDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
		std::vector<VkDescriptorSet> CubeBoxDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
		VkDescriptorImageInfo vaseimageInfo{};
		vaseimageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vaseimageInfo.imageView = gameObjects[1].model->builder->textureImageview;
		vaseimageInfo.sampler = gameObjects[1].model->builder->textureSampler;
		VkDescriptorImageInfo skyBoximageInfo{};
		skyBoximageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		skyBoximageInfo.imageView = gameObjects[0].model->builder->textureImageview;
		skyBoximageInfo.sampler = gameObjects[0].model->builder->textureSampler;

		std::vector<VkDescriptorImageInfo> descImageInfos = { vaseimageInfo , skyBoximageInfo };

		// for image sampler descriptor pool
		// for vase texture;
		for (int i = 0; i < globalImageSamplerDescriptorSets.size(); i++)
		{
			DescriptorWriter(*descSetLayouts[1], *globalPools[1]).writeImage(0, &descImageInfos[0]).build(globalImageSamplerDescriptorSets[i]);
			DescriptorWriter(*descSetLayouts[2], *globalPools[2]).writeImage(0, &descImageInfos[1]).build(CubeBoxDescriptorSets[i]);
		}
		

		SimpleRenderSystem simpleRenderSystem{ device, renderer.getSwapChainRenderPass(), descSetLayouts};
		PointLightSystem pointLightSystem{ device, renderer.getSwapChainRenderPass(), descSetLayouts[0]->getDescriptorSetLayout() };
		SkyBoxRenderSystem skyboxRenderSystem{ device, renderer.getSwapChainRenderPass(), descSetLayouts };
		Camera camera{};

		auto viewerObject = GameObject::createGameObject();
		viewerObject.transform.translation.z = -2.5f;
		InputController cameraController{device.getWindow().GetGLFWwindow(), viewerObject};
		double x, y;
		auto currentTime = std::chrono::high_resolution_clock::now();
		while (!glfwWindowShouldClose(&window.GetGLFWwindow()))
		{
			glfwPollEvents(); //may block
			glfwGetCursorPos(&window.GetGLFWwindow(), &x, &y);
			auto newTime = std::chrono::high_resolution_clock::now();
			float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
			currentTime = newTime;
			window.mouseMove(x, y, frameTime, viewerObject);

			cameraController.moveInPlaneXZ(&window.GetGLFWwindow(), frameTime, viewerObject);
			camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

			float aspect = renderer.getAspectRatio();
			camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 200.f);
			if (auto commandBuffer = renderer.beginFrame()) // begine frame return null pointer if swap chain need recreated
			{
				int frameIndex = renderer.getFrameIndex();
				FrameInfo frameInfo{
					frameIndex,
					frameTime,
					commandBuffer,
					camera,
					globalDescriptorSets[frameIndex],
					globalImageSamplerDescriptorSets[frameIndex],
					CubeBoxDescriptorSets[frameIndex],
					gameObjects
				};

				// update part : resources
				GlobalUbo ubo{};
				ubo.projection = camera.getProjection();
				ubo.view = camera.getView();
				ubo.inverseView = camera.getInverseView();
				pointLightSystem.update(frameInfo, ubo);
				uboBuffers[frameIndex]->writeToBuffer(&ubo); // wrtie to using frame buffer index
				uboBuffers[frameIndex]->flush(); //not using coherent_bit flag, so must to flush memory manually
				// and now we need tell to pipeline object where this buffer is and how data within it's structure
				// so using descriptor

				// render part : vkcmd
				// this is why beginFram and beginswapchian renderpass are not combined;
				// because main application control over this multiple render pass like reflections, shadows, post-processing effects
				renderer.beginSwapChainRenderPass(commandBuffer);
				skyboxRenderSystem.renderSkyBox(frameInfo);
				simpleRenderSystem.renderGameObjects(frameInfo);
				pointLightSystem.render(frameInfo);
				renderer.endSwapChainRenderPass(commandBuffer);
				renderer.endFrame();
			}
		}

		vkDeviceWaitIdle(device.getLogicalDevice());
	}
	void HelloTriangleApplication::loadGameObjects()
	{
		std::shared_ptr<Model> model =
		Model::createModelFromFile(device, "Models/smooth_vase.obj", "Texture/vase_txture.jpg");
		auto flatVase = GameObject::createGameObject();
		flatVase.model = model;
		flatVase.transform.translation = { -.5f, .5f, 0.f };
		flatVase.transform.scale = { 3.f, 1.5f, 3.f };
		gameObjects.emplace(flatVase.getId(), std::move(flatVase));


		std::vector<glm::vec3> lightColors{
			{1.f, 1.f, 1.f},
		};

		for (int i = 0; i < lightColors.size(); i++)
		{
			auto pointLight = GameObject::makePointLight(1.f);
			pointLight.color = lightColors[i];
			auto rotateLight = glm::rotate(glm::mat4(1.f),(i * glm::two_pi<float>()/lightColors.size()), {0.f, -1.f, 0.f});
			pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, 1.f));
			gameObjects.emplace(pointLight.getId(), std::move(pointLight));
		}
	}

	void HelloTriangleApplication::createCube()
	{
		std::unique_ptr<jhb::Model::Builder> builder = std::make_unique<jhb::Model::Builder>(device);
		std::vector<std::string> cubefiles = { "Texture/right.jpg","Texture/left.jpg" , "Texture/top.jpg" ,"Texture/bottom.jpg" , "Texture/front.jpg" , "Texture/back.jpg" };
		builder->loadTextrue3D(cubefiles);
		builder->vertices = {
			{{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
	  {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},
	  {{-.5f, -.5f, .5f}, {.9f, .9f, .9f}},
	  {{-.5f, .5f, -.5f}, {.9f, .9f, .9f}},

	  // right face (yellow)
	  {{.5f, -.5f, -.5f}, {.8f, .8f, .5f}},
	  {{.5f, .5f, .5f}, {.8f, .8f, .5f}},
	  {{.5f, -.5f, .5f}, {.8f, .8f, .5f}},
	  {{.5f, .5f, -.5f}, {.8f, .8f, .5f}},

	  // top face (orange, remember y axis points down)
	  {{-.5f, -.5f, -.5f}, {.9f, .6f, .5f}},
	  {{.5f, -.5f, .5f}, {.9f, .6f, .5f}},
	  {{-.5f, -.5f, .5f}, {.9f, .6f, .5f}},
	  {{.5f, -.5f, -.5f}, {.9f, .6f, .5f}},

	  // bottom face (red)
	  {{-.5f, .5f, -.5f}, {.8f, .5f, .5f}},
	  {{.5f, .5f, .5f}, {.8f, .5f, .5f}},
	  {{-.5f, .5f, .5f}, {.8f, .5f, .5f}},
	  {{.5f, .5f, -.5f}, {.8f, .5f, .5f}},

	  // nose face (blue)
	  {{-.5f, -.5f, 0.5f}, {.5f, .5f, .8f}},
	  {{.5f, .5f, 0.5f}, {.5f, .5f, .8f}},
	  {{-.5f, .5f, 0.5f}, {.5f, .5f, .8f}},
	  {{.5f, -.5f, 0.5f}, {.5f, .5f, .8f}},

	  // tail face (green)
	  {{-.5f, -.5f, -0.5f}, {.5f, .8f, .5f}},
	  {{.5f, .5f, -0.5f}, {.5f, .8f, .5f}},
	  {{-.5f, .5f, -0.5f}, {.5f, .8f, .5f}},
	  {{.5f, -.5f, -0.5f}, {.5f, .8f, .5f}},
		};

		builder->indices = { 0,  1,  2,  0,  3,  1,  4,  5,  6,  4,  7,  5,  8,  9,  10, 8,  11, 9,
								12, 13, 14, 12, 15, 13, 16, 17, 18, 16, 19, 17, 20, 21, 22, 20, 23, 21 };

		std::shared_ptr<Model> cube = std::make_unique<Model>(device, std::move(builder));
		auto skyBox = GameObject::createGameObject();
		skyBox.model = cube;
		skyBox.transform.translation = { 0.f, 0.f, 0.f };
		skyBox.transform.scale = { 10.f, 10.f ,10.f };
		gameObjects.emplace(skyBox.getId(), std::move(skyBox));
	}
}