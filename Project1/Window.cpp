#include "GameObject.h"
#include "Window.h"
#include "InputController.h"
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

void jhb::Window::mouseMove(float x, float y, float dt, GameObject& camera)
{
	if (!isMousePressed)
		return;

	auto pos = glm::vec2(x, y);
	auto deltaPose = pos - prevPos;
	glm::normalize(deltaPose);

	const float cameraRotSpeed = 0.8f;
	camera.transform.rotation.x -= deltaPose.y * dt * cameraRotSpeed;
	camera.transform.rotation.y += deltaPose.x * dt * cameraRotSpeed;

	// for not upside down
	camera.transform.rotation.x = glm::clamp(camera.transform.rotation.x, -1.5f, 1.5f);
	camera.transform.rotation.y = glm::mod(camera.transform.rotation.y, glm::two_pi<float>());
	prevPos = pos;
}