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

#include <stdint.h>
#include <chrono>
#include <array>

namespace jhb {
	class HelloTriangleApplication {
	public:
		HelloTriangleApplication();
		~HelloTriangleApplication();

		HelloTriangleApplication(const HelloTriangleApplication&) = delete;
		HelloTriangleApplication(HelloTriangleApplication&&) = delete;
		HelloTriangleApplication& operator=(const HelloTriangleApplication&) = delete;

		void Run();

	private:
		void loadGameObjects();
		

	private:
		// init top to bottom
		Window window{ 800, 600, "TriangleApp!" };
		Device device{ window };
		Renderer renderer{ window, device };

		std::array<std::unique_ptr<DescriptorPool>, 2> globalPools{};
		
		GameObject::Map gameObjects;
	};
}