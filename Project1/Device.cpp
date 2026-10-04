#include "Device.h"
#include "ShaderReflection.h"
#include "RenderGraph/RenderGraph.h"

namespace jhb
{


void Device::beginDebugLabel(VkCommandBuffer, std::string_view) const
{
}

void Device::endDebugLabel(VkCommandBuffer) const
{
}

VkDescriptorPoolCreateFlags Device::descriptorPoolFlags() const noexcept
{
    return 0;
}
} // namespace jhb

#include <fstream>
#include <cstring>
#include <map>
#ifdef ANDROID
#include "AndroidAssets.h"
#endif

namespace jhb
{
namespace
{
void checked(VkResult result, const char *operation)
{
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error(operation);
    }
}

std::vector<uint32_t> spirv(const std::string &path)
{
    std::vector<uint32_t> words;
#ifdef ANDROID
    AAsset *asset = AAssetManager_open(getAndroidAssetManager(), path.c_str(), AASSET_MODE_BUFFER);
    if (!asset)
    {
        throw std::runtime_error("Missing shader: " + path);
    }
    auto size = AAsset_getLength(asset);
    if (size <= 0 || size % 4)
    {
        AAsset_close(asset);
        throw std::runtime_error("Invalid SPIR-V");
    }
    words.resize(size / 4);
    auto read = AAsset_read(asset, words.data(), size);
    AAsset_close(asset);
    if (read != size)
    {
        throw std::runtime_error("Shader read failed");
    }
#else
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        throw std::runtime_error("Missing shader: " + path);
    }
    auto size = file.tellg();
    if (size <= 0 || static_cast<size_t>(size) % 4)
    {
        throw std::runtime_error("Invalid SPIR-V");
    }
    words.resize(static_cast<size_t>(size) / 4);
    file.seekg(0);
    if (!file.read(reinterpret_cast<char *>(words.data()), size))
    {
        throw std::runtime_error("Shader read failed");
    }
#endif
    return words;
}
} // namespace

rg::Texture Device::createTexture(const rg::ImageDesc &d, std::string_view name)
{
    if (!d.width || !d.height || !d.arrayLayers || !d.mipLevels || d.depth != 1)
    {
        throw std::invalid_argument("Graph textures currently support 2D/array/cube images");
    }
    if (d.imageType == rg::ImageType::CubeMap && (d.arrayLayers != 6 || d.width != d.height))
    {
        throw std::invalid_argument("Cube texture must be square with six layers");
    }
    rg::Texture t;
    t.image.desc = d;
    t.image.debugName = name;
    try
    {
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ci.flags = d.imageType == rg::ImageType::CubeMap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
        ci.imageType = VK_IMAGE_TYPE_2D;
        ci.format = d.format;
        ci.extent = {d.width, d.height, 1};
        ci.mipLevels = d.mipLevels;
        ci.arrayLayers = d.arrayLayers;
        ci.samples = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling = VK_IMAGE_TILING_OPTIMAL;
        ci.usage = d.usage;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        checked(vkCreateImage(handle(), &ci, nullptr, &t.image.image), "Create graph image");
        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(handle(), t.image.image, &req);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex =
            findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        checked(vkAllocateMemory(handle(), &alloc, nullptr, &t.image.memory),
                "Allocate graph image");
        checked(vkBindImageMemory(handle(), t.image.image, t.image.memory, 0), "Bind graph image");
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = t.image.image;
        vi.format = d.format;
        vi.viewType = d.imageType == rg::ImageType::CubeMap ? VK_IMAGE_VIEW_TYPE_CUBE
                      : d.arrayLayers > 1                   ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                                                            : VK_IMAGE_VIEW_TYPE_2D;
        vi.subresourceRange = {d.aspectFlags, 0, d.mipLevels, 0, d.arrayLayers};
        checked(vkCreateImageView(handle(), &vi, nullptr, &t.image.imageView), "Create graph view");
        // Attachment views always select mip zero and one layer.
        for (uint32_t layer = 0; layer < d.arrayLayers; ++layer)
        {
            vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vi.subresourceRange = {d.aspectFlags, 0, 1, layer, 1};
            VkImageView view{};
            checked(vkCreateImageView(handle(), &vi, nullptr, &view), "Create layer view");
            t.image.layerViews.push_back(view);
        }
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        si.magFilter = si.minFilter = d.filter;
        si.mipmapMode = d.mipmapMode;
        si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.maxLod = float(d.mipLevels);
        checked(vkCreateSampler(handle(), &si, nullptr, &t.sampler), "Create graph sampler");
        t.descriptorInfo = {t.sampler, t.image.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        return t;
    }
    catch (...)
    {
        destroyTexture(t);
        throw;
    }
}

rg::Buffer Device::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                VkMemoryPropertyFlags properties, std::string_view name)
{
    rg::Buffer b;
    b.size = size;
    b.debugName = name;
    try
    {
        VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        ci.size = size;
        ci.usage = usage;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        checked(vkCreateBuffer(handle(), &ci, nullptr, &b.buffer), "Create graph buffer");
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(handle(), b.buffer, &req);
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, properties);
        checked(vkAllocateMemory(handle(), &ai, nullptr, &b.memory), "Allocate graph buffer");
        checked(vkBindBufferMemory(handle(), b.buffer, b.memory, 0), "Bind graph buffer");
        if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        {
            if (!(properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            {
                throw std::invalid_argument("Graph upload requires coherent host memory");
            }
            checked(vkMapMemory(handle(), b.memory, 0, size, 0, &b.mapped), "Map graph buffer");
        }
        return b;
    }
    catch (...)
    {
        destroyBuffer(b);
        throw;
    }
}

void Device::uploadBuffer(rg::Buffer &b, const void *data, std::size_t size)
{
    if (!b.mapped || size > b.size)
    {
        throw std::invalid_argument("Invalid graph buffer upload");
    }
    std::memcpy(b.mapped, data, size);
}

void Device::destroyBuffer(rg::Buffer &b) noexcept
{
    if (b.mapped)
    {
        vkUnmapMemory(handle(), b.memory);
    }
    vkDestroyBuffer(handle(), b.buffer, nullptr);
    vkFreeMemory(handle(), b.memory, nullptr);
    b = {};
}

void Device::destroyTexture(rg::Texture &t) noexcept
{
    for (auto view : t.image.layerViews)
    {
        vkDestroyImageView(handle(), view, nullptr);
    }
    vkDestroySampler(handle(), t.sampler, nullptr);
    vkDestroyImageView(handle(), t.image.imageView, nullptr);
    vkDestroyImage(handle(), t.image.image, nullptr);
    vkFreeMemory(handle(), t.image.memory, nullptr);
    t = {};
}

void Device::destroyPipeline(rg::Pipeline &p) noexcept
{
    vkDestroyPipeline(handle(), p.handle, nullptr);
    vkDestroyPipelineLayout(handle(), p.layout, nullptr);
    for (auto layout : p.descriptorSetLayouts)
    {
        vkDestroyDescriptorSetLayout(handle(), layout, nullptr);
    }
    p = {};
}

rg::Pipeline Device::createPipeline(const rg::PipelineDesc &description)
{
    auto d = description;
    if (d.reflectShaders)
    {
        rg::reflectPipelineInterface(d);
    }
    rg::Pipeline p;
    p.desc = d;
    p.type = d.type;
    std::vector<VkShaderModule> modules;
    try
    {
        uint32_t sets = 0;
        for (const auto &b : d.bindings)
        {
            sets = (std::max)(sets, b.set + 1);
        }
        for (uint32_t set = 0; set < sets; ++set)
        {
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            for (const auto &b : d.bindings)
            {
                if (b.set == set)
                {
                    bindings.push_back({b.binding, b.type, b.count, b.stages, nullptr});
                    if (!b.name.empty())
                    {
                        p.namedBindings[b.name] = {b.set, b.binding, b.type};
                    }
                }
            }
            VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            ci.bindingCount = static_cast<uint32_t>(bindings.size());
            ci.pBindings = bindings.data();
            VkDescriptorSetLayout layout{};
            checked(vkCreateDescriptorSetLayout(handle(), &ci, nullptr, &layout),
                    "Create graph set layout");
            p.descriptorSetLayouts.push_back(layout);
        }
        VkPushConstantRange push{d.pushConstantStages, 0, d.pushConstantSize};
        VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        li.setLayoutCount = static_cast<uint32_t>(p.descriptorSetLayouts.size());
        li.pSetLayouts = p.descriptorSetLayouts.data();
        li.pushConstantRangeCount = d.pushConstantSize ? 1 : 0;
        li.pPushConstantRanges = &push;
        checked(vkCreatePipelineLayout(handle(), &li, nullptr, &p.layout),
                "Create graph pipeline layout");
        auto stage = [&](const std::string &path, VkShaderStageFlagBits flag)
        {
            auto words = spirv(path);
            VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            mi.codeSize = words.size() * 4;
            mi.pCode = words.data();
            VkShaderModule module{};
            checked(vkCreateShaderModule(handle(), &mi, nullptr, &module), "Create shader module");
            modules.push_back(module);
            VkPipelineShaderStageCreateInfo si{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            si.stage = flag;
            si.module = module;
            si.pName = "main";
            return si;
        };
        if (d.type == rg::PipelineType::Compute)
        {
            VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            ci.stage = stage(d.computePath, VK_SHADER_STAGE_COMPUTE_BIT);
            ci.layout = p.layout;
            checked(vkCreateComputePipelines(handle(), VK_NULL_HANDLE, 1, &ci, nullptr, &p.handle),
                    "Create compute pipeline");
        }
        else
        {
            std::vector<VkPipelineShaderStageCreateInfo> stages{
                stage(d.vertexPath, VK_SHADER_STAGE_VERTEX_BIT)};
            if (!d.fragmentPath.empty())
            {
                stages.push_back(stage(d.fragmentPath, VK_SHADER_STAGE_FRAGMENT_BIT));
            }
            VkPipelineVertexInputStateCreateInfo vertex{
                VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            vertex.vertexBindingDescriptionCount = static_cast<uint32_t>(d.vertexBindings.size());
            vertex.pVertexBindingDescriptions = d.vertexBindings.data();
            vertex.vertexAttributeDescriptionCount =
                static_cast<uint32_t>(d.vertexAttributes.size());
            vertex.pVertexAttributeDescriptions = d.vertexAttributes.data();
            VkPipelineInputAssemblyStateCreateInfo assembly{
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo viewport{
                VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            viewport.viewportCount = viewport.scissorCount = 1;
            VkPipelineRasterizationStateCreateInfo raster{
                VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            raster.lineWidth = 1;
            raster.cullMode = d.cullMode;
            raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            VkPipelineMultisampleStateCreateInfo ms{
                VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            VkPipelineDepthStencilStateCreateInfo depth{
                VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
            depth.depthTestEnable = d.depthTest;
            depth.depthWriteEnable = d.depthWrite;
            depth.depthCompareOp = d.depthCompare;
            std::vector<VkPipelineColorBlendAttachmentState> attachments(
                d.colorAttachmentFormats.size());
            for (auto &a : attachments)
            {
                a.colorWriteMask = 0xf;
            }
            VkPipelineColorBlendStateCreateInfo blend{
                VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            blend.attachmentCount = static_cast<uint32_t>(attachments.size());
            blend.pAttachments = attachments.data();
            VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamic{
                VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            dynamic.dynamicStateCount = 2;
            dynamic.pDynamicStates = dynamicStates;
            VkPipelineRenderingCreateInfo rendering{
                VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
            rendering.colorAttachmentCount = static_cast<uint32_t>(d.colorAttachmentFormats.size());
            rendering.pColorAttachmentFormats = d.colorAttachmentFormats.data();
            rendering.depthAttachmentFormat = d.depthStencilAttachmentFormat;
            VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            ci.pNext = &rendering;
            ci.stageCount = static_cast<uint32_t>(stages.size());
            ci.pStages = stages.data();
            ci.pVertexInputState = &vertex;
            ci.pInputAssemblyState = &assembly;
            ci.pViewportState = &viewport;
            ci.pRasterizationState = &raster;
            ci.pMultisampleState = &ms;
            ci.pDepthStencilState = &depth;
            ci.pColorBlendState = &blend;
            ci.pDynamicState = &dynamic;
            ci.layout = p.layout;
            checked(vkCreateGraphicsPipelines(handle(), VK_NULL_HANDLE, 1, &ci, nullptr, &p.handle),
                    "Create graphics pipeline");
        }
        for (auto m : modules)
        {
            vkDestroyShaderModule(handle(), m, nullptr);
        }
        return p;
    }
    catch (...)
    {
        for (auto m : modules)
        {
            vkDestroyShaderModule(handle(), m, nullptr);
        }
        destroyPipeline(p);
        throw;
    }
}
} // namespace jhb

#include "SwapChain.h"
#ifdef ANDROID
#include <android/log.h>
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer", __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "VulkanRenderer", __VA_ARGS__)
#else
#define LOGE(...)
#define LOGI(...)
#endif


namespace jhb {
	Device::Device(Window& window) : window(window)
	{
		initVulkan();

	}

	Device::~Device()
	{
		cleanup();
	}
	

	uint32_t Device::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) &&
				(memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

	void Device::createInstance() {
		// using for optimizatgion my applciation (optional)
		if (enableValidationLayers && !checkValidationLayerSupport())
		{
			throw std::runtime_error("validation layers not available!");
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "HelloTriangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_3;

#ifdef ANDROID
		// 기기가 지원하는 instance extension 목록 출력 (디버깅)
		{
			uint32_t availableExtCount = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &availableExtCount, nullptr);
			std::vector<VkExtensionProperties> availableExts(availableExtCount);
			vkEnumerateInstanceExtensionProperties(nullptr, &availableExtCount, availableExts.data());
			LOGI("Available instance extensions (%u):", availableExtCount);
			for (auto& e : availableExts) LOGI("  %s", e.extensionName);
		}

		std::vector<const char*> extensions = {
			VK_KHR_SURFACE_EXTENSION_NAME,
			VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
		};
#else
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
#endif

		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();

		// NO more distinction between instance and device
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
			debugCreateInfo = {};
			debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			debugCreateInfo.pfnUserCallback = debugCallback;
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else {
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		VkResult instanceResult = vkCreateInstance(&createInfo, nullptr, &instance);
		if (instanceResult != VK_SUCCESS)
		{
			LOGE("vkCreateInstance failed: VkResult = %d", (int)instanceResult);
			throw std::runtime_error("failed to create instance!!");
		}
	}

	void Device::createImageWithInfo(const VkImageCreateInfo& imageInfo, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
	{
		if (vkCreateImage(logicalDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(logicalDevice, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		if (vkBindImageMemory(logicalDevice, image, imageMemory, 0) != VK_SUCCESS) {
			throw std::runtime_error("failed to bind image memory!");
		}
	}

	VkSampleCountFlagBits Device::getMaxUsableSampleCount()
	{
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

		VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
		if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
		if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
		if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
		if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
		if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
		if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

		return VK_SAMPLE_COUNT_1_BIT;
	}

	bool Device::checkValidationLayerSupport()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (auto layerName : validationLayers)
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}

	void Device::initVulkan() {
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		vkGetPhysicalDeviceFeatures(physicalDevice, &features);
		createLogicalDevice();
		createCommandPool();
	}

	void Device::setupDebugMessenger() {
		if (!enableValidationLayers) return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;

		if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
			throw std::runtime_error("failed to set up debug messenger!");
		}
	}

	VkResult Device::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void Device::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, pAllocator);
		}
	}

	void Device::createSurface()
	{
#ifdef ANDROID
		VkAndroidSurfaceCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
		createInfo.window = window.GetNativeWindow();

		if (vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create Android window surface!");
		}
#else
		VkWin32SurfaceCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.hwnd = glfwGetWin32Window(&window.GetGLFWwindow());
		createInfo.hinstance = GetModuleHandle(nullptr); //get current process handle.

		if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create window surface!");
		}
#endif
	}

	void Device::createLogicalDevice()
	{
		QueueFamilyIndexes familyindexs = findQueueFamilies(physicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<std::optional<uint32_t>> uniqueQueueFamiliesIndexs = { familyindexs.graphicsFamily, familyindexs.presentFamily };
		float priority = 1.0;

		for (auto queueFamilyIndex : uniqueQueueFamiliesIndexs)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamilyIndex.value();
			queueCreateInfo.queueCount = 1; // number of queues for single queue family
			queueCreateInfo.pQueuePriorities = &priority; // floating point between 0.0 ~ 1.0

			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        if (properties.apiVersion < VK_API_VERSION_1_3)
            throw std::runtime_error("RenderGraph requires Vulkan 1.3");
        VkPhysicalDeviceDynamicRenderingFeatures dynamic{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
        VkPhysicalDeviceFeatures2 supported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        supported.pNext = &dynamic;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &supported);
        if (!dynamic.dynamicRendering) throw std::runtime_error("Device lacks dynamic rendering");
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.independentBlend = supported.features.independentBlend;
        dynamic.dynamicRendering = VK_TRUE;

		// Create the logical device
		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &dynamic;

		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

		if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create logical device!");
		}

		vkGetDeviceQueue(logicalDevice, familyindexs.graphicsFamily.value(), 0, &graphicsQueue);
		vkGetDeviceQueue(logicalDevice, familyindexs.presentFamily.value(), 0, &presentQueue);
	}

	void Device::createCommandPool()
	{
		QueueFamilyIndexes queueFamilyIndices = findQueueFamilies(physicalDevice);

		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
		poolInfo.flags =
			VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create command pool!");
		}
	}

	void Device::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
	{
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to create vertex buffer!");
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate vertex buffer memory!");
		}

		vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);
	}

	void Device::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;  // Optional
		copyRegion.dstOffset = 0;  // Optional
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		endSingleTimeCommands(commandBuffer);
	}

	void Device::transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkImageSubresourceRange subresourceRange)
	{
		VkImageMemoryBarrier imageMemoryBarrier{};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.oldLayout = oldImageLayout;
		imageMemoryBarrier.newLayout = newImageLayout;

		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		imageMemoryBarrier.image = image;
		imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageMemoryBarrier.subresourceRange = subresourceRange;

		VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;


		if (oldImageLayout == VK_IMAGE_LAYOUT_UNDEFINED && newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			imageMemoryBarrier.srcAccessMask = 0;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else {
			switch (oldImageLayout)
			{
			case VK_IMAGE_LAYOUT_UNDEFINED:
				// Image layout is undefined (or does not matter)
				// Only valid as initial layout
				// No flags required, listed only for completeness
				imageMemoryBarrier.srcAccessMask = 0;
				break;

			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				// Image is preinitialized
				// Only valid as initial layout for linear images, preserves memory contents
				// Make sure host writes have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image is a color attachment
				// Make sure any writes to the color buffer have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image is a depth/stencil attachment
				// Make sure any writes to the depth/stencil buffer have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image is a transfer source
				// Make sure any reads from the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image is a transfer destination
				// Make sure any writes to the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image is read by a shader
				// Make sure any shader reads from the image have been finished
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
			}

			// Target layouts (new)
			// Destination access mask controls the dependency for the new image layout
			switch (newImageLayout)
			{
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image will be used as a transfer destination
				// Make sure any writes to the image have been finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image will be used as a transfer source
				// Make sure any reads from the image have been finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image will be used as a color attachment
				// Make sure any writes to the color buffer have been finished
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image layout will be used as a depth/stencil attachment
				// Make sure any writes to depth/stencil buffer have been finished
				imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image will be read in a shader (sampler, input attachment)
				// Make sure any writes to the image have been finished
				if (imageMemoryBarrier.srcAccessMask == 0)
				{
					imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
				}
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				break;
			default:
				// Other source layouts aren't handled (yet)
				break;
			}
		}
		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage /* TODO */, destinationStage /* TODO */,
			0,
			0, nullptr,
			0, nullptr,
			1, &imageMemoryBarrier
		);
	}

	void Device::transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = aspectMask;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;
		transitionImageLayout(cmd, image, oldLayout, newLayout, subresourceRange);
	}

	void Device::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, int count)
	{
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = count;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			width,
			height,
			1
		};

		vkCmdCopyBufferToImage(
			commandBuffer,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		endSingleTimeCommands(commandBuffer);
	}

	void Device::copyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, int count)
	{
		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = count;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			width,
			height,
			1
		};

		vkCmdCopyBufferToImage(
			cmd,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);
	}

	void Device::copyImageToBuffer(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
	{
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			width,
			height,
			1
		};

		vkCmdCopyImageToBuffer(
			commandBuffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			buffer,
			1,
			&region
		);

		endSingleTimeCommands(commandBuffer);
	}

	VkCommandBuffer Device::beginSingleTimeCommands()
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);
		return commandBuffer;
	}

	void Device::endSingleTimeCommands(VkCommandBuffer commandBuffer)
	{
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("end commandbuffer failed!!");
		}

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
		{
			throw std::runtime_error("queue submit failed!!");
		}

		if (vkQueueWaitIdle(graphicsQueue) != VK_SUCCESS)
		{
			throw std::runtime_error("queue wait idel failed!!");
		}

		vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
	}

	VkCommandBuffer Device::beginSingleComputeCommands()
	{
		return VkCommandBuffer();
	}

	void Device::endSingleComputeCommands(VkCommandBuffer commandBuffer)
	{
		//// Wait for fence to ensure that compute buffer writes have finished
		//vkWaitForFences(logicalDevice, 1, &F, VK_TRUE, UINT64_MAX);
		//vkResetFences(logicalDevice, 1, &compute.fence);

		//VkSubmitInfo computeSubmitInfo = vks::initializers::submitInfo();
		//computeSubmitInfo.commandBufferCount = 1;
		//computeSubmitInfo.pCommandBuffers = &compute.commandBuffer;
		//computeSubmitInfo.signalSemaphoreCount = 1;
		//computeSubmitInfo.pSignalSemaphores = &compute.semaphore;
	}

	Device::QueueFamilyIndexes Device::findQueueFamilies(VkPhysicalDevice device) {
		QueueFamilyIndexes QueueFamilyIndexes;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if (QueueFamilyIndexes.graphicsFamily.has_value() && QueueFamilyIndexes.presentFamily.has_value() && QueueFamilyIndexes.computeFamily.has_value())
			{
				break;
			}

			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				QueueFamilyIndexes.graphicsFamily = i;
			}

			// only compute queue family
			if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) && ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)==0))
			{
				QueueFamilyIndexes.computeFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			if (presentSupport)
			{
				QueueFamilyIndexes.presentFamily = i;
			}

			i++;
		}

		// ���� Compute queue family���� ���ٸ� graphic queue�� compute queue �� ���
		for (const auto& queueFamily : queueFamilies) {
			if (!QueueFamilyIndexes.computeFamily.has_value())
			{
				QueueFamilyIndexes.computeFamily = QueueFamilyIndexes.graphicsFamily;
				break;
			}
		}

		return QueueFamilyIndexes;
	}

	std::vector<const char*> Device::getRequiredExtensions()
	{
#ifdef ANDROID
		std::vector<const char*> extensions = {
			VK_KHR_SURFACE_EXTENSION_NAME,
			VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
		};
#else
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
#endif

		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	void Device::pickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
		for (const auto& device : devices)
		{
			if (isDeviceSuitable(device))
			{
				physicalDevice = device;
#ifdef ANDROID
				// Android: 발열 감소를 위해 MSAA를 4x 이하로 제한한다.
				// getMaxUsableSampleCount()는 8x~64x를 반환할 수 있어 GPU 부하가 크다.
				{
					VkSampleCountFlagBits maxSupported = getMaxUsableSampleCount();
					if (maxSupported >= VK_SAMPLE_COUNT_4_BIT)
						msaaSamples = VK_SAMPLE_COUNT_4_BIT;
					else
						msaaSamples = maxSupported;
				}
#else
				msaaSamples = getMaxUsableSampleCount();
#endif
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}
	}

	VkFormat Device::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		for (VkFormat format : candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
				return format;
			}
			else if (
				tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}
		throw std::runtime_error("failed to find supported format!");
	}

	bool Device::isDeviceSuitable(VkPhysicalDevice device)
	{
		QueueFamilyIndexes indexes = findQueueFamilies(device);

		bool extensionsSupported = checkDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChain::SwapChainSupportDetails swapChainSupport = SwapChain::querySwapChainSupport(device, surface);
			swapChainAdequate = swapChainSupport.result == VK_SUCCESS &&
				!swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indexes.graphicsFamily.has_value() && indexes.presentFamily.has_value() && extensionsSupported && swapChainAdequate;
	}

	bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	void Device::cleanup() {
		if (logicalDevice) {
			vkDeviceWaitIdle(logicalDevice);
			if (commandPool) vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
			vkDestroyDevice(logicalDevice, nullptr);
			logicalDevice = VK_NULL_HANDLE;
			commandPool = VK_NULL_HANDLE;
		}
		if (instance) {
			if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
			if (debugMessenger) DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
			vkDestroyInstance(instance, nullptr);
			instance = VK_NULL_HANDLE;
		}
	}
}
