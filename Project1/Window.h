#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

namespace jhb {
	class Window
	{
	public:
		Window(int w, int h, const std::string name);
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		// void creatWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
		GLFWwindow& GetGLFWwindow()
		{
			return *window;
		}
		VkExtent2D getExtent() { return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) }; }

	private:
		void initWindow();
		const int width;
		const int height;

		std::string windowName;
		GLFWwindow* window;
	};
}
