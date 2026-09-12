#include "ImGuiContext.h"
#ifndef ANDROID
#include "Device.h"
#include "Window.h"
#include "External/Imgui/imgui.h"
#include "External/Imgui/imgui_impl_glfw.h"
#include "External/Imgui/imgui_impl_vulkan.h"
#include <stdexcept>

namespace jhb
{
namespace
{
void check(VkResult result)
{
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("ImGui Vulkan operation failed");
    }
}
} // namespace

ImGuiContext::ImGuiContext(Device &device, VkFormat format, uint32_t imageCount)
    : device_(device), format_(format), imageCount_(imageCount)
{
    if (format == VK_FORMAT_UNDEFINED || imageCount < 2 || ImGui::GetCurrentContext())
    {
        throw std::invalid_argument(
            "ImGui requires a color format, >=2 images and its own context");
    }
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    try
    {
        ImGui::StyleColorsDark();
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool.maxSets = 256;
        pool.poolSizeCount = 1;
        pool.pPoolSizes = &size;
        check(vkCreateDescriptorPool(device_.getLogicalDevice(), &pool, nullptr, &pool_));
        glfwReady_ = ImGui_ImplGlfw_InitForVulkan(&device_.getWindow().GetGLFWwindow(), true);
        if (!glfwReady_)
        {
            throw std::runtime_error("ImGui GLFW initialization failed");
        }
        initializeVulkan();
    }
    catch (...)
    {
        cleanup();
        throw;
    }
}

void ImGuiContext::initializeVulkan()
{
    ImGui_ImplVulkan_InitInfo info{};
    info.Instance = device_.getInstance();
    info.PhysicalDevice = device_.getPhysicalDevice();
    info.Device = device_.getLogicalDevice();
    info.QueueFamily = device_.findQueueFamilies(info.PhysicalDevice).graphicsFamily.value();
    info.Queue = device_.getGraphicsQueue();
    info.DescriptorPool = pool_;
    info.MinImageCount = 2;
    info.ImageCount = imageCount_;
    info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    info.UseDynamicRendering = true;
    info.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &format_;
    info.CheckVkResultFn = check;
    vulkanReady_ = ImGui_ImplVulkan_Init(&info);
    if (!vulkanReady_)
    {
        throw std::runtime_error("ImGui Vulkan initialization failed");
    }
    if (!ImGui_ImplVulkan_CreateFontsTexture())
    {
        throw std::runtime_error("ImGui font upload failed");
    }
}

ImGuiContext::~ImGuiContext()
{
    cleanup();
}

void ImGuiContext::cleanup() noexcept
{
    if (!context_)
    {
        return;
    }
    ImGui::SetCurrentContext(context_);
    vkDeviceWaitIdle(device_.getLogicalDevice());
    // Init may throw after ImGui has allocated its renderer data.
    if (ImGui::GetIO().BackendRendererUserData)
    {
        ImGui_ImplVulkan_Shutdown();
    }
    if (glfwReady_)
    {
        ImGui_ImplGlfw_Shutdown();
    }
    if (pool_)
    {
        vkDestroyDescriptorPool(device_.getLogicalDevice(), pool_, nullptr);
    }
    ImGui::DestroyContext(context_);
    context_ = nullptr;
    pool_ = VK_NULL_HANDLE;
    vulkanReady_ = glfwReady_ = false;
}

void ImGuiContext::configureTarget(VkFormat format, uint32_t imageCount)
{
    if (frameOpen_ || format == VK_FORMAT_UNDEFINED || imageCount < 2)
    {
        throw std::invalid_argument("Configure ImGui target outside a frame with >=2 images");
    }
    if (format == format_ && imageCount == imageCount_ && vulkanReady_)
    {
        return;
    }
    ImGui::SetCurrentContext(context_);
    check(vkDeviceWaitIdle(device_.getLogicalDevice()));
    if (ImGui::GetIO().BackendRendererUserData)
    {
        ImGui_ImplVulkan_Shutdown();
    }
    vulkanReady_ = false;
    drawReady_ = false;
    ++generation_;
    format_ = format;
    imageCount_ = imageCount;
    initializeVulkan();
}

void ImGuiContext::beginFrame()
{
    if (frameOpen_ || !vulkanReady_)
    {
        throw std::logic_error("Invalid ImGui frame start");
    }
    ImGui::SetCurrentContext(context_);
    ++generation_;
    drawReady_ = false;
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame(); // Gets logical size and framebuffer scale, including resize/HiDPI.
    ImGui::NewFrame();
    frameOpen_ = true;
}

void ImGuiContext::endFrame()
{
    if (!frameOpen_)
    {
        throw std::logic_error("No open ImGui frame");
    }
    ImGui::SetCurrentContext(context_);
    ImGui::Render();
    frameOpen_ = false;
    drawReady_ = true;
}

rg::RenderFunction ImGuiContext::drawFunction()
{
    if (!drawReady_)
    {
        throw std::logic_error("Finish ImGui frame before declaring its pass");
    }
    return [this, generation = generation_](VkCommandBuffer cmd, const rg::RenderPass &pass,
                                            const rg::GraphResources &resources)
    {
        if (!drawReady_ || generation != generation_ || pass.writes.size() != 1 ||
            resources.texture(pass.writes[0].texture).texture.image.desc.format != format_)
        {
            throw std::logic_error("Stale ImGui draw data or mismatched target format");
        }
        ImGui::SetCurrentContext(context_);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    };
}
} // namespace jhb
#endif
