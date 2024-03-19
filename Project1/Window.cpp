#include "Window.h"
#include <stdexcept>

jhb::Window::Window(int w, int h, const std::string name) : width{ w }, height{ h }, windowName{ name }
{
	initWindow();
}

jhb::Window::~Window()
{
	glfwDestroyWindow(window);
	glfwTerminate();
}

//void jhb::Window::creatWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
//{
//	if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
//	{
//		throw std::runtime_error("failed to create window surface");
//	}
//}

void jhb::Window::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
{
}

void jhb::Window::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto windowPtr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	windowPtr->framebufferResized = true;
	windowPtr->width = width;
	windowPtr->height = height;
}

void jhb::Window::initWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);

	// register framebufferResizeCallback, so whenever window resized this callback invoked 
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}