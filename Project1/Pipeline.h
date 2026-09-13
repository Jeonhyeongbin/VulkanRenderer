#pragma once
#include <vulkan/vulkan.h>
#include "DescriptorSet.h"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>

#include <string>
#include <vector>

#ifdef ANDROID
#include <android/asset_manager.h>
#endif

namespace jhb
{
class Device;
struct Material;

struct PipelineConfigInfo
{
    PipelineConfigInfo() = default;
    PipelineConfigInfo(const PipelineConfigInfo &) = delete;
    PipelineConfigInfo &operator=(const PipelineConfigInfo &) = delete;

    std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
    VkPipelineViewportStateCreateInfo viewportInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    VkPipelineRasterizationStateCreateInfo rasterizationInfo;
    VkPipelineMultisampleStateCreateInfo multisampleInfo;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo colorBlendInfo;
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
    std::vector<VkDynamicState> dynamicStateEnables;
    VkPipelineDynamicStateCreateInfo dynamicStateInfo;
    VkPipelineLayout pipelineLayout = nullptr;
    VkRenderPass renderPass = nullptr;
    uint32_t subpass = 0;
};

class Pipeline
{
  public:
    Pipeline() = default;
    Pipeline(Device &device, const std::string &vertFilepath, const std::string &fragFilepath,
             PipelineConfigInfo &configInfo);
    Pipeline(Device &device, const std::string &vertFilepath, const std::string &fragFilepath,
             PipelineConfigInfo &configInfo, class Material &materials);
    ~Pipeline();

    Pipeline(const Pipeline &) = delete;
    Pipeline &operator=(const Pipeline &) = delete;

    void bind(VkCommandBuffer buffer);
    static void defaultPipelineConfigInfo(PipelineConfigInfo &info);

    VkPipeline &getPipeline()
    {
        return graphicsPipeline;
    }

  public:
    static std::vector<char> readFile(const std::string &filepath);
#ifdef ANDROID
    static void setAssetManager(AAssetManager *mgr);
#endif

  private:
    void createGraphicsPipeline(const std::string &vertFilepath, const std::string &fragFilepath,
                                PipelineConfigInfo &configInfo);
    void createGraphicsPipelinePerMaterial(const std::string &vertFilepath,
                                           const std::string &fragFilepath,
                                           PipelineConfigInfo &configInfo, Material &materials);

    void createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule);

    Device &device;
    VkPipeline graphicsPipeline;
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
};
} // namespace jhb

namespace jhb::rg
{
enum class PipelineType
{
    Graphics,
    Compute
};

struct PipelineBinding
{
    uint32_t set{0}, binding{0};
    VkDescriptorType type{VK_DESCRIPTOR_TYPE_MAX_ENUM};
    uint32_t count{1};
    VkShaderStageFlags stages{VK_SHADER_STAGE_ALL};
    std::string name;

    bool operator==(const PipelineBinding &b) const
    {
        return set == b.set && binding == b.binding && type == b.type && count == b.count &&
               stages == b.stages && name == b.name;
    }
};

class PipelineDescBuilder;

struct PipelineDesc
{
    std::string vertexPath;
    std::string fragmentPath;
    std::string computePath;
    std::vector<VkVertexInputBindingDescription> vertexBindings;
    std::vector<VkVertexInputAttributeDescription> vertexAttributes;
    std::vector<VkFormat> colorAttachmentFormats;
    VkFormat depthStencilAttachmentFormat{VK_FORMAT_UNDEFINED};
    PipelineType type{PipelineType::Graphics};
    bool depthTest{false}, depthWrite{false};
    VkCompareOp depthCompare{VK_COMPARE_OP_LESS_OR_EQUAL};
    VkCullModeFlags cullMode{VK_CULL_MODE_NONE};
    std::vector<PipelineBinding> bindings;
    uint32_t pushConstantSize{0};
    VkShaderStageFlags pushConstantStages{VK_SHADER_STAGE_VERTEX_BIT};
    bool reflectShaders{false};

    static PipelineDescBuilder builder();

    // Lets a renderer include raster/depth/blend state not represented above in
    // graph cache equality.
    std::size_t stateHash{0};

    [[nodiscard]] bool operator==(const PipelineDesc &other) const;
};

class PipelineDescBuilder
{
  public:
    PipelineDescBuilder()
    {
        desc_.reflectShaders = true;
    }

    PipelineDescBuilder &vertexPath(std::string path)
    {
        desc_.vertexPath = std::move(path);
        return *this;
    }

    PipelineDescBuilder &fragmentPath(std::string path)
    {
        desc_.fragmentPath = std::move(path);
        return *this;
    }

    PipelineDescBuilder &depthTest(bool enabled)
    {
        desc_.depthTest = enabled;
        return *this;
    }

    PipelineDescBuilder &depthWrite(bool enabled)
    {
        desc_.depthWrite = enabled;
        return *this;
    }

    PipelineDescBuilder &defaultVertexInput();
    PipelineDescBuilder &defaultMeshVertexInput();

    operator PipelineDesc() const
    {
        return desc_;
    }

  private:
    PipelineDesc desc_;
};

inline PipelineDescBuilder PipelineDesc::builder()
{
    return {};
}

struct Pipeline
{
    // Layouts belong to this pipeline; the returned set owns only its pool/allocation.
    DescriptorSet allocateDescriptorSet(VkDevice device, uint32_t setIndex,
                                        VkDescriptorPoolCreateFlags flags = 0) const;
    VkPipeline handle{VK_NULL_HANDLE};
    VkPipelineLayout layout{VK_NULL_HANDLE};
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::unordered_map<std::string, DescriptorBinding> namedBindings;
    PipelineDesc desc{};
    PipelineType type{PipelineType::Graphics};
};

} // namespace jhb::rg
