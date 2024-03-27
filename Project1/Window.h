#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include <glm/glm.hpp>


namespace jhb {
	class GameObject;
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
		void SetMouseButtonPress(bool _isMouseButtonPressed)
		{
			isMousePressed = _isMouseButtonPressed;
		}
		void SetMouseCursorPose(float x, float y)
		{
			prevPos = { x,y };
		}

		VkExtent2D getExtent() { return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) }; }
		bool wasWindowResized() { return framebufferResized; }
		void resetWindowResizedFlag(){ framebufferResized = false; }
		void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
		void mouseMove(float x, float y, float dt, GameObject& camera);
	private:
		static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
		void initWindow();

		int width;
		int height;

		bool framebufferResized = false;
		bool isMousePressed = false;
		
		glm::vec2 prevPos{0.f};
		std::string windowName;
		GLFWwindow* window;
	};
}
