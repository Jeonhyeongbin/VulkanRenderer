#include "RenderGraph.h"
#include "../Renderer.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace jhb::rg
{
namespace
{

VkPipelineBindPoint bindPoint(PipelineType type)
{
    switch (type)
    {
    case PipelineType::Graphics:
        return VK_PIPELINE_BIND_POINT_GRAPHICS;
    case PipelineType::Compute:
        return VK_PIPELINE_BIND_POINT_COMPUTE;
    }
    throw std::logic_error("Unknown RenderGraph pipeline type");
}

VkImageSubresourceRange wholeImageRange(const Image &image)
{
    VkImageSubresourceRange range{};
    range.aspectMask = image.desc.aspectFlags;
    range.baseMipLevel = 0;
    range.levelCount = VK_REMAINING_MIP_LEVELS;
    range.baseArrayLayer = 0;
    range.layerCount = VK_REMAINING_ARRAY_LAYERS;
    return range;
}

VkImageView attachmentView(const Image &image, ViewType view, uint32_t layer)
{
    return view == ViewType::Full ? image.imageView : image.viewForLayer(layer);
}

} // namespace

const GraphBuffer &GraphResources::buffer(BufferId id) const
{
    return buffers.at(id);
}

GraphBuffer &GraphResources::buffer(BufferId id)
{
    return buffers.at(id);
}

const GraphTexture &GraphResources::texture(TextureId id) const
{
    return textures.at(id);
}

GraphTexture &GraphResources::texture(TextureId id)
{
    return textures.at(id);
}

const Pipeline &GraphResources::pipeline(PipelineId id) const
{
    return pipelines.at(id);
}

PassBuilder::PassBuilder(RenderGraph &graph, std::string name, PipelineId pipelineHandle)
    : graph_{&graph}, name_{std::move(name)}, pipelineHandle_{pipelineHandle}
{
}

PassBuilder &PassBuilder::read(TextureId id)
{
    reads_.emplace_back(
        TextureResource{id, TextureResourceType::CombinedImageSampler,
                        vk_sync::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer});
    return *this;
}

PassBuilder &PassBuilder::imageWrite(TextureId id)
{
    reads_.emplace_back(TextureResource{id, TextureResourceType::StorageImage,
                                        vk_sync::AccessType::AnyShaderWrite});
    return *this;
}

PassBuilder &PassBuilder::writeBuffer(BufferId id)
{
    reads_.emplace_back(BufferResource{id, vk_sync::AccessType::AnyShaderWrite});
    return *this;
}

PassBuilder &PassBuilder::readBuffer(BufferId id)
{
    reads_.emplace_back(BufferResource{id, vk_sync::AccessType::AnyShaderReadOther});
    return *this;
}

PassBuilder &PassBuilder::write(TextureId id)
{
    writes_.push_back({id, ViewType::Full, 0, VK_ATTACHMENT_LOAD_OP_CLEAR});
    return *this;
}

PassBuilder &PassBuilder::writeLayer(TextureId id, uint32_t layer, VkClearColorValue clearColor)
{
    writes_.push_back({id, ViewType::Layer, layer, VK_ATTACHMENT_LOAD_OP_CLEAR, clearColor});
    return *this;
}

PassBuilder &PassBuilder::loadWrite(TextureId id)
{
    writes_.push_back({id, ViewType::Full, 0, VK_ATTACHMENT_LOAD_OP_LOAD});
    return *this;
}

PassBuilder &
PassBuilder::extraBarriers(const std::vector<std::pair<BufferId, vk_sync::AccessType>> &barriers)
{
    extraBarriers_ = barriers;
    return *this;
}

PassBuilder &PassBuilder::render(RenderFunction function)
{
    renderFunction_ = std::move(function);
    return *this;
}

PassBuilder &PassBuilder::dispatch(uint32_t x, uint32_t y, uint32_t z)
{
    renderFunction_ = [x, y, z](VkCommandBuffer commandBuffer, const RenderPass &,
                                const GraphResources &) { vkCmdDispatch(commandBuffer, x, y, z); };
    return *this;
}

PassBuilder &PassBuilder::copyImage(TextureId source, TextureId destination, VkImageCopy region)
{
    copyCommand_ = TextureCopy{source, destination, region};
    return *this;
}

PassBuilder &PassBuilder::presentationPass(bool enabled)
{
    presentationPass_ = enabled;
    return *this;
}

PassBuilder &PassBuilder::copyImageToBuffer(TextureId source, BufferId destination,
                                            VkBufferImageCopy region,
                                            vk_sync::AccessType finalAccess)
{
    ImageToBufferCopy copy{source, destination, region, finalAccess};
    copy.validate(graph_->resources());
    if (bufferCopyCommand_)
    {
        throw std::logic_error("Only one image-to-buffer copy per pass is supported");
    }
    bufferCopyCommand_ = copy;
    return *this;
}

VkDeviceSize ImageToBufferCopy::validate(const GraphResources &resources) const
{
    const auto &image = resources.texture(source).texture.image.desc;
    const auto &buffer = resources.buffer(destination).buffer;
    uint32_t texelBytes = 0;
    switch (image.format)
    {
    case VK_FORMAT_R8_UNORM:
        texelBytes = 1;
        break;
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R16_SFLOAT:
        texelBytes = 2;
        break;
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
        texelBytes = 4;
        break;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        texelBytes = 8;
        break;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        texelBytes = 16;
        break;
    default:
        throw std::invalid_argument("Unsupported image-to-buffer copy format");
    }
    const auto &sub = region.imageSubresource;
    if (!buffer.buffer || !(buffer.usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) ||
        !(image.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) || image.imageType != ImageType::Image2D ||
        image.aspectFlags != VK_IMAGE_ASPECT_COLOR_BIT ||
        sub.aspectMask != VK_IMAGE_ASPECT_COLOR_BIT || sub.layerCount != 1 ||
        sub.baseArrayLayer >= image.arrayLayers || sub.mipLevel >= image.mipLevels ||
        sub.mipLevel >= 32 || region.bufferRowLength || region.bufferImageHeight ||
        region.imageOffset.x < 0 || region.imageOffset.y < 0 || region.imageOffset.z != 0 ||
        region.imageExtent.depth != 1 || !region.imageExtent.width || !region.imageExtent.height ||
        region.bufferOffset % 4 || region.bufferOffset % texelBytes)
    {
        throw std::invalid_argument("Invalid tightly packed 2D image-to-buffer copy");
    }
    const auto width = (std::max)(1u, image.width >> sub.mipLevel);
    const auto height = (std::max)(1u, image.height >> sub.mipLevel);
    const uint64_t texels = uint64_t(region.imageExtent.width) * region.imageExtent.height;
    if (uint64_t(region.imageOffset.x) + region.imageExtent.width > width ||
        uint64_t(region.imageOffset.y) + region.imageExtent.height > height ||
        region.bufferOffset > buffer.size ||
        texels > (buffer.size - region.bufferOffset) / texelBytes)
    {
        throw std::invalid_argument("Image-to-buffer copy exceeds resource bounds");
    }
    return texels * texelBytes;
}

void ImageToBufferCopy::record(VkCommandBuffer cmd, GraphResources &resources) const
{
    validate(resources);
    auto &image = resources.texture(source);
    auto &buffer = resources.buffer(destination);
    image.previousAccess = vk_sync::imagePipelineBarrier(
        cmd, image.texture.image.image, wholeImageRange(image.texture.image), image.previousAccess,
        vk_sync::AccessType::TransferRead, false);
    // Buffer tracking is whole-resource, matching the graph's existing access model.
    vk_sync::BufferBarrier barrier;
    barrier.buffer = buffer.buffer.buffer;
    barrier.previousAccesses = {buffer.previousAccess};
    barrier.nextAccesses = {vk_sync::AccessType::TransferWrite};
    vk_sync::cmdPipelineBarrier(cmd, std::nullopt, {barrier}, {});
    buffer.previousAccess = vk_sync::AccessType::TransferWrite;
    vkCmdCopyImageToBuffer(cmd, image.texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           buffer.buffer.buffer, 1, &region);
    if (finalAccess != vk_sync::AccessType::TransferWrite)
    {
        barrier.previousAccesses = {buffer.previousAccess};
        barrier.nextAccesses = {finalAccess};
        vk_sync::cmdPipelineBarrier(cmd, std::nullopt, {barrier}, {});
        buffer.previousAccess = finalAccess;
    }
}

PassBuilder &PassBuilder::depthAttachment(TextureId texture)
{
    depthAttachment_ = Attachment{texture, ViewType::Full, 0, VK_ATTACHMENT_LOAD_OP_CLEAR};
    return *this;
}

PassBuilder &PassBuilder::loadDepthAttachment(TextureId texture)
{
    depthAttachment_ = Attachment{texture, ViewType::Full, 0, VK_ATTACHMENT_LOAD_OP_LOAD};
    return *this;
}

PassBuilder &PassBuilder::depthAttachmentLayer(TextureId texture, uint32_t layer)
{
    depthAttachment_ = Attachment{texture, ViewType::Layer, layer, VK_ATTACHMENT_LOAD_OP_CLEAR};
    return *this;
}

PassBuilder &PassBuilder::externalDepthAttachment(Image image, VkAttachmentLoadOp loadOp)
{
    depthAttachment_ = ExternalDepthAttachment{std::move(image), loadOp};
    return *this;
}

void PassBuilder::build()
{
    if (built_ || graph_ == nullptr)
    {
        throw std::logic_error("RenderGraph PassBuilder::build called more than once");
    }
    built_ = true;
    graph_->finishPass(std::move(*this));
}

RenderGraph::RenderGraph(Device &device, uint32_t framesInFlight)
    : device_{device}, descriptorCache_(framesInFlight), passes_(framesInFlight)
{
    if (framesInFlight == 0)
    {
        throw std::invalid_argument("RenderGraph requires at least one frame in flight");
    }
    if (device_.handle() == VK_NULL_HANDLE)
    {
        throw std::invalid_argument("RenderGraph Device handle must be valid");
    }
}

RenderGraph::~RenderGraph()
{
    descriptorCache_.clear();
    for (auto &framePasses : passes_)
    {
        framePasses.clear();
    }
    for (auto &pipeline : resources_.pipelines)
    {
        device_.destroyPipeline(pipeline);
    }
    for (auto &texture : resources_.textures)
    {
        if (texture.owned)
        {
            device_.destroyTexture(texture.texture);
        }
    }
    for (auto &buffer : resources_.buffers)
    {
        if (buffer.owned)
        {
            device_.destroyBuffer(buffer.buffer);
        }
    }
}

void RenderGraph::newFrame(std::size_t currentFrame)
{
    if (currentFrame >= passes_.size())
    {
        throw std::out_of_range("RenderGraph frame index is out of range");
    }
    currentFrame_ = currentFrame;
}

void RenderGraph::clear()
{
    // clear() is called after beginFrame() has waited for this slot.
    auto &cache = descriptorCache_[currentFrame_];
    cache.clear();
    for (auto &pass : passes_[currentFrame_])
    {
        cache.push_back({pass.name, pass.pipelineHandle,
                         pass.uniforms.empty() ? std::string{} : pass.uniforms.begin()->second.first,
                         std::move(pass.readResourcesDescriptorSet),
                         std::move(pass.uniformsDescriptorSet)});
    }
    passes_[currentFrame_].clear();
}

PassBuilder RenderGraph::addPass(std::string name, PipelineId pipelineHandle)
{
    if (pipelineHandle >= pipelineDescs_.size())
    {
        throw std::out_of_range("RenderGraph pipeline handle is out of range");
    }
    return PassBuilder{*this, std::move(name), pipelineHandle};
}

PassBuilder RenderGraph::addPassFromDesc(std::string name, PipelineDesc pipelineDesc)
{
    const PipelineId pipeline = createPipeline(std::move(pipelineDesc));
    return addPass(std::move(name), pipeline);
}

TextureId RenderGraph::createTexture(std::string debugName, const ImageDesc &imageDesc)
{
    const auto found = std::find_if(resources_.textures.begin(), resources_.textures.end(),
                                    [&](const GraphTexture &texture)
                                    { return texture.texture.image.debugName == debugName; });
    if (found != resources_.textures.end())
    {
        const auto &d = found->texture.image.desc;
        if (d.width != imageDesc.width || d.height != imageDesc.height ||
            d.format != imageDesc.format || d.usage != imageDesc.usage ||
            d.depth != imageDesc.depth || d.arrayLayers != imageDesc.arrayLayers ||
            d.mipLevels != imageDesc.mipLevels || d.aspectFlags != imageDesc.aspectFlags ||
            d.imageType != imageDesc.imageType || d.filter != imageDesc.filter ||
            d.mipmapMode != imageDesc.mipmapMode)
        {
            throw std::invalid_argument("Texture cache name reused with a different description");
        }
        return static_cast<TextureId>(std::distance(resources_.textures.begin(), found));
    }
    Texture texture = device_.createTexture(imageDesc, debugName);
    texture.image.debugName = debugName;
    resources_.textures.push_back({std::move(texture), vk_sync::AccessType::Nothing, true});
    return resources_.textures.size() - 1;
}

TextureId RenderGraph::importTexture(std::string debugName, Texture texture,
                                     vk_sync::AccessType initialAccess)
{
    texture.image.debugName = std::move(debugName);
    resources_.textures.push_back({std::move(texture), initialAccess, false});
    return resources_.textures.size() - 1;
}

BufferId RenderGraph::createBuffer(std::string debugName, VkDeviceSize size,
                                   VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties)
{
    const auto found = std::find_if(resources_.buffers.begin(), resources_.buffers.end(),
                                    [&](const GraphBuffer &buffer)
                                    { return buffer.buffer.debugName == debugName; });
    if (found != resources_.buffers.end())
    {
        if (found->buffer.size != size)
        {
            throw std::invalid_argument("Buffer cache name reused with a different size");
        }
        return static_cast<BufferId>(std::distance(resources_.buffers.begin(), found));
    }
    Buffer buffer = device_.createBuffer(size, usage, memoryProperties, debugName);
    buffer.size = size;
    buffer.usage = usage;
    buffer.debugName = debugName;
    resources_.buffers.push_back({std::move(buffer), vk_sync::AccessType::Nothing, true});
    return resources_.buffers.size() - 1;
}

BufferId RenderGraph::importBuffer(std::string debugName, Buffer buffer,
                                   vk_sync::AccessType initialAccess)
{
    buffer.debugName = std::move(debugName);
    resources_.buffers.push_back({std::move(buffer), initialAccess, false});
    return resources_.buffers.size() - 1;
}

PipelineId RenderGraph::createPipeline(PipelineDesc pipelineDesc)
{
    const auto found = std::find(pipelineDescs_.begin(), pipelineDescs_.end(), pipelineDesc);
    if (found != pipelineDescs_.end())
    {
        return static_cast<PipelineId>(std::distance(pipelineDescs_.begin(), found));
    }
    pipelineDescs_.push_back(std::move(pipelineDesc));
    return pipelineDescs_.size() - 1;
}

void RenderGraph::addExternalGraphicsPass(std::string name, TextureId color, RenderFunction record)
{
    if (!record)
    {
        throw std::invalid_argument("External graphics pass requires a recorder");
    }
    const auto &desc = resources_.texture(color).texture.image.desc;
    if (!(desc.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) ||
        desc.aspectFlags != VK_IMAGE_ASPECT_COLOR_BIT || desc.imageType != ImageType::Image2D ||
        desc.arrayLayers != 1 || desc.mipLevels != 1 || !desc.width || !desc.height ||
        desc.format == VK_FORMAT_UNDEFINED || isDepthFormat(desc.format))
    {
        throw std::invalid_argument("External graphics pass requires a 2D color attachment");
    }
    RenderPass pass;
    pass.name = std::move(name);
    pass.externalGraphics = true;
    pass.pipelineHandle = static_cast<PipelineId>(-1);
    pass.writes.push_back({color, ViewType::Full, 0, VK_ATTACHMENT_LOAD_OP_LOAD});
    pass.renderFunction = std::move(record);
    passes_[currentFrame_].push_back(std::move(pass));
}

VkResult RenderGraph::prepare()
{
    // Builder descriptions may be drafts without attachment formats. Only create
    // the finalized pipelines actually referenced by this frame's passes.
    resources_.pipelines.resize(pipelineDescs_.size());
    for (const auto &pass : passes_[currentFrame_])
    {
        if (pass.externalGraphics)
        {
            continue;
        }
        const auto i = pass.pipelineHandle;
        if (resources_.pipelines[i].handle != VK_NULL_HANDLE)
        {
            continue;
        }
        Pipeline pipeline = device_.createPipeline(pipelineDescs_[i]);
        // Device returns the resolved descriptor/push-constant interface after reflection.
        pipeline.type = pipelineDescs_[i].type;
        resources_.pipelines[i] = std::move(pipeline);
    }

    auto &cache = descriptorCache_[currentFrame_];
    for (size_t index = 0; index < passes_[currentFrame_].size(); ++index)
    {
        auto &pass = passes_[currentFrame_][index];
        if (pass.externalGraphics)
        {
            continue;
        }
        if (index < cache.size() && cache[index].name == pass.name &&
            cache[index].pipelineHandle == pass.pipelineHandle)
        {
            if (!pass.readResourcesDescriptorSet)
            {
                pass.readResourcesDescriptorSet.swap(cache[index].reads);
            }
            if (!pass.uniformsDescriptorSet && !pass.uniforms.empty() &&
                cache[index].uniformName == pass.uniforms.begin()->second.first)
            {
                pass.uniformsDescriptorSet.swap(cache[index].uniforms);
            }
        }
        auto result = createReadResourcesDescriptorSet(pass);
        if (result != VK_SUCCESS)
        {
            return result;
        }

        result = createUniformDescriptorSet(pass);
        if (result != VK_SUCCESS)
        {
            return result;
        }

        updateUniformBuffer(pass);
    }

    return VK_SUCCESS;
}

VkResult RenderGraph::render(Renderer &renderer, VkCommandBuffer commandBuffer, RenderContext &context,
                         const Image &presentImage)
{
    if (!renderer.isFrameInProgress() || commandBuffer != renderer.getCurrentCommandBuffer() ||
        renderer.isRendering())
    {
        throw std::logic_error("Graph execution requires a recording frame outside rendering");
    }

    // Validate every pass before recording barriers or drawing any pass in this graph.
    for (const auto &pass : passes_[currentFrame_])
    {
        if (pass.externalGraphics)
        {
            continue; // External renderers manage their own descriptor bindings.
        }

        const auto &pipeline = resources_.pipeline(pass.pipelineHandle);
        for (const auto &binding : pipeline.desc.bindings)
        {
            VkDescriptorSet set = VK_NULL_HANDLE;
            if (binding.set == kDescriptorSetIndexBindless)
            {
                set = context.bindlessDescriptorSet;
            }
            else if (binding.set == kDescriptorSetIndexView)
            {
                if (currentFrame_ < context.viewDescriptorSets.size())
                {
                    set = context.viewDescriptorSets[currentFrame_];
                }
            }
            else if (binding.set == kDescriptorSetIndexInputResources)
            {
                if (pass.readResourcesDescriptorSet)
                {
                    set = pass.readResourcesDescriptorSet->handle;
                }
            }
            else if (pass.uniformsDescriptorSet && !pass.uniforms.empty())
            {
                const auto uniform = pipeline.namedBindings.find(pass.uniforms.begin()->second.first);
                if (uniform != pipeline.namedBindings.end() && uniform->second.set == binding.set)
                {
                    set = pass.uniformsDescriptorSet->handle;
                }
            }

            if (set == VK_NULL_HANDLE || binding.set >= pipeline.descriptorSetLayouts.size() ||
                pipeline.descriptorSetLayouts[binding.set] == VK_NULL_HANDLE)
            {
                std::cerr << "RenderGraph pass '" << pass.name
                          << "': required descriptor set " << binding.set
                          << " or its layout is missing\n";
                return VK_ERROR_INITIALIZATION_FAILED;
            }
        }
    }

    for (const auto &pass : passes_[currentFrame_])
    {
        device_.beginDebugLabel(commandBuffer, pass.name);
        const auto *pipeline =
            pass.externalGraphics ? nullptr : &resources_.pipeline(pass.pipelineHandle);

        for (const auto &read : pass.reads)
        {
            if (const auto *textureRead = std::get_if<TextureResource>(&read))
            {
                auto &texture = resources_.texture(textureRead->texture);
                texture.previousAccess = vk_sync::imagePipelineBarrier(
                    commandBuffer, texture.texture.image.image,
                    wholeImageRange(texture.texture.image), texture.previousAccess,
                    textureRead->accessType, false);
            }
            else if (const auto *bufferRead = std::get_if<BufferResource>(&read))
            {
                auto &buffer = resources_.buffer(bufferRead->buffer);
                // Intentional: graph.rs/vk-sync prefers a global memory barrier for
                // buffers unless queue ownership transfer is required.
                buffer.previousAccess = vk_sync::globalPipelineBarrier(
                    commandBuffer, buffer.previousAccess, bufferRead->accessType);
            }
        }

        for (const auto &[bufferId, nextAccess] : pass.extraBarriers)
        {
            auto &buffer = resources_.buffer(bufferId);
            buffer.previousAccess =
                vk_sync::globalPipelineBarrier(commandBuffer, buffer.previousAccess, nextAccess);
        }

        std::vector<Attachment> writesForSynchronization = pass.writes;
        if (pass.depthAttachment)
        {
            if (const auto *graphDepth = std::get_if<Attachment>(&*pass.depthAttachment))
            {
                writesForSynchronization.push_back(*graphDepth);
            }
        }
        for (const auto &write : writesForSynchronization)
        {
            auto &texture = resources_.texture(write.texture);
            const auto nextAccess = isDepthFormat(texture.texture.image.desc.format)
                                        ? vk_sync::AccessType::DepthStencilAttachmentReadWrite
                                        : (write.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD
                                               ? vk_sync::AccessType::ColorAttachmentReadWrite
                                               : vk_sync::AccessType::ColorAttachmentWrite);
            texture.previousAccess = vk_sync::imagePipelineBarrier(
                commandBuffer, texture.texture.image.image, wholeImageRange(texture.texture.image),
                texture.previousAccess, nextAccess, false);
        }

        if (pass.presentationPass)
        {
            vk_sync::imagePipelineBarrier(
                commandBuffer, presentImage.image, wholeImageRange(presentImage),
                vk_sync::AccessType::Nothing, vk_sync::AccessType::ColorAttachmentWrite, true);
        }

        std::vector<std::tuple<Image, ViewType, uint32_t, VkAttachmentLoadOp>> colorAttachments;
        for (const auto &write : pass.writes)
        {
            colorAttachments.emplace_back(resources_.texture(write.texture).texture.image,
                                          write.view, write.layer, write.loadOp);
        }

        std::optional<std::tuple<Image, ViewType, uint32_t, VkAttachmentLoadOp>> depthAttachment;
        if (pass.depthAttachment)
        {
            if (const auto *graphDepth = std::get_if<Attachment>(&*pass.depthAttachment))
            {
                depthAttachment.emplace(resources_.texture(graphDepth->texture).texture.image,
                                        graphDepth->view, graphDepth->layer, graphDepth->loadOp);
            }
            else
            {
                const auto &externalDepth =
                    std::get<ExternalDepthAttachment>(*pass.depthAttachment);
                depthAttachment.emplace(externalDepth.image, ViewType::Full, 0,
                                        externalDepth.loadOp);
            }
        }

        VkExtent2D extent{1, 1};
        if (!pass.writes.empty())
        {
            extent = resources_.texture(pass.writes.front().texture).texture.image.extent2D();
        }
        else if (depthAttachment)
        {
            extent = std::get<0>(*depthAttachment).extent2D();
        }

        if (pass.presentationPass)
        {
            colorAttachments.clear();
            colorAttachments.emplace_back(presentImage, ViewType::Full, 0,
                                          VK_ATTACHMENT_LOAD_OP_CLEAR);
            extent = presentImage.extent2D();
        }

        prepareRenderPass(renderer, commandBuffer, pass, colorAttachments, depthAttachment, extent);

        if (pipeline &&
            std::any_of(pipeline->desc.bindings.begin(), pipeline->desc.bindings.end(),
                        [](const auto &b) { return b.set == kDescriptorSetIndexBindless; }))
        {
            bindDescriptorSet(commandBuffer, *pipeline, kDescriptorSetIndexBindless,
                              context.bindlessDescriptorSet);
        }
        if (pipeline &&
            std::any_of(pipeline->desc.bindings.begin(), pipeline->desc.bindings.end(),
                        [](const auto &b) { return b.set == kDescriptorSetIndexView; }))
        {
            bindDescriptorSet(commandBuffer, *pipeline, kDescriptorSetIndexView,
                              context.viewDescriptorSets[currentFrame_]);
        }
        if (pass.readResourcesDescriptorSet)
        {
            bindDescriptorSet(commandBuffer, *pipeline, kDescriptorSetIndexInputResources,
                              pass.readResourcesDescriptorSet->handle);
        }
        if (pass.uniformsDescriptorSet)
        {
            const auto &uniformName = pass.uniforms.begin()->second.first;
            const auto &binding = pipeline->namedBindings.at(uniformName);
            bindDescriptorSet(commandBuffer, *pipeline, binding.set,
                              pass.uniformsDescriptorSet->handle);
        }

        if (pass.renderFunction)
        {
            pass.renderFunction(commandBuffer, pass, resources_);
        }
        if (!pipeline || pipeline->type == PipelineType::Graphics)
        {
            renderer.endRendering(commandBuffer);
        }

        if (pass.copyCommand)
        {
            const auto &copy = *pass.copyCommand;
            auto &source = resources_.texture(copy.source);
            auto &destination = resources_.texture(copy.destination);
            source.previousAccess = vk_sync::imagePipelineBarrier(
                commandBuffer, source.texture.image.image, wholeImageRange(source.texture.image),
                source.previousAccess, vk_sync::AccessType::TransferRead, false);
            destination.previousAccess = vk_sync::imagePipelineBarrier(
                commandBuffer, destination.texture.image.image,
                wholeImageRange(destination.texture.image), destination.previousAccess,
                vk_sync::AccessType::TransferWrite, false);

            VkImageCopy region = copy.region;
            region.srcSubresource.aspectMask = source.texture.image.desc.aspectFlags;
            region.dstSubresource.aspectMask = destination.texture.image.desc.aspectFlags;
            vkCmdCopyImage(commandBuffer, source.texture.image.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination.texture.image.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }

        if (pass.bufferCopyCommand)
        {
            pass.bufferCopyCommand->record(commandBuffer, resources_);
        }
        device_.endDebugLabel(commandBuffer);
    }

    return VK_SUCCESS;
}

} // namespace jhb::rg
