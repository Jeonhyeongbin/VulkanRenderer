#include "RenderGraph.h"
#include "../Renderer.h"
#include <stdexcept>
#include <algorithm>

namespace jhb::rg
{
namespace
{
void check(VkResult r)
{
    if (r != VK_SUCCESS)
    {
        throw std::runtime_error("Descriptor allocation failed");
    }
}

VkPipelineBindPoint point(const Pipeline &p)
{
    return p.type == PipelineType::Compute ? VK_PIPELINE_BIND_POINT_COMPUTE
                                           : VK_PIPELINE_BIND_POINT_GRAPHICS;
}
} // namespace

void RenderGraph::finishPass(PassBuilder &&b)
{
    RenderPass p;
    p.name = std::move(b.name_);
    p.pipelineHandle = b.pipelineHandle_;
    p.reads = std::move(b.reads_);
    p.writes = std::move(b.writes_);
    p.depthAttachment = std::move(b.depthAttachment_);
    p.presentationPass = b.presentationPass_;
    p.uniforms = std::move(b.uniforms_);
    p.renderFunction = std::move(b.renderFunction_);
    p.copyCommand = std::move(b.copyCommand_);
    p.extraBarriers = std::move(b.extraBarriers_);
    // Finalize a copy. Never mutate a cached description shared by earlier passes.
    auto desc = pipelineDescs_.at(p.pipelineHandle);
    if (!p.presentationPass)
    {
        desc.colorAttachmentFormats.clear();
        for (const auto &w : p.writes)
        {
            desc.colorAttachmentFormats.push_back(
                resources_.texture(w.texture).texture.image.desc.format);
        }
    }
    if (p.depthAttachment)
    {
        if (auto *a = std::get_if<Attachment>(&*p.depthAttachment))
        {
            desc.depthStencilAttachmentFormat =
                resources_.texture(a->texture).texture.image.desc.format;
        }
        else
        {
            desc.depthStencilAttachmentFormat =
                std::get<ExternalDepthAttachment>(*p.depthAttachment).image.desc.format;
        }
    }
    if (p.uniforms.size() > 1)
    {
        throw std::invalid_argument("Only one uniform block per pass is supported");
    }
    p.pipelineHandle = createPipeline(std::move(desc));
    if (!p.uniforms.empty())
    {
        p.uniformBuffer = createBuffer(
            p.name + "/" + p.uniforms.begin()->first + "/" + std::to_string(currentFrame_),
            p.uniforms.begin()->second.second.size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
    passes_[currentFrame_].push_back(std::move(p));
}

DescriptorSet RenderGraph::allocateDescriptorSet(VkDescriptorSetLayout layout,
                                                 const std::vector<VkDescriptorType> &types) const
{
    std::map<VkDescriptorType, uint32_t> counts;
    for (auto t : types)
    {
        ++counts[t];
    }
    std::vector<VkDescriptorPoolSize> sizes;
    for (auto [t, count] : counts)
    {
        sizes.push_back({t, count});
    }
    DescriptorSet set;
    set.device = device_.handle();
    VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    info.flags = device_.descriptorPoolFlags();
    info.maxSets = 1;
    info.poolSizeCount = static_cast<uint32_t>(sizes.size());
    info.pPoolSizes = sizes.data();
    check(vkCreateDescriptorPool(set.device, &info, nullptr, &set.pool));
    VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc.descriptorPool = set.pool;
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts = &layout;
    check(vkAllocateDescriptorSets(set.device, &alloc, &set.handle));
    return set;
}

void RenderGraph::createReadResourcesDescriptorSet(RenderPass &p)
{
    if (p.reads.empty() || p.readResourcesDescriptorSet)
    {
        return;
    }
    const auto &pipeline = resources_.pipeline(p.pipelineHandle);
    std::vector<VkDescriptorType> types;
    // Size the pool for the entire layout, not just the bindings written below.
    for (const auto &b : pipeline.desc.bindings)
    {
        if (b.set == kDescriptorSetIndexInputResources)
        {
            for (uint32_t i = 0; i < b.count; ++i)
            {
                types.push_back(b.type);
            }
        }
    }

    // The vector index is the shader set number, not a binding number.
    // Set 0 is reserved for bindless resources; set 1 is the per-frame view.
    auto set = allocateDescriptorSet(
        pipeline.descriptorSetLayouts.at(kDescriptorSetIndexInputResources), types);
    for (uint32_t i = 0; i < p.reads.size(); ++i)
    {
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = set.handle;
        w.dstBinding = i;
        w.descriptorCount = 1;
        VkDescriptorImageInfo image{};
        VkDescriptorBufferInfo buffer{};
        if (auto *t = std::get_if<TextureResource>(&p.reads[i]))
        {
            const auto &texture = resources_.texture(t->texture).texture;
            const bool sampled = t->inputType == TextureResourceType::CombinedImageSampler;
            image = {sampled ? texture.sampler : VK_NULL_HANDLE, texture.image.imageView,
                     sampled ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL};
            w.descriptorType = sampled ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                       : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            w.pImageInfo = &image;
        }
        else
        {
            const auto &b = resources_.buffer(std::get<BufferResource>(p.reads[i]).buffer).buffer;
            buffer = {b.buffer, 0, b.size};
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            w.pBufferInfo = &buffer;
        }
        auto binding = std::find_if(pipeline.desc.bindings.begin(), pipeline.desc.bindings.end(),
                                    [&](const auto &b)
                                    {
                                        return b.set == kDescriptorSetIndexInputResources &&
                                               b.binding == i && b.type == w.descriptorType &&
                                               b.count == 1;
                                    });
        if (binding == pipeline.desc.bindings.end())
        {
            throw std::invalid_argument("Pass read does not match set 2 layout");
        }
        vkUpdateDescriptorSets(device_.handle(), 1, &w, 0, nullptr);
    }
    p.readResourcesDescriptorSet = std::move(set);
}

void RenderGraph::createUniformDescriptorSet(RenderPass &p)
{
    if (p.uniforms.empty() || p.uniformsDescriptorSet)
    {
        return;
    }
    const auto &pipeline = resources_.pipeline(p.pipelineHandle);
    const auto &binding = pipeline.namedBindings.at(p.uniforms.begin()->second.first);
    if (binding.type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
        binding.set <= kDescriptorSetIndexInputResources)
    {
        throw std::invalid_argument("Pass uniform must use its own uniform-buffer set above set 2");
    }
    std::vector<VkDescriptorType> types;
    for (const auto &b : pipeline.desc.bindings)
    {
        if (b.set == binding.set)
        {
            for (uint32_t i = 0; i < b.count; ++i)
            {
                types.push_back(b.type);
            }
        }
    }
    auto set = allocateDescriptorSet(pipeline.descriptorSetLayouts.at(binding.set), types);
    const auto &b = resources_.buffer(p.uniformBuffer.value()).buffer;
    VkDescriptorBufferInfo info{b.buffer, 0, b.size};
    VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = set.handle;
    w.dstBinding = binding.binding;
    w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    w.descriptorCount = 1;
    w.pBufferInfo = &info;
    vkUpdateDescriptorSets(device_.handle(), 1, &w, 0, nullptr);
    p.uniformsDescriptorSet = std::move(set);
}

void RenderGraph::updateUniformBuffer(RenderPass &p)
{
    if (!p.uniformBuffer)
    {
        return;
    }
    const auto &u = p.uniforms.begin()->second.second;
    device_.uploadBuffer(resources_.buffer(*p.uniformBuffer).buffer, u.data.data(),
                         static_cast<size_t>(u.size));
}

void RenderGraph::bindDescriptorSet(VkCommandBuffer cb, const Pipeline &p, uint32_t index,
                                    VkDescriptorSet set) const
{
    vkCmdBindDescriptorSets(cb, point(p), p.layout, index, 1, &set, 0, nullptr);
}

void RenderGraph::prepareRenderPass(
    Renderer &renderer, VkCommandBuffer cb, const RenderPass &p,
    const std::vector<std::tuple<Image, ViewType, uint32_t, VkAttachmentLoadOp>> &colors,
    const std::optional<std::tuple<Image, ViewType, uint32_t, VkAttachmentLoadOp>> &depth,
    VkExtent2D extent)
{
    const auto &pipeline = resources_.pipeline(p.pipelineHandle);
    if (pipeline.type == PipelineType::Graphics)
    {
        auto attachment = [](const auto &a, bool isDepth)
        {
            const auto &[image, view, layer, load] = a;
            VkRenderingAttachmentInfoKHR info{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR};
            info.imageView = view == ViewType::Full ? image.imageView : image.viewForLayer(layer);
            info.imageLayout = isDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                       : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            info.loadOp = load;
            info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            if (isDepth)
            {
                info.clearValue.depthStencil = {1.0f, 0};
            }
            return info; // color defaults to transparent black
        };
        std::vector<VkRenderingAttachmentInfoKHR> colorInfos;
        for (const auto &c : colors)
        {
            colorInfos.push_back(attachment(c, false));
            if (colorInfos.size() <= p.writes.size())
            {
                colorInfos.back().clearValue.color = p.writes[colorInfos.size() - 1].clearColor;
            }
        }
        VkRenderingAttachmentInfoKHR depthInfo{};
        if (depth)
        {
            depthInfo = attachment(*depth, true);
        }
        VkRenderingInfoKHR info{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
        info.renderArea.extent = extent;
        info.layerCount = 1;
        info.colorAttachmentCount = static_cast<uint32_t>(colorInfos.size());
        info.pColorAttachments = colorInfos.data();
        info.pDepthAttachment = depth ? &depthInfo : nullptr;
        renderer.beginRendering(cb, info);
        VkViewport viewport{0, float(extent.height), float(extent.width), -float(extent.height), 0,
                            1};
        VkRect2D scissor{{0, 0}, extent};
        vkCmdSetViewport(cb, 0, 1, &viewport);
        vkCmdSetScissor(cb, 0, 1, &scissor);
    }
    vkCmdBindPipeline(cb, point(pipeline), pipeline.handle);
}

bool isDepthFormat(VkFormat f)
{
    return f == VK_FORMAT_D16_UNORM || f == VK_FORMAT_D32_SFLOAT ||
           f == VK_FORMAT_X8_D24_UNORM_PACK32 || f == VK_FORMAT_D24_UNORM_S8_UINT ||
           f == VK_FORMAT_D32_SFLOAT_S8_UINT || f == VK_FORMAT_D16_UNORM_S8_UINT;
}
} // namespace jhb::rg
