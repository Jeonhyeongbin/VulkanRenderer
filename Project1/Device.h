#pragma once
#define VK_USE_PLATFORM_WIN32_KHR

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

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

#include "Window.h"

namespace jhb {
	class Device
	{
		static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData) {
			std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

			return VK_FALSE;
		}

		struct QueueFamilyIndexes {
			std::optional<uint32_t> graphicsFamily;
			std::optional<uint32_t> presentFamily;
		};

		struct SwapChainSupportDetails {
			VkSurfaceCapabilitiesKHR capabilities;
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR> presentModes;
		};

	public:
		Device(Window& window);
		void createInstance();
		bool checkValidationLayerSupport();
		void initVulkan();
		void setupDebugMessenger();
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
		void createSwapChain();
		void createSurface();
		void createLogicalDevice();
		QueueFamilyIndexes findQueueFamilies(VkPhysicalDevice device);
		std::vector<const char*> getRequiredExtensions();
		void pickPhysicalDevice();
		SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
		VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
		VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilites);
		bool isDeviceSuitable(VkPhysicalDevice device);
		bool checkDeviceExtensionSupport(VkPhysicalDevice device);
		void cleanup();

	private:
		Window& window;
		VkInstance instance;
		VkDevice logicalDevice;
		VkSurfaceKHR surface; // window system intergration extension, but you should know that every device in the system supports this not true;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkQueue graphicsQueue; // queues are automatically create with logical device, you must create explictly handle to interface
		VkQueue presentQueue;
		VkSwapchainKHR swapChain;
		VkDebugUtilsMessengerEXT debugMessenger;

		const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
		};

		const std::vector<const char*> deviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		};

#ifdef NDEBUG
		const bool enableValidationLayers = false;
#else
		const bool enableValidationLayers = true;
#endif
	};
}
