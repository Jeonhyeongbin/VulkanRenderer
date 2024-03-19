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
	public:
		struct QueueFamilyIndexes {
			std::optional<uint32_t> graphicsFamily;
			std::optional<uint32_t> presentFamily;
		};

	public:
		Device(Window& window);
		~Device();

		// Not copyable or movable
		Device(const Device&) = delete;
		void operator=(const Device&) = delete;
		Device(Device&&) = delete;
		Device& operator=(Device&&) = delete;

		Window& getWindow() const { return window; }
		VkDevice getLogicalDevice() const  { return logicalDevice; }
		VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
		VkSurfaceKHR getSurface() const { return surface; }
		VkQueue getGraphicsQueue() const { return graphicsQueue; }
		VkQueue getPresentQueue() const { return presentQueue; }
		VkCommandPool getCommnadPool() { return commandPool; }

		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		void createInstance();
		void createImageWithInfo(
			const VkImageCreateInfo& imageInfo,
			VkMemoryPropertyFlags properties,
			VkImage& image,
			VkDeviceMemory& imageMemory);

		bool checkValidationLayerSupport();
		void initVulkan();
		void setupDebugMessenger();
		VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
		void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
		void createSurface();
		void createLogicalDevice();
		void createCommandPool();
		void createBuffer(VkDeviceSize size,
			VkBufferUsageFlags usage,
			VkMemoryPropertyFlags properties,
			VkBuffer& buffer,
			VkDeviceMemory& bufferMemory);

		QueueFamilyIndexes findQueueFamilies(VkPhysicalDevice device);
		std::vector<const char*> getRequiredExtensions();
		void pickPhysicalDevice();

		VkFormat findSupportedFormat(
			const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

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
		VkCommandPool commandPool;

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
