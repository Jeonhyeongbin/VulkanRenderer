#pragma once
#include "Window.h"
#include "Pipeline.h"
#include "Device.h"
#include "SwapChain.h"
#include "Model.h"

#include <stdint.h>

namespace jhb {
	class HelloTriangleApplication {
	public:
		HelloTriangleApplication(uint32_t width, uint32_t height);
		~HelloTriangleApplication();

		HelloTriangleApplication(const HelloTriangleApplication&) = delete;
		HelloTriangleApplication(HelloTriangleApplication&&) = delete;
		HelloTriangleApplication& operator=(const HelloTriangleApplication&) = delete;

		void Run();

	private:
		void loadModels();
		void createPipeLineLayout();
		void createPipeline();
		void createCommandBuffers();
		void drawFrame();

		// init top to bottom
		Window window{ 800, 600, "TriangleApp!" };
		Device device{ window };
		SwapChain swapChain{ device, window.getExtent() };
		std::unique_ptr<Pipeline> pipeline;
		VkPipelineLayout pipelineLayout;
		std::vector<VkCommandBuffer> commandBuffers;
		std::unique_ptr<Model> model;
	};
}