#include "TriangleApplication.h"

namespace jhb {
	HelloTriangleApplication::HelloTriangleApplication(uint32_t width, uint32_t height) {
	}

	void HelloTriangleApplication::Run()
	{
		while (!glfwWindowShouldClose(&window.GetGLFWwindow()))
		{
			glfwPollEvents();
		}
	}
}