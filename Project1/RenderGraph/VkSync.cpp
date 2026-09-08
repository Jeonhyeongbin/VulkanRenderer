#include "VkSync.h"

#include <stdexcept>

namespace jhb::vk_sync
{
namespace
{

VkImageLayout chooseLayout(ImageLayout policy, AccessType access, VkImageLayout optimal)
{
    switch (policy)
    {
    case ImageLayout::Optimal:
        return optimal;
    case ImageLayout::General:
        return access == AccessType::Present ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                                             : VK_IMAGE_LAYOUT_GENERAL;
    case ImageLayout::GeneralAndPresentation:
#ifdef VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR
        return VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR;
#else
        return VK_IMAGE_LAYOUT_GENERAL;
#endif
    }
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

} // namespace

AccessInfo getAccessInfo(AccessType type)
{
    using A = AccessType;
    switch (type)
    {
    case A::Nothing:
        return {};
    case A::CommandBufferReadNVX:
        return {VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
                VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV, VK_IMAGE_LAYOUT_UNDEFINED};
    case A::IndirectBuffer:
        return {VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED};
    case A::IndexBuffer:
        return {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_INDEX_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED};
    case A::VertexBuffer:
        return {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED};
    case A::VertexShaderReadUniformBuffer:
        return {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED};
    case A::VertexShaderReadSampledImageOrUniformTexelBuffer:
        return {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    case A::VertexShaderReadOther:
        return {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case A::TessellationControlShaderReadUniformBuffer:
        return {VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED};
    case A::TessellationControlShaderReadSampledImageOrUniformTexelBuffer:
        return {VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    case A::TessellationControlShaderReadOther:
        return {VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case A::TessellationEvaluationShaderReadUniformBuffer:
        return {VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED};
    case A::TessellationEvaluationShaderReadSampledImageOrUniformTexelBuffer:
        return {VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    case A::TessellationEvaluationShaderReadOther:
        return {VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case A::GeometryShaderReadUniformBuffer:
        return {VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED};
    case A::GeometryShaderReadSampledImageOrUniformTexelBuffer:
        return {VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    case A::GeometryShaderReadOther:
        return {VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case A::FragmentShaderReadUniformBuffer:
        return {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED};
    case A::FragmentShaderReadSampledImageOrUniformTexelBuffer:
        return {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    case A::FragmentShaderReadColorInputAttachment:
        return {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    case A::FragmentShaderReadDepthStencilInputAttachment:
        return {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
    case A::FragmentShaderReadOther:
        return {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case A::ColorAttachmentRead:
        return {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    case A::DepthStencilAttachmentRead:
        return {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
    case A::ComputeShaderReadUniformBuffer:
        return {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_UNIFORM_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED};
    case A::ComputeShaderReadSampledImageOrUniformTexelBuffer:
        return {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    case A::ComputeShaderReadOther:
        return {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case A::AnyShaderReadUniformBuffer:
        return {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_UNIFORM_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED};
    case A::AnyShaderReadUniformBufferOrVertexBuffer:
        return {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED};
    case A::AnyShaderReadSampledImageOrUniformTexelBuffer:
        return {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    case A::AnyShaderReadOther:
        return {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case A::TransferRead:
        return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL};
    case A::HostRead:
        return {VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT, VK_IMAGE_LAYOUT_GENERAL};
    case A::Present:
        return {0, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
    case A::CommandBufferWriteNVX:
        return {VK_PIPELINE_STAGE_COMMAND_PREPROCESS_BIT_NV,
                VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV, VK_IMAGE_LAYOUT_UNDEFINED};
    case A::VertexShaderWrite:
        return {VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case A::TessellationControlShaderWrite:
        return {VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case A::TessellationEvaluationShaderWrite:
        return {VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case A::GeometryShaderWrite:
        return {VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case A::FragmentShaderWrite:
        return {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case A::ColorAttachmentWrite:
        return {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    case A::DepthStencilAttachmentWrite:
        return {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    case A::DepthAttachmentWriteStencilReadOnly:
        return {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL};
    case A::StencilAttachmentWriteDepthReadOnly:
        return {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL};
    case A::ComputeShaderWrite:
        return {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case A::AnyShaderWrite:
        return {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                VK_IMAGE_LAYOUT_GENERAL};
    case A::TransferWrite:
        return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL};
    case A::HostWrite:
        return {VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL};
    case A::ColorAttachmentReadWrite:
        return {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    case A::DepthStencilAttachmentReadWrite:
        return {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    case A::General:
        return {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL};
    }
    throw std::logic_error("Unknown vk_sync::AccessType");
}

bool isWriteAccess(AccessType type)
{
    using A = AccessType;
    switch (type)
    {
    case A::CommandBufferWriteNVX:
    case A::VertexShaderWrite:
    case A::TessellationControlShaderWrite:
    case A::TessellationEvaluationShaderWrite:
    case A::GeometryShaderWrite:
    case A::FragmentShaderWrite:
    case A::ColorAttachmentWrite:
    case A::DepthStencilAttachmentWrite:
    case A::DepthAttachmentWriteStencilReadOnly:
    case A::StencilAttachmentWriteDepthReadOnly:
    case A::ComputeShaderWrite:
    case A::AnyShaderWrite:
    case A::TransferWrite:
    case A::HostWrite:
    case A::ColorAttachmentReadWrite:
    case A::General:
    case A::DepthStencilAttachmentReadWrite:
        return true;
    default:
        return false;
    }
}

void cmdPipelineBarrier(VkCommandBuffer commandBuffer,
                        const std::optional<GlobalBarrier> &globalBarrier,
                        const std::vector<BufferBarrier> &bufferBarriers,
                        const std::vector<ImageBarrier> &imageBarriers)
{
    VkPipelineStageFlags sourceStages = 0;
    VkPipelineStageFlags destinationStages = 0;
    VkMemoryBarrier memoryBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    bool hasMemoryBarrier = false;

    if (globalBarrier)
    {
        hasMemoryBarrier = true;
        for (const auto access : globalBarrier->previousAccesses)
        {
            const auto info = getAccessInfo(access);
            sourceStages |= info.stageMask;
            if (isWriteAccess(access))
            {
                memoryBarrier.srcAccessMask |= info.accessMask;
            }
        }
        for (const auto access : globalBarrier->nextAccesses)
        {
            const auto info = getAccessInfo(access);
            destinationStages |= info.stageMask;
            if (memoryBarrier.srcAccessMask != 0)
            {
                memoryBarrier.dstAccessMask |= info.accessMask;
            }
        }
    }

    std::vector<VkBufferMemoryBarrier> vkBufferBarriers;
    vkBufferBarriers.reserve(bufferBarriers.size());
    for (const auto &barrier : bufferBarriers)
    {
        VkBufferMemoryBarrier result{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        result.srcQueueFamilyIndex = barrier.srcQueueFamilyIndex;
        result.dstQueueFamilyIndex = barrier.dstQueueFamilyIndex;
        result.buffer = barrier.buffer;
        result.offset = barrier.offset;
        result.size = barrier.size;
        for (const auto access : barrier.previousAccesses)
        {
            const auto info = getAccessInfo(access);
            sourceStages |= info.stageMask;
            if (isWriteAccess(access))
            {
                result.srcAccessMask |= info.accessMask;
            }
        }
        for (const auto access : barrier.nextAccesses)
        {
            const auto info = getAccessInfo(access);
            destinationStages |= info.stageMask;
            if (result.srcAccessMask != 0)
            {
                result.dstAccessMask |= info.accessMask;
            }
        }
        vkBufferBarriers.push_back(result);
    }

    std::vector<VkImageMemoryBarrier> vkImageBarriers;
    vkImageBarriers.reserve(imageBarriers.size());
    for (const auto &barrier : imageBarriers)
    {
        VkImageMemoryBarrier result{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        result.srcQueueFamilyIndex = barrier.srcQueueFamilyIndex;
        result.dstQueueFamilyIndex = barrier.dstQueueFamilyIndex;
        result.image = barrier.image;
        result.subresourceRange = barrier.range;
        for (const auto access : barrier.previousAccesses)
        {
            const auto info = getAccessInfo(access);
            sourceStages |= info.stageMask;
            if (isWriteAccess(access))
            {
                result.srcAccessMask |= info.accessMask;
            }
            result.oldLayout = barrier.discardContents
                                   ? VK_IMAGE_LAYOUT_UNDEFINED
                                   : chooseLayout(barrier.previousLayout, access, info.imageLayout);
        }
        for (const auto access : barrier.nextAccesses)
        {
            const auto info = getAccessInfo(access);
            destinationStages |= info.stageMask;
            if (result.srcAccessMask != 0)
            {
                result.dstAccessMask |= info.accessMask;
            }
            result.newLayout = chooseLayout(barrier.nextLayout, access, info.imageLayout);
        }
        vkImageBarriers.push_back(result);
    }

    if (sourceStages == 0)
    {
        sourceStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
    if (destinationStages == 0)
    {
        destinationStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStages, destinationStages, 0,
                         hasMemoryBarrier ? 1u : 0u, hasMemoryBarrier ? &memoryBarrier : nullptr,
                         static_cast<uint32_t>(vkBufferBarriers.size()), vkBufferBarriers.data(),
                         static_cast<uint32_t>(vkImageBarriers.size()), vkImageBarriers.data());
}

AccessType globalPipelineBarrier(VkCommandBuffer commandBuffer, AccessType previousAccess,
                                 AccessType nextAccess)
{
    cmdPipelineBarrier(commandBuffer, GlobalBarrier{{previousAccess}, {nextAccess}}, {}, {});
    return nextAccess;
}

AccessType imagePipelineBarrier(VkCommandBuffer commandBuffer, VkImage image,
                                const VkImageSubresourceRange &range, AccessType previousAccess,
                                AccessType nextAccess, bool discardContents)
{
    ImageBarrier barrier;
    barrier.previousAccesses = {previousAccess};
    barrier.nextAccesses = {nextAccess};
    barrier.discardContents = discardContents;
    barrier.image = image;
    barrier.range = range;
    cmdPipelineBarrier(commandBuffer, std::nullopt, {}, {barrier});
    return nextAccess;
}

} // namespace jhb::vk_sync
