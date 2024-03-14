#pragma once
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

	private:
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
