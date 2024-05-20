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

#define _USE_MATH_DEFINESimgui
#include <math.h>
#include <memory>
#include <numeric>

namespace jhb {
	JHBApplication::JHBApplication() : instanceBuffer(Buffer(device, sizeof(InstanceData), 64, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
	{
		uboBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
		CubeBoxDescriptorSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
		globalDescriptorSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
		pbrResourceDescriptorSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);
		pickingObjUboDescriptorSets.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

		createCube();
		loadGameObjects();
		create2DModelForBRDFLUT();
		initDescriptorSets();
	}

	JHBApplication::~JHBApplication()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void JHBApplication::Run()
	{	
		std::vector<VkPushConstantRange> pushConstantRanges;
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		pushConstantRange.offset = 0;
		pushConstantRanges.push_back(pushConstantRange);
		pushConstantRanges[0].size = sizeof(gltfPushConstantData);


		imguiRenderSystem = std::make_unique<ImguiRenderSystem>(device, renderer.GetSwapChain());

		pickingPhaseInit({ pushConstantRanges[0], VkPushConstantRange{VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(uint32_t)}}, {descSetLayouts[0]->getDescriptorSetLayout(), descSetLayouts[4]->getDescriptorSetLayout()}); // todo : should add descriptorsetlayout for object index

		PBRRendererSystem pbrRenderSystem{ device, renderer.getSwapChainRenderPass(), {descSetLayouts[0]->getDescriptorSetLayout(), descSetLayouts[2]->getDescriptorSetLayout(),
		descSetLayouts[3]->getDescriptorSetLayout()
		},"shaders/pbr.vert.spv",
			"shaders/pbr.frag.spv" , pushConstantRanges, gameObjects[1].model->materials};
		pushConstantRanges[0].size = sizeof(PointLightPushConstants);
		pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

		PointLightSystem pointLightSystem{ device, renderer.getSwapChainRenderPass(), { descSetLayouts[0]->getDescriptorSetLayout()}, "shaders/point_light.vert.spv",
			"shaders/point_light.frag.spv" , pushConstantRanges };
		pushConstantRanges[0].size = sizeof(SimplePushConstantData);
		SkyBoxRenderSystem skyboxRenderSystem{ device, renderer.getSwapChainRenderPass(), { descSetLayouts[0]->getDescriptorSetLayout(), descSetLayouts[1]->getDescriptorSetLayout() } ,"shaders/skybox.vert.spv",
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
			window.getCamera()->setViewDirection(viewerObject.transform.translation, forwardDir);
			float aspect = renderer.getAspectRatio();
			window.getCamera()->setPerspectiveProjection(aspect, 0.1f, 200.f);

			auto commandBuffer = renderer.beginFrame();
			if (commandBuffer == nullptr) // begine frame return null pointer if swap chain need recreated
			{
				continue;
			}

			int frameIndex = renderer.getFrameIndex();
			FrameInfo frameInfo{
				frameIndex,
				frameTime,
				commandBuffer,
				*window.getCamera(),
				globalDescriptorSets[frameIndex],
				pbrResourceDescriptorSets[frameIndex],
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
			
			/*
			
create offscreen framebuffer and images,

if(mouse clicked event)
{
    first renderPass

    record object id to offscreen image to color attachment;

    end render pass

======================================================================
    
    second renderPass

    create staging buffer

    copy offscreen image to staging buffer
    
    data map to staging buffer

    get mouse click coordinate mx, my;

    compare mx, my to data's coordinate

    check the data's obejct id

    if object exist, then rotate only that object;
}

else
{
    평소 처럼 렌더링

}
			*/
			void* data;
		    uint32_t objectId;
			float tmp;
			if(window.GetMousePressed())
			{
				renderer.beginSwapChainRenderPass(commandBuffer, pickingRenderpass, offscreenFrameBuffer[frameIndex], window.getExtent());
				mousePickingRenderSystem->renderMousePickedObjToOffscreen(commandBuffer, gameObjects, {globalDescriptorSets[frameIndex], pickingObjUboDescriptorSets[frameIndex]}, frameIndex, &instanceBuffer, uboPickingIndexBuffer[frameIndex].get());
				renderer.endSwapChainRenderPass(commandBuffer);

				// check object id from a pixel whicch located in mouse pointer coordinate
				VkBuffer stagingBuffer;
				VkDeviceMemory stagingBufferMemory;
				VkDeviceSize imageSize = window.getExtent().width * window.getExtent().height * 16; // 4byte per pixel
				device.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, stagingBuffer, stagingBufferMemory);

				auto offscreenCmd = device.beginSingleTimeCommands();
				device.transitionImageLayout(offscreenCmd,offscreenImage[frameIndex], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				device.endSingleTimeCommands(offscreenCmd);
				device.copyImageToBuffer(stagingBuffer, offscreenImage[frameIndex], window.getExtent().width, window.getExtent().height);

				vkMapMemory(device.getLogicalDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
				int offset = (window.getExtent().width * ((int)y - 1) + (int)x)*4;
				objectId = *((uint32_t*)data + offset);

				//마우스 포인터 좌표를 objectId의 object 공간으로의 역변환을 해준 후 그 오브젝트 공간의 sphere로 투영시킨다.
			}

			renderer.beginSwapChainRenderPass(commandBuffer);

			skyboxRenderSystem.renderSkyBox(frameInfo);
			pbrRenderSystem.renderGameObjects(frameInfo, &instanceBuffer);
			pointLightSystem.renderGameObjects(frameInfo);
			renderer.endSwapChainRenderPass(commandBuffer);
			
			renderer.beginSwapChainRenderPass(commandBuffer, device.imguiRenderPass, imguiRenderSystem->framebuffers[frameIndex], window.getExtent());
			imguiRenderSystem->newFrame();
			ImDrawData* draw_data = ImGui::GetDrawData();
			ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);
			renderer.endSwapChainRenderPass(commandBuffer);
			renderer.endFrame();
			if (window.wasWindowResized())
			{
				renderer.setWindowExtent(window.getExtent());
				imguiRenderSystem->recreateFrameBuffer(device, renderer.GetSwapChain(), window.getExtent());
				window.resetWindowResizedFlag();
				continue;
			}
		}

		vkDeviceWaitIdle(device.getLogicalDevice());
	}

	void JHBApplication::loadGameObjects()
	{
		loadGLTFFile("Models/DamagedHelmet/DamagedHelmet.gltf");

		std::vector<glm::vec3> lightColors{
			{1.f, 1.f, 1.f},
		};

		for (int i = 0; i < lightColors.size(); i++)
		{
			auto pointLight = GameObject::makePointLight(1.f);
			pointLight.color = lightColors[i];
			pointLight.pointLight->lightIntensity = 10.f;
			auto rotateLight = glm::rotate(glm::mat4(1.f),(i * glm::two_pi<float>()/lightColors.size()), {0.f, -1.f, 0.f});
			pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(2.f, 1.f, 1.f, 1.f));
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

	  {{.5f, -.5f, -.5f}, {.8f, .8f, .5f}},
	  {{.5f, .5f, .5f}, {.8f, .8f, .5f}},
	  {{.5f, -.5f, .5f}, {.8f, .8f, .5f}},
	  {{.5f, .5f, -.5f}, {.8f, .8f, .5f}},

	  {{-.5f, -.5f, -.5f}, {.9f, .6f, .5f}},
	  {{.5f, -.5f, .5f}, {.9f, .6f, .5f}},
	  {{-.5f, -.5f, .5f}, {.9f, .6f, .5f}},
	  {{.5f, -.5f, -.5f}, {.9f, .6f, .5f}},

	  {{-.5f, .5f, -.5f}, {.8f, .5f, .5f}},
	  {{.5f, .5f, .5f}, {.8f, .5f, .5f}},
	  {{-.5f, .5f, .5f}, {.8f, .5f, .5f}},
	  {{.5f, .5f, -.5f}, {.8f, .5f, .5f}},

	  {{-.5f, -.5f, 0.5f}, {.5f, .5f, .8f}},
	  {{.5f, .5f, 0.5f}, {.5f, .5f, .8f}},
	  {{-.5f, .5f, 0.5f}, {.5f, .5f, .8f}},
	  {{.5f, -.5f, 0.5f}, {.5f, .5f, .8f}},

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

	void JHBApplication::loadGLTFFile(const std::string& filename)
	{
		tinygltf::Model glTFInput;
		tinygltf::TinyGLTF gltfContext;
		std::string error, warning;

		bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);

		size_t pos = filename.find_last_of('/');

		std::vector<uint32_t> indexBuffer;
		std::vector<Vertex> vertexBuffer;

		auto gameModel = GameObject::createGameObject();
		gameModel.transform.translation = { -.5f, 1.5f, 0.f };
		gameModel.transform.scale = { 1.f, 1.f, 1.f };
		gameModel.transform.rotation = glm::vec3{ 5.f, 2.f, 6.f };

		std::shared_ptr<Model> model = std::make_shared<Model>(device, gameModel.transform.mat4());
		gameModel.model = model;

		model->path = filename.substr(0, pos);

		if (fileLoaded) {
			model->loadImages(glTFInput);
			model->loadMaterials(glTFInput);
			model->loadTextures(glTFInput);
			const tinygltf::Scene& scene = glTFInput.scenes[0];
			for (size_t i = 0; i < scene.nodes.size(); i++) {
				const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
				model->loadNode(node, glTFInput, nullptr, indexBuffer, vertexBuffer);
			}

			if (!model->hasTangent)
			{
				for (int i = 0; i < indexBuffer.size(); i+=3)
				{
					Vertex& vertex1 = vertexBuffer[indexBuffer[i]];
					Vertex& vertex2 = vertexBuffer[indexBuffer[i+1]];
					Vertex& vertex3 = vertexBuffer[indexBuffer[i+2]];
					model->calculateTangent(vertex1.uv, vertex2.uv, vertex3.uv, vertex1.position, vertex2.position, vertex3.position, vertex1.tangent);
					model->calculateTangent(vertex2.uv, vertex1.uv, vertex3.uv, vertex2.position, vertex1.position, vertex3.position, vertex2.tangent);
					model->calculateTangent(vertex3.uv, vertex2.uv, vertex1.uv, vertex3.position, vertex2.position, vertex1.position, vertex3.tangent);
				}
			}
		}
		else {
			throw std::runtime_error("Could not open the glTF file.\n\nMake sure the assets submodule has been checked out and is up-to-date.");
			return;
		}

		model->createVertexBuffer(vertexBuffer);
		model->createIndexBuffer(indexBuffer);

		gameObjects.emplace(gameModel.getId(), std::move(gameModel));
	}

	void JHBApplication::initDescriptorSets()
	{
		globalPools[0] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT).build();
		// ubo
		globalPools[1] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT).build(); // skybox

		globalPools[2] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT * 3).build();	// prefiter, brud, irradiane

		// for gltf model color map and normal map and emissive, occlusion, metallicRoughness Textures
		globalPools[3] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SwapChain::MAX_FRAMES_IN_FLIGHT * 5).build();

		// for picking object index storage
		globalPools[4] = DescriptorPool::Builder(device).setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT).addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT).build();

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
		skyBoximageInfo.imageView = gameObjects[0].model->getTexture(0).view;
		skyBoximageInfo.sampler = gameObjects[0].model->getTexture(0).sampler;

		for (int i = 0; i < CubeBoxDescriptorSets.size(); i++)
		{
			DescriptorWriter(*descSetLayouts[1], *globalPools[1]).writeImage(0, &skyBoximageInfo).build(CubeBoxDescriptorSets[i]);
		}

		// should create pbr resource images using pipeline once
		generateBRDFLUT();
		generateIrradianceCube({ descSetLayouts[0]->getDescriptorSetLayout(), descSetLayouts[1]->getDescriptorSetLayout() }, {globalDescriptorSets[0], CubeBoxDescriptorSets[0]});
		generatePrefilteredCube({ descSetLayouts[0]->getDescriptorSetLayout(), descSetLayouts[1]->getDescriptorSetLayout() }, { globalDescriptorSets[0], CubeBoxDescriptorSets[0]});

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

		std::vector<VkDescriptorImageInfo> descImageInfos = { brdfImgInfo, irradianceImgInfo, prefilterImgInfo };
		// for image sampler descriptor pool
		for (int i = 0; i < pbrResourceDescriptorSets.size(); i++)
		{
			DescriptorWriter(*descSetLayouts[2], *globalPools[2]).writeImage(0, &descImageInfos[0]).writeImage(1, &descImageInfos[1])
				.writeImage(2, &descImageInfos[2]).build(pbrResourceDescriptorSets[i]);
		}

		// for gltf color map and normal map and emissive, occlusion, metallicRoughness Textures
		auto gltfModel = gameObjects[1].model;
		for (auto& material : gltfModel->materials)
		{
			std::vector<VkDescriptorImageInfo> imageinfos = { gltfModel->getTexture(material.baseColorTextureIndex).descriptor, gltfModel->getTexture(material.normalTextureIndex).descriptor
			,gltfModel->getTexture(material.occlusionTextureIndex).descriptor, gltfModel->getTexture(material.emissiveTextureIndex).descriptor, gltfModel->getTexture(material.metallicRoughnessTextureIndex).descriptor
			};
			for (int i = 0; i < SwapChain::MAX_FRAMES_IN_FLIGHT; i++)
			{
				DescriptorWriter(*descSetLayouts[3], *globalPools[3]).writeImage(0, &imageinfos[0]).writeImage(1, &imageinfos[1])
					.writeImage(2, &imageinfos[2]).writeImage(3, &imageinfos[3]).writeImage(4, &imageinfos[4])
					.build(material.descriptorSets[i]);
			}
		}
	}

	void JHBApplication::pickingPhaseInit(const std::vector<VkPushConstantRange>& pushConstantRanges, const std::vector<VkDescriptorSetLayout>& desclayouts)
	{
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

		VkAttachmentDescription attDesc = {};
		// Color attachment
		attDesc.format = VK_FORMAT_R32G32B32A32_UINT;
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

		if (vkCreateRenderPass(device.getLogicalDevice(), &renderPassCI, nullptr, &pickingRenderpass))
		{
			throw std::runtime_error("failed to create renderpass!");
		}


		for(int i =0;i<SwapChain::MAX_FRAMES_IN_FLIGHT;i++)
		{
			// Pre-filtered cube map
			// Image
			VkImageCreateInfo imageCI{};
			imageCI.imageType = VK_IMAGE_TYPE_2D;
			imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageCI.format = VK_FORMAT_R32G32B32A32_UINT;
			imageCI.extent.width = window.getExtent().width;
			imageCI.extent.height = window.getExtent().height;
			imageCI.extent.depth = 1;
			imageCI.mipLevels = 1;
			imageCI.arrayLayers = 1;
			imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			device.createImageWithInfo(imageCI, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, offscreenImage[i], offscreenMemory[i]);
			// Image view
			VkImageViewCreateInfo viewCI{};
			viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewCI.format = VK_FORMAT_R32G32B32A32_UINT;
			viewCI.subresourceRange = {};
			viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewCI.subresourceRange.levelCount = 1;
			viewCI.subresourceRange.layerCount = 1;
			viewCI.image = offscreenImage[i];
			if (vkCreateImageView(device.getLogicalDevice(), &viewCI, nullptr, &offscreenImageView[i]))
			{
				throw std::runtime_error("failed to create ImageView!");
			}

			std::vector<VkImageView> attachments = { offscreenImageView[i]};

			VkFramebufferCreateInfo fbufCreateInfo{};
			fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbufCreateInfo.renderPass = pickingRenderpass;
			fbufCreateInfo.attachmentCount = attachments.size();
			fbufCreateInfo.pAttachments = attachments.data();
			fbufCreateInfo.width = window.getExtent().width;
			fbufCreateInfo.height = window.getExtent().height;
			fbufCreateInfo.layers = 1;

			if (vkCreateFramebuffer(device.getLogicalDevice(), &fbufCreateInfo, nullptr, &offscreenFrameBuffer[i]))
			{
				throw std::runtime_error("failed to create frameBuffer!");
			}
		}

		mousePickingRenderSystem = std::make_unique<MousePickingRenderSystem>(device, pickingRenderpass, desclayouts, "shaders/pbr.vert.spv", "shaders/picking.frag.spv", pushConstantRanges);
	}

	void JHBApplication::generateBRDFLUT()
	{
		const VkFormat format = VK_FORMAT_R16G16_SFLOAT;	// R16G16 is supported pretty much everywhere
		const int32_t dim = 512;

		DescriptorPool::Builder(device).setMaxSets(1)
			.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1).build();

		auto descriptorSetLayout = DescriptorSetLayout::Builder(device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS).
			build();
		// no apply descriptorset to descriptorsetlayout but allcoate descriptorsetlayout and descritptor pool in william sascha example

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

		PBRResourceGenerator BRDFLUTGenerator{ device, renderpass, {descriptorSetLayout->getDescriptorSetLayout()},"shaders/genbrdflut.vert.spv",
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
		BRDFLUTGenerator.bindPipeline(cmd);
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

		BRDFLUTGenerator.generateBRDFLUT(cmd, gameObjects[3]);
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

		PBRResourceGenerator irradianceCubeGenerator{ device, renderpass, desclayouts ,"shaders/filtercube.vert.spv",
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

				// Render scene from cube face's point of view
				vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
				// Update shader push constant block
				pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

				irradianceCubeGenerator.bindPipeline(cmdBuf);
				irradianceCubeGenerator.generateIrradianceCube(cmdBuf, gameObjects[0], descSets, pushBlock);
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

		PBRResourceGenerator prefilteredCubeGenerator{ device, renderpass, desclayouts ,"shaders/filtercube.vert.spv",
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

		prefilteredCubeGenerator.bindPipeline(cmdBuf);
		device.transitionImageLayout(cmdBuf, preFilterCubeImg, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

		for (uint32_t m = 0; m < numMips; m++) {
			pushBlock.roughness = (float)m / (float)(numMips - 1);
			for (uint32_t f = 0; f < 6; f++) {
				viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
				viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
				vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

				// Render scene from cube face's point of view
				vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				// Update shader push constant block
				pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];
				prefilteredCubeGenerator.bindPipeline(cmdBuf);

				prefilteredCubeGenerator.generatePrefilteredCube(cmdBuf, gameObjects[0], descSets, pushBlock);
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