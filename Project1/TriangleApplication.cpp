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
		std::shared_ptr<Model> model = Model::createModelFromFile(device, "Models/smooth_vase.obj");

		auto gameObj= GameObject::createGameObject();
		gameObj.model = model;
		gameObj.transform.translation = { .0f, .0f, 2.0f };
		gameObj.transform.scale = glm::vec3(3.f);
		gameObjects.push_back(std::move(gameObj));
	}
}