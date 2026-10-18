#pragma once

#ifdef ANDROID
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <android/log.h>
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer", __VA_ARGS__)
#else
#define VK_USE_PLATFORM_WIN32_KHR

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <vulkan/vulkan.h>
#endif

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
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

#include "Image.h"
#include "Buffer.h"
#include <string_view>
#include <cstddef>

namespace jhb::rg
{
struct Texture;
struct ImageDesc;
struct Buffer;
struct Pipeline;
struct PipelineDesc;

// RenderGraph talks directly to a Device, like the Rust implementation. The
// renderer's concrete Device owns allocation, pipeline creation and extension
// dispatch details.
class Device
{
  public:
    virtual ~Device() = default;

    [[nodiscard]] virtual VkDevice handle() const noexcept = 0;
    [[nodiscard]] virtual Texture createTexture(const ImageDesc &desc,
                                                std::string_view debugName) = 0;
    [[nodiscard]] virtual Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                              VkMemoryPropertyFlags memoryProperties,
                                              std::string_view debugName) = 0;
    [[nodiscard]] virtual Pipeline createPipeline(const PipelineDesc &desc) = 0;
    virtual void uploadBuffer(Buffer &buffer, const void *data, std::size_t size) = 0;

    virtual void destroyTexture(Texture &texture) noexcept = 0;
    virtual void destroyBuffer(Buffer &buffer) noexcept = 0;
    virtual void destroyPipeline(Pipeline &pipeline) noexcept = 0;


    virtual void beginDebugLabel(VkCommandBuffer commandBuffer, std::string_view name) const = 0;
    virtual void endDebugLabel(VkCommandBuffer commandBuffer) const = 0;

    [[nodiscard]] virtual VkDescriptorPoolCreateFlags descriptorPoolFlags() const noexcept = 0;
};
} // namespace jhb::rg

namespace jhb {
	class Device : public rg::Device
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
			std::optional<uint32_t> computeFamily;
		};

	public:
          Device(Window &window);

          VkDevice handle() const noexcept override
          {
              return logicalDevice;
          }

          rg::Texture createTexture(const rg::ImageDesc &, std::string_view) override;
          rg::Buffer createBuffer(VkDeviceSize, VkBufferUsageFlags, VkMemoryPropertyFlags,
                                  std::string_view) override;
          rg::Pipeline createPipeline(const rg::PipelineDesc &) override;
          void uploadBuffer(rg::Buffer &, const void *, std::size_t) override;
          void destroyTexture(rg::Texture &) noexcept override;
          void destroyBuffer(rg::Buffer &) noexcept override;
          void destroyPipeline(rg::Pipeline &) noexcept override;


          void beginDebugLabel(VkCommandBuffer, std::string_view) const override;
          void endDebugLabel(VkCommandBuffer) const override;
          VkDescriptorPoolCreateFlags descriptorPoolFlags() const noexcept override;
          ~Device();
          VkResult checkBindlessSupport() const;

          // Not copyable or movable
          Device(const Device &) = delete;
          void operator=(const Device &) = delete;
          Device(Device &&) = delete;
          Device &operator=(Device &&) = delete;

          VkInstance getInstance() const { return instance; }
          Window &getWindow() const
          {
              return window; }
		VkDevice getLogicalDevice() const  { return logicalDevice; }
		VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
		VkSurfaceKHR getSurface() const { return surface; }
		VkQueue getGraphicsQueue() const { return graphicsQueue; }
		VkQueue getComputeQueue() const { return ComputeQueue;	}
		VkQueue getPresentQueue() const { return presentQueue; }
		VkCommandPool getCommnadPool() { return commandPool; }

		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		void createInstance();
		void createImageWithInfo(
			const VkImageCreateInfo& imageInfo,
			VkMemoryPropertyFlags properties,
			VkImage& image,
			VkDeviceMemory& imageMemory);

		VkSampleCountFlagBits getMaxUsableSampleCount();
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

		// Buffer helper methods
		void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
		void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange subresourceRange);
		void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldLayout, VkImageLayout newLayout);
		void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, int count);
		void copyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, int count);
		void copyImageToBuffer(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

		VkCommandBuffer beginSingleTimeCommands();
		void endSingleTimeCommands(VkCommandBuffer commandBuffer);

		// computeCommand
		VkCommandBuffer beginSingleComputeCommands();
		void endSingleComputeCommands(VkCommandBuffer commandBuffer);


		QueueFamilyIndexes findQueueFamilies(VkPhysicalDevice device);
		std::vector<const char*> getRequiredExtensions();
		void pickPhysicalDevice();

		VkFormat findSupportedFormat(
			const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

		bool isDeviceSuitable(VkPhysicalDevice device);
		bool checkDeviceExtensionSupport(VkPhysicalDevice device);
		void cleanup();

	public:
		VkPhysicalDeviceProperties properties;

	private:
		Window& window;
		VkInstance instance{VK_NULL_HANDLE};
		VkDevice logicalDevice{VK_NULL_HANDLE};
		VkSurfaceKHR surface; // window system intergration extension, but you should know that every device in the system supports this not true;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkQueue graphicsQueue; // queues are automatically create with logical device, you must create explictly handle to interface
		VkQueue presentQueue;
		VkQueue ComputeQueue;
		VkSwapchainKHR swapChain;
		VkDebugUtilsMessengerEXT debugMessenger{VK_NULL_HANDLE};
		VkCommandPool commandPool{VK_NULL_HANDLE};

		const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
		};

		const std::vector<const char*> deviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		};

#if defined(NDEBUG) || defined(ANDROID)
		const bool enableValidationLayers = false;
#else
		const bool enableValidationLayers = true;
#endif
		public:
			VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_8_BIT;
			VkPhysicalDeviceFeatures features;
	};
}
