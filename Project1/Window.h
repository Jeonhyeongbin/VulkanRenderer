#pragma once

#ifdef ANDROID
#include <android/native_window.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#else
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#include <string>
#include <glm/glm.hpp>
#include "Camera.h"
#include <memory>


namespace jhb {
	class GameObject;
	class Window
	{
	public:
		enum WindowState {
			Idle,
			ObjectPicking,
			CameraControl,
		};


	public:
		Window(int w, int h, const std::string name);
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

#ifdef ANDROID
		ANativeWindow* GetNativeWindow() { return window; }
		void SetNativeWindow(ANativeWindow* nativeWindow)
		{
			window = nativeWindow;
			if (window) {
				width = ANativeWindow_getWidth(window);
				height = ANativeWindow_getHeight(window);
				framebufferResized = true;
			}
		}

		// JHBApplication 멤버 초기화 순서(Window → Device) 때문에
		// Device 생성 전에 ANativeWindow를 Window에 전달하기 위한 static 메커니즘
		static void SetPendingNativeWindow(ANativeWindow* nativeWindow)
		{
			s_pendingNativeWindow = nativeWindow;
		}
	private:
		static ANativeWindow* s_pendingNativeWindow;
	public:
#else
		GLFWwindow& GetGLFWwindow() { return *window; }
#endif

		void SetMouseButtonPress(bool _isMouseButtonPressed)
		{
			isMousePressed = _isMouseButtonPressed;
		}
		void SetMouseCursorPose(float x, float y)
		{
			prevPos = { x,y };
		}

		bool GetMousePressed()
		{
			return isMousePressed;
		}

		VkExtent2D getExtent() { return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) }; }
		bool wasWindowResized() { return framebufferResized; }
		void resetWindowResizedFlag(){ framebufferResized = false; }
		void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
		void mouseMove(float x, float y, float dt, GameObject& camera);
		Camera* getCamera() { return camera.get(); }
	private:
#ifndef ANDROID
		static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
		static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
		void initWindow();
#endif

		int width;
		int height;

		bool framebufferResized = false;
		bool isMousePressed = false;

		glm::vec2 prevPos{0.f};
		std::string windowName;
#ifdef ANDROID
		ANativeWindow* window = nullptr;
#else
		GLFWwindow* window;
#endif
		std::unique_ptr<jhb::Camera> camera;
	};
}
