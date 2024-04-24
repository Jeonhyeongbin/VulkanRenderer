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


#define _USE_MATH_DEFINESimgui
#include <math.h>
#include <memory>
#include <numeric>

namespace jhb {
	JHBApplication::JHBApplication() : instanceBuffer(Buffer(device, sizeof(InstanceData), 64, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
	{
		globalPools[0] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT).build();
		// ubo and instancing
		globalPools[1] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT).build(); // brud
		globalPools[2] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT).build(); // skybox
		globalPools[3] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT).build();	// irradiane
		globalPools[4] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT).build(); //prefiter
		
		// for glft model matrix
		globalPools[5] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT).build();

		// for gltf model color map and normal map
		globalPools[6] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT * 2).build(); 
	

		// two descriptor sets
		// each descriptor set contain two UNIFORM_BUFFER descriptor
		createCube();
		loadGameObjects();
		create2DModelForBRDFLUT();
	}

	JHBApplication::~JHBApplication()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void JHBApplication::Run()
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


		std::vector<std::unique_ptr<jhb::DescriptorSetLayout>> descSetLayouts;
		descSetLayouts.push_back(DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS).
			build());
		descSetLayouts.push_back(DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).build());
		// this global set is used by all shaders
		descSetLayouts.push_back(DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).build());
		descSetLayouts.push_back(DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).build());
		descSetLayouts.push_back(DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT).build());

		// for uniform buffer descriptor pool
		std::vector<VkDescriptorSet> globalDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
		for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
		{
			auto bufferInfo = uboBuffers[i]->descriptorInfo();
			DescriptorWriter(*descSetLayouts[0], *globalPools[0]).writeBuffer(0, &bufferInfo).build(globalDescriptorSets[i]);
		}


		std::vector<VkDescriptorSet> CubeBoxDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT); // skybox
		VkDescriptorImageInfo skyBoximageInfo{};
		skyBoximageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		skyBoximageInfo.imageView = gameObjects[0].model->getTexture(0).view;
		skyBoximageInfo.sampler = gameObjects[0].model->getTexture(0).sampler;

		std::vector<VkDescriptorSetLayout> desclayouts = { descSetLayouts[0]->getDescriptorSetLayout() , descSetLayouts[2]->getDescriptorSetLayout() };

		for (int i = 0; i < CubeBoxDescriptorSets.size(); i++)
		{
			DescriptorWriter(*descSetLayouts[2], *globalPools[2]).writeImage(0, &skyBoximageInfo).build(CubeBoxDescriptorSets[i]);
		}
		std::vector<VkDescriptorSet> descSets = { globalDescriptorSets[0] , CubeBoxDescriptorSets[0]};

		generateBRDFLUT(desclayouts, descSets);
		generateIrradianceCube(desclayouts, descSets);
		generatePrefilteredCube(desclayouts, descSets);

		std::vector<VkDescriptorSet> brdfImageSamplerDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
		std::vector<VkDescriptorSet> irradianceImageSamplerDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
		std::vector<VkDescriptorSet> prefilterImageSamplerDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
		//std::vector<VkDescriptorSet> imguiImageSamplerDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);
		VkDescriptorImageInfo brdfImgInfo{};
		brdfImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		brdfImgInfo.imageView = lutBrdfView;
		brdfImgInfo.sampler = lutBrdfSampler;
		VkDescriptorImageInfo irradianceImgInfo{};
		irradianceImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		irradianceImgInfo.imageView = IrradianceCubeImgView;
		irradianceImgInfo.sampler = IrradianceCubeSampler;
		VkDescriptorImageInfo prefilterImgInfo{};
		prefilterImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		prefilterImgInfo.imageView = preFilterCubeImgView;
		prefilterImgInfo.sampler = preFilterCubeSampler;
		
		std::vector<VkPushConstantRange> pushConstantRanges;
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT ; // This means that both vertex and fragment shader using constant 
		pushConstantRange.offset = 0;
		pushConstantRanges.push_back(pushConstantRange);

		std::vector<VkDescriptorSetLayout> desclayoutsForImgui = { };

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		std::vector<VkSubpassDependency> imguidepency = { dependency };
		imguiRenderSystem = std::make_unique<ImguiRenderSystem>(device, renderer.GetSwapChain());

		std::vector<VkDescriptorImageInfo> descImageInfos = { brdfImgInfo , skyBoximageInfo, irradianceImgInfo, prefilterImgInfo };
		// for image sampler descriptor pool
		// for vase texture;
		for (int i = 0; i < brdfImageSamplerDescriptorSets.size(); i++)
		{
			DescriptorWriter(*descSetLayouts[1], *globalPools[1]).writeImage(0, &descImageInfos[0]).build(brdfImageSamplerDescriptorSets[i]);
			DescriptorWriter(*descSetLayouts[3], *globalPools[3]).writeImage(0, &descImageInfos[2]).build(irradianceImageSamplerDescriptorSets[i]);
			DescriptorWriter(*descSetLayouts[4], *globalPools[4]).writeImage(0, &descImageInfos[3]).build(prefilterImageSamplerDescriptorSets[i]);
		}

		for (int i = 1; i <= 4; i++)
		{
			if (i == 3)
				continue;
			desclayouts.push_back(descSetLayouts[i]->getDescriptorSetLayout());
		}

		pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRanges[0].size = sizeof(SimplePushConstantData);
		PBRRendererSystem simpleRenderSystem{ device, renderer.getSwapChainRenderPass(), desclayouts ,"shaders/shader.vert.spv",
			"shaders/shader.frag.spv" , pushConstantRanges };
		pushConstantRanges[0].size = sizeof(PointLightPushConstants);
		PointLightSystem pointLightSystem{ device, renderer.getSwapChainRenderPass(), desclayouts, "shaders/point_light.vert.spv",
			"shaders/point_light.frag.spv" , pushConstantRanges };
		pushConstantRanges[0].size = sizeof(SimplePushConstantData);
		SkyBoxRenderSystem skyboxRenderSystem{ device, renderer.getSwapChainRenderPass(), desclayouts ,"shaders/skybox.vert.spv",
			"shaders/skybox.frag.spv" , pushConstantRanges };

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
			
			auto forwardDir = cameraController.move(&window.GetGLFWwindow(), frameTime, viewerObject);
			//camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);
			window.getCamera()->setViewDirection(viewerObject.transform.translation, forwardDir);
			float aspect = renderer.getAspectRatio();
			window.getCamera()->setPerspectiveProjection(aspect, 0.1f, 200.f);
			if (auto commandBuffer = renderer.beginFrame()) // begine frame return null pointer if swap chain need recreated
			{
				int frameIndex = renderer.getFrameIndex();
				FrameInfo frameInfo{
					frameIndex,
					frameTime,
					commandBuffer,
					*window.getCamera(),
					globalDescriptorSets[frameIndex],
					brdfImageSamplerDescriptorSets[frameIndex],
					irradianceImageSamplerDescriptorSets[frameIndex],
					prefilterImageSamplerDescriptorSets[frameIndex],
					CubeBoxDescriptorSets[frameIndex],
					gameObjects
				};

				updateInstance();

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
				
				pointLightSystem.update(frameInfo, ubo);
				uboBuffers[frameIndex]->writeToBuffer(&ubo); // wrtie to using frame buffer index
				uboBuffers[frameIndex]->flush(); //not using coherent_bit flag, so must to flush memory manually
				// and now we need tell to pipeline object where this buffer is and how data within it's structure
				// so using descriptor

				// render part : vkcmd
				// this is why beginFram and beginswapchian renderpass are not combined;
				// because main application control over this multiple render pass like reflections, shadows, post-processing effects
				//renderer.beginSwapChainRenderPass(commandBuffer);
				
				renderer.beginSwapChainRenderPass(commandBuffer);

				skyboxRenderSystem.renderSkyBox(frameInfo);
				simpleRenderSystem.renderGameObjects(frameInfo, &instanceBuffer);
				pointLightSystem.renderGameObjects(frameInfo);
				renderer.endSwapChainRenderPass(commandBuffer);
				

				renderer.beginSwapChainRenderPass(commandBuffer, device.imguiRenderPass,imguiRenderSystem->framebuffers[frameIndex], window.getExtent());
				imguiRenderSystem->newFrame();
				ImDrawData* draw_data = ImGui::GetDrawData();
				ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);
				renderer.endSwapChainRenderPass(commandBuffer);
				renderer.endFrame();
			}
		}

		vkDeviceWaitIdle(device.getLogicalDevice());
	}
	void JHBApplication::loadGameObjects()
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
			pointLight.pointLight->lightIntensity = 1;
			auto rotateLight = glm::rotate(glm::mat4(1.f),(i * glm::two_pi<float>()/lightColors.size()), {0.f, -1.f, 0.f});
			pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(1.f, 1.f, 1.f, 1.f));
			gameObjects.emplace(pointLight.getId(), std::move(pointLight));
		}
	}

	void JHBApplication::createCube()
	{
		std::vector<std::string> cubefiles = {  };
		std::vector<Vertex> vertices = {
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

		std::vector<uint32_t> indices = { 0,  1,  2,  0,  3,  1,  4,  5,  6,  4,  7,  5,  8,  9,  10, 8,  11, 9,
								12, 13, 14, 12, 15, 13, 16, 17, 18, 16, 19, 17, 20, 21, 22, 20, 23, 21 };

		std::shared_ptr<Model> cube = std::make_unique<Model>(device);
		cube->createVertexBuffer(vertices);
		cube->createIndexBuffer(indices);
		cube->getTexture(0).loadKTXTexture(device, "Texture/pisa_cube.ktx", VK_IMAGE_VIEW_TYPE_CUBE, 6);
		auto skyBox = GameObject::createGameObject();
		skyBox.model = cube;
		skyBox.transform.translation = { 0.f, 0.f, 0.f };
		skyBox.transform.scale = { 10.f, 10.f ,10.f };
		gameObjects.emplace(skyBox.getId(), std::move(skyBox));
	}

	void JHBApplication::create2DModelForBRDFLUT()
	{
		std::vector<Vertex> vertices = {
			{{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
	  {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},
	  {{-.5f, -.5f, .5f}, {.9f, .9f, .9f}},
	
		};

		std::vector<uint32_t> indices = { 0,  1,  2};

		std::shared_ptr<Model> cube = std::make_unique<Model>(device);
		cube->createVertexBuffer(vertices);
		cube->createIndexBuffer(indices);
		cube->getTexture(0).loadKTXTexture(device, "Texture/pisa_cube.ktx", VK_IMAGE_VIEW_TYPE_CUBE, 6);
		auto skyBox = GameObject::createGameObject();
		skyBox.model = cube;
		skyBox.transform.translation = { 0.f, 0.f, 0.f };
		skyBox.transform.scale = { 10.f, 10.f ,10.f };
		gameObjects.emplace(skyBox.getId(), std::move(skyBox));
	}

	void JHBApplication::updateInstance()
	{
		std::vector<InstanceData> instanceData;
		instanceData.resize(64);
		
		float offsetx = 0.5f;
		float offsety = 0;
		for (float i = 0; i < 64; i+=8)
		{
			float y = offsety + (i/8) * 0.5f;
			for (float j = 0; j < 8; j++)
			{
				instanceData[i + j].pos.x += offsetx*j;
				instanceData[i + j].pos.y = y;
				instanceData[i + j].metallic = imguiRenderSystem->metalic;
				instanceData[i + j].roughness = imguiRenderSystem->roughness;
				instanceData[i + j].r = (i / 64.f );
				instanceData[i + j].g = 0.0f;
				instanceData[i + j].b = 0.0f;
			}
		}

		Buffer stagingBuffer(device, sizeof(InstanceData), 64, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		stagingBuffer.map();
		stagingBuffer.writeToBuffer(instanceData.data(), instanceBuffer.getBufferSize(), 0);
		
		device.copyBuffer(stagingBuffer.getBuffer(), instanceBuffer.getBuffer(), instanceBuffer.getBufferSize());
		stagingBuffer.unmap();
	}

	void JHBApplication::InitImgui()
	{
		
	}

	void JHBApplication::loadGLTFFile(const std::string& filename)
	{
		tinygltf::Model glTFInput;
		tinygltf::TinyGLTF gltfContext;
		std::string error, warning;

		bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);

		size_t pos = filename.find_last_of('/');

		std::vector<uint32_t> indexBuffer;
		std::vector<Vertex> vertexBuffer;
		std::unique_ptr<Model> model = std::make_unique<Model>(device);

		if (fileLoaded) {
			model->loadImages(glTFInput);
			model->loadMaterials(glTFInput);
			model->loadTextures(glTFInput);
			const tinygltf::Scene& scene = glTFInput.scenes[0];
			for (size_t i = 0; i < scene.nodes.size(); i++) {
				const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
				model->loadNode(node, glTFInput, nullptr, indexBuffer, vertexBuffer);
			}
		}
		else {
			throw std::runtime_error("Could not open the glTF file.\n\nMake sure the assets submodule has been checked out and is up-to-date.");
			return;
		}

		model->createVertexBuffer(vertexBuffer);
		model->createIndexBuffer(indexBuffer);
	}

	void JHBApplication::generateBRDFLUT(std::vector<VkDescriptorSetLayout> desclayouts, std::vector<VkDescriptorSet> descSets)
	{
		const VkFormat format = VK_FORMAT_R16G16_SFLOAT;	// R16G16 is supported pretty much everywhere
		const int32_t dim = 512;

		// Descriptors
		VkDescriptorSetLayout descriptorsetlayout;
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {};
		VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI{};
		descriptorsetlayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorsetlayoutCI.pBindings = setLayoutBindings.data();
		descriptorsetlayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
		if (vkCreateDescriptorSetLayout(device.getLogicalDevice(), &descriptorsetlayoutCI, nullptr, &descriptorsetlayout) != VK_SUCCESS)
		{
			throw std::runtime_error("descriptor set layout create failed!");
		}

		// Descriptor Pool
		std::vector<VkDescriptorPoolSize> poolSizes{};
		VkDescriptorPoolSize descriptorPoolSize{};
		descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorPoolSize.descriptorCount = 1;

		poolSizes.push_back(descriptorPoolSize);

		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		descriptorPoolInfo.pPoolSizes = poolSizes.data();
		descriptorPoolInfo.maxSets = 2;

		VkDescriptorPool descriptorpool;
		if (vkCreateDescriptorPool(device.getLogicalDevice(), &descriptorPoolInfo, nullptr, &descriptorpool) != VK_SUCCESS)
		{
			throw std::runtime_error("descriptor Pool create failed!");
		}

		// Descriptor sets
		VkDescriptorSet descriptorset;

		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.descriptorPool = descriptorpool;
		descriptorSetAllocateInfo.pSetLayouts = &descriptorsetlayout;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		if (vkAllocateDescriptorSets(device.getLogicalDevice(), &descriptorSetAllocateInfo, &descriptorset) != VK_SUCCESS)
		{
			throw std::runtime_error("descriptors Sets create failed!");
		}

		std::vector<VkSubpassDependency> tmpdependencies(2);
		tmpdependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		tmpdependencies[0].dstSubpass = 0;
		tmpdependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		tmpdependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		tmpdependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		tmpdependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		tmpdependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		tmpdependencies[1].srcSubpass = 0;
		tmpdependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		tmpdependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		tmpdependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		tmpdependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		tmpdependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		tmpdependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		VkImageCreateInfo imageCIa{};
		imageCIa.imageType = VK_IMAGE_TYPE_2D;
		imageCIa.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCIa.format = format;
		imageCIa.extent.width = dim;
		imageCIa.extent.height = dim;
		imageCIa.extent.depth = 1;
		imageCIa.mipLevels = 1;
		imageCIa.arrayLayers = 1;
		imageCIa.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCIa.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCIa.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		
		device.createImageWithInfo(imageCIa, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, lutBrdfImg, lutBrdfMemory);
		// Image view
		VkImageViewCreateInfo viewCIa{};
		viewCIa.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCIa.format = format;
		viewCIa.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCIa.subresourceRange = {};
		viewCIa.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCIa.subresourceRange.levelCount = 1;
		viewCIa.subresourceRange.layerCount = 1;
		viewCIa.image = lutBrdfImg;
		if (vkCreateImageView(device.getLogicalDevice(), &viewCIa, nullptr, &lutBrdfView))
		{
			throw std::runtime_error("failed to create ImageView!");
		}

		// Sampler
		VkSamplerCreateInfo samplerCI{};
		samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = 1.f;
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		if (vkCreateSampler(device.getLogicalDevice(), &samplerCI, nullptr, &lutBrdfSampler))
		{
			throw std::runtime_error("failed to create Sampler!");
		}

		std::vector<VkImageView> attachments = { lutBrdfView };


		std::vector<VkPushConstantRange> pushConstantRanges;
		
		VkAttachmentDescription attDesc = {};
		// Color attachment
		attDesc.format = format;
		attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;

		// Renderpass
		VkRenderPassCreateInfo renderPassCI{};
		renderPassCI.attachmentCount = 1;
		renderPassCI.pAttachments = &attDesc;
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDescription;
		renderPassCI.dependencyCount = 2;
		renderPassCI.pDependencies = tmpdependencies.data();
		renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		VkRenderPass renderpass;
		if (vkCreateRenderPass(device.getLogicalDevice(), &renderPassCI, nullptr, &renderpass))
		{
			throw std::runtime_error("failed to create renderpass!");
		}
		std::vector<VkDescriptorSetLayout> setlayouts = { descriptorsetlayout };
		SkyBoxRenderSystem skyboxRenderSystem{ device, renderpass, setlayouts ,"shaders/genbrdflut.vert.spv",
			"shaders/genbrdflut.frag.spv" , pushConstantRanges };

		VkFramebufferCreateInfo fbufCreateInfo{};
		fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbufCreateInfo.renderPass = renderpass;
		fbufCreateInfo.attachmentCount = attachments.size();
		fbufCreateInfo.pAttachments = attachments.data();
		fbufCreateInfo.width = dim;
		fbufCreateInfo.height = dim;
		fbufCreateInfo.layers = 1;

		VkFramebuffer frameBuffer;
		if (vkCreateFramebuffer(device.getLogicalDevice(), &fbufCreateInfo, nullptr, &frameBuffer))
		{
			throw std::runtime_error("failed to create frameBuffer!");
		}

		auto cmd = device.beginSingleTimeCommands();
		skyboxRenderSystem.bindPipeline(cmd);
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderpass;
		renderPassInfo.framebuffer = frameBuffer;

		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { (uint32_t)dim, (uint32_t)dim };

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { 0.01f, 0.1f, 0.1f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();


		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (uint32_t)dim;
		viewport.height = (uint32_t)dim;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor{ {0, 0}, {(uint32_t)dim, (uint32_t)dim} };
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		skyboxRenderSystem.renderSkyBox(cmd, gameObjects[3], descSets);
		vkCmdEndRenderPass(cmd);
		device.endSingleTimeCommands(cmd);

	}

	void JHBApplication::generateIrradianceCube(std::vector<VkDescriptorSetLayout> desclayouts, std::vector<VkDescriptorSet> descSets)
	{
		const VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
		const int32_t dim = 64;
		const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

		std::vector<VkSubpassDependency> tmpdependencies(2);
		tmpdependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		tmpdependencies[0].dstSubpass = 0;
		tmpdependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		tmpdependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		tmpdependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		tmpdependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		tmpdependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		tmpdependencies[1].srcSubpass = 0;
		tmpdependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		tmpdependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		tmpdependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		tmpdependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		tmpdependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		tmpdependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		IrradiencePushBlock pushBlock;

		std::vector<VkPushConstantRange> pushConstantRanges;
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // This means that both vertex and fragment shader using constant 
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(IrradiencePushBlock);
		pushConstantRanges.push_back(pushConstantRange);

		VkImage cubeImage;
		VkDeviceMemory cubeMemory;
		VkImageView cubeImageView;
		VkSampler cubeSampler;

		// Pre-filtered cube map
		// Image
		VkImageCreateInfo imageCIa{};
		imageCIa.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCIa.imageType = VK_IMAGE_TYPE_2D;
		imageCIa.format = format;
		imageCIa.extent.width = dim;
		imageCIa.extent.height = dim;
		imageCIa.extent.depth = 1;
		imageCIa.mipLevels = numMips;
		imageCIa.arrayLayers = 6;
		imageCIa.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCIa.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCIa.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCIa.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		device.createImageWithInfo(imageCIa, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, IrradianceCubeImg, IrradianceCubeMemory);
		// Image view
		VkImageViewCreateInfo viewCIa{};
		viewCIa.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCIa.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewCIa.format = format;
		viewCIa.subresourceRange = {};
		viewCIa.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCIa.subresourceRange.levelCount = numMips;
		viewCIa.subresourceRange.layerCount = 6;
		viewCIa.image = IrradianceCubeImg;
		if (vkCreateImageView(device.getLogicalDevice(), &viewCIa, nullptr, &IrradianceCubeImgView))
		{
			throw std::runtime_error("failed to create ImageView!");
		}

		// Sampler
		VkSamplerCreateInfo samplerCI{};
		samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = static_cast<float>(numMips);
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		if (vkCreateSampler(device.getLogicalDevice(), &samplerCI, nullptr, &IrradianceCubeSampler))
		{
			throw std::runtime_error("failed to create Sampler!");
		}

		// offscreen cube for hdr cube textre 2d
		VkImage offscreenImage;
		VkDeviceMemory offscreenMemory;
		VkImageView offscreenImageView;

		VkFramebuffer frameBuffer;
		VkRenderPass renderpass;
		{
			// Pre-filtered cube map
			// Image
			VkImageCreateInfo imageCI{};
			imageCI.imageType = VK_IMAGE_TYPE_2D;
			imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageCI.format = format;
			imageCI.extent.width = dim;
			imageCI.extent.height = dim;
			imageCI.extent.depth = 1;
			imageCI.mipLevels = 1;
			imageCI.arrayLayers = 6;
			imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			device.createImageWithInfo(imageCI, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreenImage, offscreenMemory);
			// Image view
			VkImageViewCreateInfo viewCI{};
			viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewCI.format = format;
			viewCI.subresourceRange = {};
			viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewCI.subresourceRange.levelCount = 1;
			viewCI.subresourceRange.layerCount = 1;
			viewCI.image = offscreenImage;
			if (vkCreateImageView(device.getLogicalDevice(), &viewCI, nullptr, &offscreenImageView))
			{
				throw std::runtime_error("failed to create ImageView!");
			}

			VkAttachmentDescription attDesc = {};
			// Color attachment
			attDesc.format = format;
			attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
			attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

			VkSubpassDescription subpassDescription = {};
			subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDescription.colorAttachmentCount = 1;
			subpassDescription.pColorAttachments = &colorReference;

			// Renderpass
			VkRenderPassCreateInfo renderPassCI{};
			renderPassCI.attachmentCount = 1;
			renderPassCI.pAttachments = &attDesc;
			renderPassCI.subpassCount = 1;
			renderPassCI.pSubpasses = &subpassDescription;
			renderPassCI.dependencyCount = 2;
			renderPassCI.pDependencies = tmpdependencies.data();
			renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

			if (vkCreateRenderPass(device.getLogicalDevice(), &renderPassCI, nullptr, &renderpass))
			{
				throw std::runtime_error("failed to create renderpass!");
			}



			std::vector<VkImageView> attachments = { offscreenImageView };

			VkFramebufferCreateInfo fbufCreateInfo{};
			fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbufCreateInfo.renderPass = renderpass;
			fbufCreateInfo.attachmentCount = attachments.size();
			fbufCreateInfo.pAttachments = attachments.data();
			fbufCreateInfo.width = dim;
			fbufCreateInfo.height = dim;
			fbufCreateInfo.layers = 1;

			if (vkCreateFramebuffer(device.getLogicalDevice(), &fbufCreateInfo, nullptr, &frameBuffer))
			{
				throw std::runtime_error("failed to create frameBuffer!");
			}

			VkCommandBuffer cmd = device.beginSingleTimeCommands();
			device.transitionImageLayout(cmd, offscreenImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			device.endSingleTimeCommands(cmd);
		}

		SkyBoxRenderSystem skyboxRenderSystem{ device, renderpass, desclayouts ,"shaders/filtercube.vert.spv",
	"shaders/irradiancecube.frag.spv" , pushConstantRanges };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderpass;
		renderPassInfo.framebuffer = frameBuffer;

		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { dim, dim };

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { 0.01f, 0.1f, 0.1f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		std::vector<glm::mat4> matrices = {
			// POSITIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		};

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = numMips;
		subresourceRange.layerCount = 6;

		auto cmdBuf = device.beginSingleTimeCommands();
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(dim);
		viewport.height = static_cast<float>(dim);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor{ {0, 0}, {(uint32_t)dim, (uint32_t)dim} };
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);


		device.transitionImageLayout(cmdBuf, IrradianceCubeImg, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

		for (uint32_t m = 0; m < numMips; m++) {
			for (uint32_t f = 0; f < 6; f++) {
				viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
				viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
				vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

				//// Render scene from cube face's point of view
				//vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
				// Update shader push constant block
				pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

				//vkCmdPushConstants(cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);
				skyboxRenderSystem.bindPipeline(cmdBuf);
				//vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
				//vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset, 0, NULL);
				skyboxRenderSystem.renderSkyBox(cmdBuf, gameObjects[0], descSets, pushBlock);
				vkCmdEndRenderPass(cmdBuf);

				device.transitionImageLayout(
					cmdBuf, offscreenImage, VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

				// Copy region for transfer from framebuffer to cube face
				VkImageCopy copyRegion = {};

				copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.srcSubresource.baseArrayLayer = 0;
				copyRegion.srcSubresource.mipLevel = 0;
				copyRegion.srcSubresource.layerCount = 1;
				copyRegion.srcOffset = { 0, 0, 0 };

				copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.dstSubresource.baseArrayLayer = f;
				copyRegion.dstSubresource.mipLevel = m;
				copyRegion.dstSubresource.layerCount = 1;
				copyRegion.dstOffset = { 0, 0, 0 };

				copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
				copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
				copyRegion.extent.depth = 1;

				vkCmdCopyImage(
					cmdBuf,
					offscreenImage,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					IrradianceCubeImg,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&copyRegion);

				// Transform framebuffer color attachment back

				device.transitionImageLayout(
					cmdBuf, offscreenImage, VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
				);
			}
		}

		device.transitionImageLayout(
			cmdBuf, IrradianceCubeImg,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange
		);
		device.endSingleTimeCommands(cmdBuf);
	}

	void JHBApplication::generatePrefilteredCube(std::vector<VkDescriptorSetLayout> desclayouts, std::vector<VkDescriptorSet> descSets)
	{
		const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
		const int32_t dim = 512;
		const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;
		
		std::vector<VkSubpassDependency> tmpdependencies(2);
		tmpdependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		tmpdependencies[0].dstSubpass = 0;
		tmpdependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		tmpdependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		tmpdependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		tmpdependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		tmpdependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		tmpdependencies[1].srcSubpass = 0;
		tmpdependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		tmpdependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		tmpdependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		tmpdependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		tmpdependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		tmpdependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


		PrefileterPushBlock pushBlock;
		
		std::vector<VkPushConstantRange> pushConstantRanges;
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; // This means that both vertex and fragment shader using constant 
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(PrefileterPushBlock);
		pushConstantRanges.push_back(pushConstantRange);

		VkImage cubeImage;
		VkDeviceMemory cubeMemory;
		VkImageView cubeImageView;
		VkSampler cubeSampler;

		// Pre-filtered cube map
		// Image
		VkImageCreateInfo imageCIa{};
		imageCIa.imageType = VK_IMAGE_TYPE_2D;
		imageCIa.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCIa.format = format;
		imageCIa.extent.width = dim;
		imageCIa.extent.height = dim;
		imageCIa.extent.depth = 1;
		imageCIa.mipLevels = numMips;
		imageCIa.arrayLayers = 6;
		imageCIa.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCIa.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCIa.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCIa.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		device.createImageWithInfo(imageCIa, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, preFilterCubeImg, preFilterCubeMemory);
		// Image view
		VkImageViewCreateInfo viewCIa{};
		viewCIa.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewCIa.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCIa.format = format;
		viewCIa.subresourceRange = {};
		viewCIa.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCIa.subresourceRange.levelCount = numMips;
		viewCIa.subresourceRange.layerCount = 6;
		viewCIa.image = preFilterCubeImg;
		if (vkCreateImageView(device.getLogicalDevice(), &viewCIa, nullptr, &preFilterCubeImgView))
		{
			throw std::runtime_error("failed to create ImageView!");
		}

		// Sampler
		VkSamplerCreateInfo samplerCI{};
		samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = static_cast<float>(numMips);
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		if (vkCreateSampler(device.getLogicalDevice(), &samplerCI, nullptr, &preFilterCubeSampler))
		{
			throw std::runtime_error("failed to create Sampler!");
		}

		// offscreen cube for hdr cube textre 2d
		VkImage offscreenImage;
		VkDeviceMemory offscreenMemory;
		VkImageView offscreenImageView;

		// Pre-filtered cube map
		// Image
		VkImageCreateInfo imageCI{};
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.format = format;
		imageCI.extent.width = dim;
		imageCI.extent.height = dim;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		device.createImageWithInfo(imageCI, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreenImage, offscreenMemory);
		// Image view
		VkImageViewCreateInfo viewCI{};
		viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCI.format = format;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = 1;
		viewCI.subresourceRange.layerCount = 1;
		viewCI.image = offscreenImage;
		if (vkCreateImageView(device.getLogicalDevice(), &viewCI, nullptr, &offscreenImageView))
		{
			throw std::runtime_error("failed to create ImageView!");
		}

		VkAttachmentDescription attDesc = {};
		// Color attachment
		attDesc.format = format;
		attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;

		// Renderpass
		VkRenderPassCreateInfo renderPassCI{};
		renderPassCI.attachmentCount = 1;
		renderPassCI.pAttachments = &attDesc;
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDescription;
		renderPassCI.dependencyCount = 2;
		renderPassCI.pDependencies = tmpdependencies.data();
		renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		VkRenderPass renderpass;
		if (vkCreateRenderPass(device.getLogicalDevice(), &renderPassCI, nullptr, &renderpass))
		{
			throw std::runtime_error("failed to create renderpass!");
		}

		SkyBoxRenderSystem skyboxRenderSystem{ device, renderpass, desclayouts ,"shaders/filtercube.vert.spv",
		"shaders/prefilterenvmap.frag.spv" , pushConstantRanges };
		


		std::vector<VkImageView> attachments = { offscreenImageView };

		VkFramebufferCreateInfo fbufCreateInfo{};
		fbufCreateInfo.renderPass = renderpass;
		fbufCreateInfo.attachmentCount = attachments.size();
		fbufCreateInfo.pAttachments = attachments.data();
		fbufCreateInfo.width = dim;
		fbufCreateInfo.height = dim;
		fbufCreateInfo.layers = 1;
		fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

		VkFramebuffer frameBuffer;
		if (vkCreateFramebuffer(device.getLogicalDevice(), &fbufCreateInfo, nullptr, &frameBuffer))
		{
			throw std::runtime_error("failed to create frameBuffer!");
		}

		VkCommandBuffer cmd = device.beginSingleTimeCommands();
		device.transitionImageLayout(cmd, offscreenImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		device.endSingleTimeCommands(cmd);

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderpass;
		renderPassInfo.framebuffer = frameBuffer;

		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { dim, dim };

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { 0.01f, 0.1f, 0.1f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		std::vector<glm::mat4> matrices = {
			// POSITIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		};

		
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = numMips;
		subresourceRange.layerCount = 6;

		auto cmdBuf = device.beginSingleTimeCommands();

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(dim);
		viewport.height = static_cast<float>(dim);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor{ {0, 0}, {(uint32_t)dim, (uint32_t)dim} };
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		skyboxRenderSystem.bindPipeline(cmdBuf);
		device.transitionImageLayout(cmdBuf, preFilterCubeImg, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

		for (uint32_t m = 0; m < numMips; m++) {
			pushBlock.roughness = (float)m / (float)(numMips - 1);
			for (uint32_t f = 0; f < 6; f++) {
				viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
				viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
				vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

				//// Render scene from cube face's point of view
				//vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
				
				vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				// Update shader push constant block
				pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];
				skyboxRenderSystem.bindPipeline(cmdBuf);
				//vkCmdPushConstants(cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

				//vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
				//vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset, 0, NULL);
				skyboxRenderSystem.renderSkyBox(cmdBuf, gameObjects[0], descSets, pushBlock);
				vkCmdEndRenderPass(cmdBuf);

				device.transitionImageLayout(
					cmdBuf, offscreenImage, VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

				// Copy region for transfer from framebuffer to cube face
				VkImageCopy copyRegion = {};

				copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.srcSubresource.baseArrayLayer = 0;
				copyRegion.srcSubresource.mipLevel = 0;
				copyRegion.srcSubresource.layerCount = 1;
				copyRegion.srcOffset = { 0, 0, 0 };

				copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.dstSubresource.baseArrayLayer = f;
				copyRegion.dstSubresource.mipLevel = m;
				copyRegion.dstSubresource.layerCount = 1;
				copyRegion.dstOffset = { 0, 0, 0 };

				copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
				copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
				copyRegion.extent.depth = 1;

				vkCmdCopyImage(
					cmdBuf,
					offscreenImage,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					preFilterCubeImg,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&copyRegion);

				// Transform framebuffer color attachment back

				device.transitionImageLayout(
				cmdBuf, offscreenImage, VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
				);
			}
		}

		device.transitionImageLayout(
			cmdBuf, preFilterCubeImg, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange
		);
		device.endSingleTimeCommands(cmdBuf);
	}
}