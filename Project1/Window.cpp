#include "GameObject.h"
#include "Window.h"
#include "InputController.h"
#ifndef ANDROID
#include "External/Imgui/imgui_impl_glfw.h"
#endif
#include <stdexcept>

#ifdef ANDROID
// JHBApplication 생성 전에 Main.cpp에서 미리 설정해두는 native window
ANativeWindow* jhb::Window::s_pendingNativeWindow = nullptr;
#endif

jhb::Window::Window(int w, int h, const std::string name) : width{ w }, height{ h }, windowName{ name }
{
#ifdef ANDROID
	// 생성자 호출 시점에 pending window가 있으면 즉시 적용
	// (Device 멤버가 이 Window 이후에 초기화되므로 createSurface()가 올바른 포인터를 받게 됨)
	if (s_pendingNativeWindow) {
		SetNativeWindow(s_pendingNativeWindow);
		s_pendingNativeWindow = nullptr;
	}
#else
	initWindow();
#endif
	camera = std::make_unique<jhb::Camera>(0.8f);
}

jhb::Window::~Window()
{
#ifdef ANDROID
	if (window) {
		ANativeWindow_release(window);
		window = nullptr;
	}
#else
	glfwDestroyWindow(window);
	glfwTerminate();
#endif
}

void jhb::Window::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
{
}

#ifndef ANDROID
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
#endif

void jhb::Window::mouseMove(float x, float y, float dt, GameObject& camera)
{
	if (!isMousePressed)
		return;

	auto pos = glm::vec2(x, y);
	auto deltaPose = pos - prevPos;

	const float cameraRotSpeed = 0.02f;
	camera.transform.rotation.x -= deltaPose.y * cameraRotSpeed;
	camera.transform.rotation.y -= deltaPose.x * cameraRotSpeed;

	// InputController::move() limits the angles after all rotation input is applied.
	prevPos = pos;
}
