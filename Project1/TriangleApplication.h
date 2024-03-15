#pragma once
#include "Window.h"
#include "Pipeline.h"
#include "Device.h"
#include "SwapChain.h"

#include <stdint.h>

namespace jhb {
	class HelloTriangleApplication {
	public:
		HelloTriangleApplication(uint32_t width, uint32_t height);

		void Run();

	private:
		Window window{ 800, 600, "TriangleApp!" };
		Device device{ window };
		SwapChain swapchain{ device, window.getExtent() };
		std::unique_ptr<Pipeline> pipeline;
		VkPipelineLayout pipelineLayout;
		std::vector<VkCommandBuffer> commandBuffer;
	};
}