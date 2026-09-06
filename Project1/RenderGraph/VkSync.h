#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace jhb::vk_sync
{

// C++ port of the access model used by vk-sync-rs.  RenderGraph stores one of
// these values per resource instead of exposing stage/access/layout triples to
// every pass.
enum class AccessType
{
    Nothing,
    CommandBufferReadNVX,
    IndirectBuffer,
    IndexBuffer,
    VertexBuffer,
    VertexShaderReadUniformBuffer,
    VertexShaderReadSampledImageOrUniformTexelBuffer,
    VertexShaderReadOther,
    TessellationControlShaderReadUniformBuffer,
    TessellationControlShaderReadSampledImageOrUniformTexelBuffer,
    TessellationControlShaderReadOther,
    TessellationEvaluationShaderReadUniformBuffer,
    TessellationEvaluationShaderReadSampledImageOrUniformTexelBuffer,
    TessellationEvaluationShaderReadOther,
    GeometryShaderReadUniformBuffer,
    GeometryShaderReadSampledImageOrUniformTexelBuffer,
    GeometryShaderReadOther,
    FragmentShaderReadUniformBuffer,
    FragmentShaderReadSampledImageOrUniformTexelBuffer,
    FragmentShaderReadColorInputAttachment,
    FragmentShaderReadDepthStencilInputAttachment,
    FragmentShaderReadOther,
    ColorAttachmentRead,
    DepthStencilAttachmentRead,
    ComputeShaderReadUniformBuffer,
    ComputeShaderReadSampledImageOrUniformTexelBuffer,
    ComputeShaderReadOther,
    AnyShaderReadUniformBuffer,
    AnyShaderReadUniformBufferOrVertexBuffer,
    AnyShaderReadSampledImageOrUniformTexelBuffer,
    AnyShaderReadOther,
    TransferRead,
    HostRead,
    Present,
    CommandBufferWriteNVX,
    VertexShaderWrite,
    TessellationControlShaderWrite,
    TessellationEvaluationShaderWrite,
    GeometryShaderWrite,
    FragmentShaderWrite,
    ColorAttachmentWrite,
    DepthStencilAttachmentWrite,
    DepthAttachmentWriteStencilReadOnly,
    StencilAttachmentWriteDepthReadOnly,
    ComputeShaderWrite,
    AnyShaderWrite,
    TransferWrite,
    HostWrite,
    ColorAttachmentReadWrite,
    General,
};

enum class ImageLayout
{
    Optimal,
    General,
    GeneralAndPresentation,
};

struct AccessInfo
{
    VkPipelineStageFlags stageMask{0};
    VkAccessFlags accessMask{0};
    VkImageLayout imageLayout{VK_IMAGE_LAYOUT_UNDEFINED};
};

struct GlobalBarrier
{
    std::vector<AccessType> previousAccesses;
    std::vector<AccessType> nextAccesses;
};

struct BufferBarrier
{
    std::vector<AccessType> previousAccesses;
    std::vector<AccessType> nextAccesses;
    uint32_t srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED};
    uint32_t dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED};
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceSize offset{0};
    VkDeviceSize size{VK_WHOLE_SIZE};
};

struct ImageBarrier
{
    std::vector<AccessType> previousAccesses;
    std::vector<AccessType> nextAccesses;
    ImageLayout previousLayout{ImageLayout::Optimal};
    ImageLayout nextLayout{ImageLayout::Optimal};
    bool discardContents{false};
    uint32_t srcQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED};
    uint32_t dstQueueFamilyIndex{VK_QUEUE_FAMILY_IGNORED};
    VkImage image{VK_NULL_HANDLE};
    VkImageSubresourceRange range{};
};

AccessInfo getAccessInfo(AccessType accessType);
bool isWriteAccess(AccessType accessType);

// Combines all supplied barriers into one vkCmdPipelineBarrier call, matching
// vk-sync-rs's command API.
void cmdPipelineBarrier(VkCommandBuffer commandBuffer,
                        const std::optional<GlobalBarrier> &globalBarrier,
                        const std::vector<BufferBarrier> &bufferBarriers,
                        const std::vector<ImageBarrier> &imageBarriers);

AccessType globalPipelineBarrier(VkCommandBuffer commandBuffer, AccessType previousAccess,
                                 AccessType nextAccess);

AccessType imagePipelineBarrier(VkCommandBuffer commandBuffer, VkImage image,
                                const VkImageSubresourceRange &range, AccessType previousAccess,
                                AccessType nextAccess, bool discardContents = false);

} // namespace jhb::vk_sync
