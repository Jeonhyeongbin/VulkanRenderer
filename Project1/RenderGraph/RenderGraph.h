#pragma once

#include "../Device.h"
#include "../DescriptorSet.h"
#include "VkSync.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace jhb
{
class Renderer;
}

namespace jhb::rg
{

using TextureId = std::size_t;
using BufferId = std::size_t;
using PipelineId = std::size_t;

constexpr uint32_t kDescriptorSetIndexBindless = 0;
constexpr uint32_t kDescriptorSetIndexView = 1;
constexpr uint32_t kDescriptorSetIndexInputResources = 2;
constexpr std::size_t kMaxUniformSize = 2048;

enum class ImageType
{
    Image2D,
    Image2DArray,
    CubeMap
};

struct ImageDesc
{
    uint32_t width{1};
    uint32_t height{1};
    uint32_t depth{1};
    uint32_t arrayLayers{1};
    VkFormat format{VK_FORMAT_UNDEFINED};
    ImageType imageType{ImageType::Image2D};
    VkImageAspectFlags aspectFlags{VK_IMAGE_ASPECT_COLOR_BIT};
    VkImageUsageFlags usage{0};
    uint32_t mipLevels{1};
};

struct Image
{
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkImage image{VK_NULL_HANDLE};
    VkImageView imageView{VK_NULL_HANDLE};
    std::vector<VkImageView> layerViews;
    ImageDesc desc{};
    std::string debugName;

    [[nodiscard]] VkImageView viewForLayer(uint32_t layer) const;

    [[nodiscard]] VkExtent2D extent2D() const
    {
        return {desc.width, desc.height};
    }
};

struct Texture
{
    Image image;
    VkSampler sampler{VK_NULL_HANDLE};
    VkDescriptorImageInfo descriptorInfo{};
};

struct Buffer
{
    VkDeviceMemory memory{VK_NULL_HANDLE};
    void *mapped{nullptr};
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceSize size{0};
    std::string debugName;
};

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

struct DescriptorBinding
{
    uint32_t set{0};
    uint32_t binding{0};
    VkDescriptorType type{VK_DESCRIPTOR_TYPE_MAX_ENUM};
};

struct Pipeline
{
    VkPipeline handle{VK_NULL_HANDLE};
    VkPipelineLayout layout{VK_NULL_HANDLE};
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::unordered_map<std::string, DescriptorBinding> namedBindings;
    PipelineDesc desc{};
    PipelineType type{PipelineType::Graphics};
};

struct GraphTexture
{
    Texture texture;
    vk_sync::AccessType previousAccess{vk_sync::AccessType::Nothing};
    bool owned{true};
};

struct GraphBuffer
{
    Buffer buffer;
    vk_sync::AccessType previousAccess{vk_sync::AccessType::Nothing};
    bool owned{true};
};

struct GraphResources
{
    std::vector<GraphBuffer> buffers;
    std::vector<GraphTexture> textures;
    std::vector<Pipeline> pipelines;

    [[nodiscard]] const GraphBuffer &buffer(BufferId id) const;
    [[nodiscard]] GraphBuffer &buffer(BufferId id);
    [[nodiscard]] const GraphTexture &texture(TextureId id) const;
    [[nodiscard]] GraphTexture &texture(TextureId id);
    [[nodiscard]] const Pipeline &pipeline(PipelineId id) const;
};

enum class ViewType
{
    Full,
    Layer
};

struct Attachment
{
    TextureId texture{};
    ViewType view{ViewType::Full};
    uint32_t layer{0};
    VkAttachmentLoadOp loadOp{VK_ATTACHMENT_LOAD_OP_CLEAR};
};

struct ExternalDepthAttachment
{
    Image image;
    VkAttachmentLoadOp loadOp{VK_ATTACHMENT_LOAD_OP_CLEAR};
};

using DepthAttachment = std::variant<Attachment, ExternalDepthAttachment>;

enum class TextureResourceType
{
    CombinedImageSampler,
    StorageImage
};

struct TextureResource
{
    TextureId texture{};
    TextureResourceType inputType{TextureResourceType::CombinedImageSampler};
    vk_sync::AccessType accessType{
        vk_sync::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer};
};

struct BufferResource
{
    BufferId buffer{};
    vk_sync::AccessType accessType{vk_sync::AccessType::AnyShaderReadOther};
};

using Resource = std::variant<TextureResource, BufferResource>;

struct TextureCopy
{
    TextureId source{};
    TextureId destination{};
    VkImageCopy region{};
};

struct UniformData
{
    std::array<std::byte, kMaxUniformSize> data{};
    VkDeviceSize size{0};
};

class RenderGraph;
struct RenderPass;

using RenderFunction =
    std::function<void(VkCommandBuffer, const RenderPass &, const GraphResources &)>;

struct RenderPass
{
    PipelineId pipelineHandle{};
    RenderFunction renderFunction;
    std::vector<Resource> reads;
    std::vector<Attachment> writes;
    std::optional<DepthAttachment> depthAttachment;
    bool presentationPass{false};
    std::optional<DescriptorSet> readResourcesDescriptorSet;
    std::string name;
    // key = pass name + '_' + shader uniform name, value = {shader name, bytes}
    std::map<std::string, std::pair<std::string, UniformData>> uniforms;
    std::optional<BufferId> uniformBuffer;
    std::optional<DescriptorSet> uniformsDescriptorSet;
    std::optional<TextureCopy> copyCommand;
    std::vector<std::pair<BufferId, vk_sync::AccessType>> extraBarriers;
};

struct RenderContext
{
    VkDescriptorSet bindlessDescriptorSet{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> viewDescriptorSets;
};

class PassBuilder
{
  public:
    PassBuilder(RenderGraph &graph, std::string name, PipelineId pipelineHandle);
    PassBuilder(PassBuilder &&) noexcept = default;
    PassBuilder &operator=(PassBuilder &&) noexcept = default;
    PassBuilder(const PassBuilder &) = delete;
    PassBuilder &operator=(const PassBuilder &) = delete;

    PassBuilder &read(TextureId resourceId);
    PassBuilder &imageWrite(TextureId resourceId);
    PassBuilder &writeBuffer(BufferId resourceId);
    PassBuilder &readBuffer(BufferId resourceId);
    PassBuilder &write(TextureId resourceId);
    PassBuilder &writeLayer(TextureId resourceId, uint32_t layer);
    PassBuilder &loadWrite(TextureId resourceId);
    PassBuilder &
    extraBarriers(const std::vector<std::pair<BufferId, vk_sync::AccessType>> &barriers);
    PassBuilder &render(RenderFunction function);
    PassBuilder &dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    PassBuilder &copyImage(TextureId source, TextureId destination, VkImageCopy region);
    PassBuilder &presentationPass(bool enabled = true);
    PassBuilder &depthAttachment(TextureId texture);
    PassBuilder &depthAttachmentLayer(TextureId texture, uint32_t layer);
    PassBuilder &externalDepthAttachment(Image image, VkAttachmentLoadOp loadOp);

    template <typename T> PassBuilder &uniforms(std::string_view name, const T &value)
    {
        static_assert(std::is_trivially_copyable_v<T>,
                      "RenderGraph uniform data must be trivially copyable");
        static_assert(sizeof(T) < kMaxUniformSize,
                      "RenderGraph uniform data exceeds kMaxUniformSize");
        const std::string uniqueName = name_ + "_" + std::string{name};
        auto &entry = uniforms_[uniqueName];
        entry.first = std::string{name};
        entry.second.size = sizeof(T);
        std::memcpy(entry.second.data.data(), &value, sizeof(T));
        return *this;
    }

    // Consumes this builder. Calling build twice is an error.
    void build();

  private:
    friend class RenderGraph;

    RenderGraph *graph_{};
    std::string name_;
    PipelineId pipelineHandle_{};
    std::vector<Resource> reads_;
    std::vector<Attachment> writes_;
    RenderFunction renderFunction_;
    std::optional<DepthAttachment> depthAttachment_;
    bool presentationPass_{false};
    std::map<std::string, std::pair<std::string, UniformData>> uniforms_;
    std::optional<TextureCopy> copyCommand_;
    std::vector<std::pair<BufferId, vk_sync::AccessType>> extraBarriers_;
    bool built_{false};
};

class RenderGraph
{
  public:
    explicit RenderGraph(Device &device, uint32_t framesInFlight);
    ~RenderGraph();
    RenderGraph(const RenderGraph &) = delete;
    RenderGraph &operator=(const RenderGraph &) = delete;

    void newFrame(std::size_t currentFrame);
    void clear();

    PassBuilder addPass(std::string name, PipelineId pipelineHandle);
    PassBuilder addPassFromDesc(std::string name, PipelineDesc pipelineDesc);

    TextureId createTexture(std::string debugName, const ImageDesc &imageDesc);
    TextureId importTexture(std::string debugName, Texture texture,
                            vk_sync::AccessType initialAccess = vk_sync::AccessType::Nothing);
    BufferId createBuffer(std::string debugName, VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags memoryProperties);
    BufferId importBuffer(std::string debugName, Buffer buffer,
                          vk_sync::AccessType initialAccess = vk_sync::AccessType::Nothing);
    PipelineId createPipeline(PipelineDesc pipelineDesc);

    void prepare();
    void render(Renderer &renderer, VkCommandBuffer commandBuffer, RenderContext &context,
                const Image &presentImage);

    [[nodiscard]] const GraphResources &resources() const
    {
        return resources_;
    }

    [[nodiscard]] GraphResources &resources()
    {
        return resources_;
    }

    [[nodiscard]] const std::vector<PipelineDesc> &pipelineDescriptions() const
    {
        return pipelineDescs_;
    }

    [[nodiscard]] std::size_t currentFrame() const
    {
        return currentFrame_;
    }

    const std::vector<RenderPass> &passes() const
    {
        return passes_.at(currentFrame_);
    }

  private:
    friend class PassBuilder;

    void finishPass(PassBuilder &&builder);
    void createReadResourcesDescriptorSet(RenderPass &pass);
    void createUniformDescriptorSet(RenderPass &pass);
    void updateUniformBuffer(RenderPass &pass);
    void
    prepareRenderPass(Renderer &renderer, VkCommandBuffer commandBuffer, const RenderPass &pass,
                      const std::vector<std::tuple<Image, ViewType, uint32_t, VkAttachmentLoadOp>>
                          &colorAttachments,
                      const std::optional<std::tuple<Image, ViewType, uint32_t, VkAttachmentLoadOp>>
                          &depthAttachment,
                      VkExtent2D extent);
    DescriptorSet allocateDescriptorSet(VkDescriptorSetLayout layout,
                                        const std::vector<VkDescriptorType> &types) const;
    void bindDescriptorSet(VkCommandBuffer commandBuffer, const Pipeline &pipeline,
                           uint32_t setIndex, VkDescriptorSet descriptorSet) const;
    Device &device_;
    std::vector<std::vector<RenderPass>> passes_;
    GraphResources resources_;
    std::vector<PipelineDesc> pipelineDescs_;
    std::size_t currentFrame_{0};
};

[[nodiscard]] bool isDepthFormat(VkFormat format);

} // namespace jhb::rg
