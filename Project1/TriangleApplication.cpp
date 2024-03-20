#include "TriangleApplication.h"
#include "SimpleRenderSystem.h"

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


		while (!glfwWindowShouldClose(&window.GetGLFWwindow()))
		{
			glfwPollEvents();

			if (auto commandBuffer = renderer.beginFrame()) // begine frame return null pointer if swap chain need recreated
			{
				// this is why beginFram and beginswapchian renderpass are not combined;
				// because main application control over this multiple render pass like reflections, shadows, post-processing effects
				renderer.beginSwapChainRenderPass(commandBuffer);
				simpleRenderSystem.renderGameObjects(commandBuffer, gameObjects);
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
		cube.transform.translation = { .0f, .0f, .5f };
		cube.transform.scale = { .5f, .5f, .5f };
		gameObjects.push_back(std::move(cube));
	}

	std::unique_ptr<Model> HelloTriangleApplication::createCubeModel(Device& device, glm::vec3 offset)
	{
		std::vector<Model::Vertex> vertices{
			// left face (white)
			{ {-.5f, -.5f, -.5f}, { .9f, .9f, .9f }},
			{ {-.5f, .5f, .5f}, {.9f, .9f, .9f} },
			{ {-.5f, -.5f, .5f}, {.9f, .9f, .9f} },
			{ {-.5f, -.5f, -.5f}, {.9f, .9f, .9f} },
			{ {-.5f, .5f, -.5f}, {.9f, .9f, .9f} },
			{ {-.5f, .5f, .5f}, {.9f, .9f, .9f} },

				// right face (yellow)
			{ {.5f, -.5f, -.5f}, {.8f, .8f, .1f} },
			{ {.5f, .5f, .5f}, {.8f, .8f, .1f} },
			{ {.5f, -.5f, .5f}, {.8f, .8f, .1f} },
			{ {.5f, -.5f, -.5f}, {.8f, .8f, .1f} },
			{ {.5f, .5f, -.5f}, {.8f, .8f, .1f} },
			{ {.5f, .5f, .5f}, {.8f, .8f, .1f} },

				// top face (orange, remember y axis points down)
			{ {-.5f, -.5f, -.5f}, {.9f, .6f, .1f} },
			{ {.5f, -.5f, .5f}, {.9f, .6f, .1f} },
			{ {-.5f, -.5f, .5f}, {.9f, .6f, .1f} },
			{ {-.5f, -.5f, -.5f}, {.9f, .6f, .1f} },
			{ {.5f, -.5f, -.5f}, {.9f, .6f, .1f} },
			{ {.5f, -.5f, .5f}, {.9f, .6f, .1f} },

				// bottom face (red)
			{ {-.5f, .5f, -.5f}, {.8f, .1f, .1f} },
			{ {.5f, .5f, .5f}, {.8f, .1f, .1f} },
			{ {-.5f, .5f, .5f}, {.8f, .1f, .1f} },
			{ {-.5f, .5f, -.5f}, {.8f, .1f, .1f} },
			{ {.5f, .5f, -.5f}, {.8f, .1f, .1f} },
			{ {.5f, .5f, .5f}, {.8f, .1f, .1f} },

				// nose face (blue)
			{ {-.5f, -.5f, 0.5f}, {.1f, .1f, .8f} },
			{ {.5f, .5f, 0.5f}, {.1f, .1f, .8f} },
			{ {-.5f, .5f, 0.5f}, {.1f, .1f, .8f} },
			{ {-.5f, -.5f, 0.5f}, {.1f, .1f, .8f} },
			{ {.5f, -.5f, 0.5f}, {.1f, .1f, .8f} },
			{ {.5f, .5f, 0.5f}, {.1f, .1f, .8f} },

				// tail face (green)
			{ {-.5f, -.5f, -0.5f}, {.1f, .8f, .1f} },
			{ {.5f, .5f, -0.5f}, {.1f, .8f, .1f} },
			{ {-.5f, .5f, -0.5f}, {.1f, .8f, .1f} },
			{ {-.5f, -.5f, -0.5f}, {.1f, .8f, .1f} },
			{ {.5f, -.5f, -0.5f}, {.1f, .8f, .1f} },
			{ {.5f, .5f, -0.5f}, {.1f, .8f, .1f} },
		};

		for (auto v : vertices)
		{
			v.position += offset;
		}
		return std::make_unique<Model>(device, vertices);
	}
}