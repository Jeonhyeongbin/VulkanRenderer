#include "GameObject.h"
#include "Window.h"
#include "InputController.h"
#include "External/Imgui/imgui_impl_glfw.h"
#include <stdexcept>

jhb::Window::Window(int w, int h, const std::string name) : width{ w }, height{ h }, windowName{ name }
{
	initWindow();
	camera = std::make_unique<jhb::Camera>(0.8f);
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

void jhb::Window::scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	auto windowPtr = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
	auto camera = windowPtr->camera.get();
	float fov = camera->getfovy();
	const float zoomSpeed = 0.05f;
	float offsetradiance = glm::radians(yoffset);
	fov -= (float)yoffset * zoomSpeed;
	if (fov < 0.1f)
		fov = 0.1f;
	if (fov > 1.5f)
		fov = 1.5f;

	camera->setfovy(fov);
}

void jhb::Window::initWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
	glfwSetScrollCallback(window, scroll_callback);

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

	const float cameraRotSpeed = 0.02f;
	camera.transform.rotation.x += deltaPose.y * cameraRotSpeed;
	camera.transform.rotation.y -= deltaPose.x * cameraRotSpeed;

	// for not upside down
	camera.transform.rotation.x = glm::clamp(camera.transform.rotation.x, -80.f, 80.f);
	camera.transform.rotation.y = glm::mod(camera.transform.rotation.y, glm::two_pi<float>());
	prevPos = pos;
}