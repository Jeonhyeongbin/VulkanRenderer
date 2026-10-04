#include "PickingPass.h"
#include <stdexcept>
#include <cmath>

namespace jhb
{
std::optional<VkOffset2D> PickingPass::pixel(double x, double y, int windowWidth, int windowHeight,
                                             VkExtent2D extent)
{
    if (!std::isfinite(x) || !std::isfinite(y) || windowWidth <= 0 || windowHeight <= 0 ||
        extent.width == 0 || extent.height == 0 || x < 0 || y < 0 || x >= windowWidth ||
        y >= windowHeight)
    {
        return std::nullopt;
    }
    return VkOffset2D{static_cast<int32_t>(x * extent.width / windowWidth),
                      static_cast<int32_t>(y * extent.height / windowHeight)};
}

void PickingPass::addPass(rg::RenderGraph &graph, const Resources &r,
                          rg::RenderFunction drawObjects, std::optional<Readback> readback) const
{
    if (!drawObjects)
    {
        throw std::invalid_argument("Picking requires an object draw callback");
    }
    const auto &ids = graph.resources().texture(r.ids).texture.image.desc;
    const auto &depth = graph.resources().texture(r.depth).texture.image.desc;
    if (ids.format != VK_FORMAT_R32_UINT || ids.imageType != rg::ImageType::Image2D ||
        ids.arrayLayers != 1 || ids.mipLevels != 1 || ids.width == 0 || ids.height == 0 ||
        ids.aspectFlags != VK_IMAGE_ASPECT_COLOR_BIT ||
        (ids.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) !=
            (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT) ||
        depth.width != ids.width || depth.height != ids.height ||
        depth.imageType != rg::ImageType::Image2D || depth.arrayLayers != 1 ||
        depth.mipLevels != 1 || depth.aspectFlags != VK_IMAGE_ASPECT_DEPTH_BIT ||
        (depth.format != VK_FORMAT_D32_SFLOAT && depth.format != VK_FORMAT_D16_UNORM) ||
        !(depth.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
    {
        throw std::invalid_argument(
            "Picking requires an R32_UINT transfer-source target and matching depth");
    }
    auto pass = graph.addPassFromDesc("picking", rg::PipelineDesc::builder()
                                                     .vertexPath("shaders/picking.vert.spv")
                                                     .fragmentPath("shaders/picking.frag.spv")
                                                     .defaultMeshVertexInput()
                                                     .depthTest(true)
                                                     .depthWrite(true));
    pass.write(r.ids).depthAttachment(r.depth).render(std::move(drawObjects));
    if (readback)
    {
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageOffset = {readback->pixel.x, readback->pixel.y, 0};
        region.imageExtent = {1, 1, 1};
        pass.copyImageToBuffer(r.ids, readback->buffer, region, vk_sync::AccessType::HostRead);
    }
    pass.build();
}

void PickingPass::pushBaseId(VkCommandBuffer commandBuffer, VkPipelineLayout layout,
                             uint32_t baseId)
{
    if (baseId == 0)
    {
        throw std::invalid_argument("Picking ID zero is reserved for background");
    }
    vkCmdPushConstants(commandBuffer, layout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 80, sizeof(baseId),
                       &baseId);
}
} // namespace jhb
