#define VK_USE_PLATFORM_WIN32_KHR

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>

#include <vulkan/vulkan.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <set>
#include <optional>

#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp

class HelloTriangleApplication {
public:
	HelloTriangleApplication(uint32_t width, uint32_t height) {
		initWindow(width, height);
		initVulkan();
	}

	void Run()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
		}
	}
};

int main() {
	try {
		HelloTriangleApplication app(800, 600);
		app.Run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}