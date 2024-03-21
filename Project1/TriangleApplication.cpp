#include "TriangleApplication.h"
#include "SimpleRenderSystem.h"
#include "KeyboardController.h"

#include <memory>
#include <array>

namespace jhb {
	HelloTriangleApplication::HelloTriangleApplication() {
		loadGameObjects();
	}

	HelloTriangleApplication::~HelloTriangleApplication()
	{
	}

	void HelloTriangleApplication::Run()
	{
		SimpleRenderSystem simpleRenderSystem{ device, renderer.getSwapChainRenderPass() };
		Camera camera{};

		auto viewerObject = GameObject::createGameObject();
		KeyboardController cameraController{};

		auto currentTime = std::chrono::high_resolution_clock::now();
		while (!glfwWindowShouldClose(&window.GetGLFWwindow()))
		{
			glfwPollEvents(); //may block

			auto newTime = std::chrono::high_resolution_clock::now();
			float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
			currentTime = newTime;

			cameraController.moveInPlaneXZ(&window.GetGLFWwindow(), frameTime, viewerObject);
			camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);


			float aspect = renderer.getAspectRatio();
			camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 10.f);
			if (auto commandBuffer = renderer.beginFrame()) // begine frame return null pointer if swap chain need recreated
			{
				// this is why beginFram and beginswapchian renderpass are not combined;
				// because main application control over this multiple render pass like reflections, shadows, post-processing effects
				renderer.beginSwapChainRenderPass(commandBuffer);
				simpleRenderSystem.renderGameObjects(commandBuffer, gameObjects, camera);
				renderer.endSwapChainRenderPass(commandBuffer);
				renderer.endFrame();
			}
		}

		vkDeviceWaitIdle(device.getLogicalDevice());
	}
	void HelloTriangleApplication::loadGameObjects()
	{
		std::shared_ptr<Model> model = createCubeModel(device, {0.f, 0.f, 0.f});

		auto cube = GameObject::createGameObject();
		cube.model = model;
		cube.transform.translation = { .0f, .0f, 2.0f };
		cube.transform.scale = { .5f, .5f, .5f };
		gameObjects.push_back(std::move(cube));
	}

	std::unique_ptr<Model> HelloTriangleApplication::createCubeModel(Device& device, glm::vec3 offset)
	{
		Model::Builder modelBuilder{};
		modelBuilder.vertices = {
			// left face (white)
			{ {-.5f, -.5f, -.5f}, { .9f, .9f, .9f }},
			{ {-.5f, .5f, .5f}, {.9f, .9f, .9f} },
			{ {-.5f, -.5f, .5f}, {.9f, .9f, .9f} },
			{ {-.5f, .5f, -.5f}, {.9f, .9f, .9f} },

				// right face (yellow)
			{ {.5f, -.5f, -.5f}, {.8f, .8f, .1f} },
			{ {.5f, .5f, .5f}, {.8f, .8f, .1f} },
			{ {.5f, -.5f, .5f}, {.8f, .8f, .1f} },
			{ {.5f, .5f, -.5f}, {.8f, .8f, .1f} },

				// top face (orange, remember y axis points down)
			{ {-.5f, -.5f, -.5f}, {.9f, .6f, .1f} },
			{ {.5f, -.5f, .5f}, {.9f, .6f, .1f} },
			{ {-.5f, -.5f, .5f}, {.9f, .6f, .1f} },
			{ {.5f, -.5f, -.5f}, {.9f, .6f, .1f} },

				// bottom face (red)
			{ {-.5f, .5f, -.5f}, {.8f, .1f, .1f} },
			{ {.5f, .5f, .5f}, {.8f, .1f, .1f} },
			{ {-.5f, .5f, .5f}, {.8f, .1f, .1f} },
			{ {.5f, .5f, -.5f}, {.8f, .1f, .1f} },

				// nose face (blue)
			{ {-.5f, -.5f, 0.5f}, {.1f, .1f, .8f} },
			{ {.5f, .5f, 0.5f}, {.1f, .1f, .8f} },
			{ {-.5f, .5f, 0.5f}, {.1f, .1f, .8f} },
			{ {.5f, -.5f, 0.5f}, {.1f, .1f, .8f} },

				// tail face (green)
			{ {-.5f, -.5f, -0.5f}, {.1f, .8f, .1f} },
			{ {.5f, .5f, -0.5f}, {.1f, .8f, .1f} },
			{ {-.5f, .5f, -0.5f}, {.1f, .8f, .1f} },
			{ {.5f, -.5f, -0.5f}, {.1f, .8f, .1f} },
		};

		modelBuilder.indices = { 0,  1,  2,  0,  3,  1,  4,  5,  6,  4,  7,  5,  8,  9,  10, 8,  11, 9,
						 12, 13, 14, 12, 15, 13, 16, 17, 18, 16, 19, 17, 20, 21, 22, 20, 23, 21 };

		for (auto v : modelBuilder.vertices)
		{
			v.position += offset;
		}
		return std::make_unique<Model>(device, modelBuilder);
	}
}