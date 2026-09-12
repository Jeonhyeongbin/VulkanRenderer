#include "PickingPass.h"
#include <stdexcept>

namespace jhb
{
void PickingPass::addPass(rg::RenderGraph &graph, const Resources &r,
                          rg::RenderFunction drawObjects) const
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
    graph
        .addPassFromDesc("picking", rg::PipelineDesc::builder()
                                        .vertexPath("shaders/picking.vert.spv")
                                        .fragmentPath("shaders/picking.frag.spv")
                                        .defaultMeshVertexInput()
                                        .depthTest(true)
                                        .depthWrite(true))
        .write(r.ids)
        .depthAttachment(r.depth)
        .render(std::move(drawObjects))
        .build();
}

void PickingPass::pushBaseId(VkCommandBuffer commandBuffer, VkPipelineLayout layout,
                             uint32_t baseId)
{
    if (baseId == 0)
    {
        throw std::invalid_argument("Picking ID zero is reserved for background");
    }
    vkCmdPushConstants(commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 64, sizeof(baseId),
                       &baseId);
}
} // namespace jhb
