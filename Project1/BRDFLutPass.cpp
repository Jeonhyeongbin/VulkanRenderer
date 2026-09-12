#include "BRDFLutPass.h"
#include <stdexcept>

namespace jhb
{
void BRDFLutPass::addPass(rg::RenderGraph &graph, rg::TextureId output) const
{
    const auto &d = graph.resources().texture(output).texture.image.desc;
    if (d.format != VK_FORMAT_R16G16_SFLOAT || d.imageType != rg::ImageType::Image2D ||
        d.arrayLayers != 1 || d.mipLevels != 1 || !d.width || !d.height ||
        d.aspectFlags != VK_IMAGE_ASPECT_COLOR_BIT ||
        (d.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) !=
            (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT))
    {
        throw std::invalid_argument("BRDF LUT requires an RG16F color/sampled 2D image");
    }
    graph
        .addPassFromDesc("brdf-lut", rg::PipelineDesc::builder()
                                         .vertexPath("shaders/genbrdflut.vert.spv")
                                         .fragmentPath("shaders/genbrdflut.frag.spv"))
        .write(output)
        .render([](VkCommandBuffer cmd, const rg::RenderPass &, const rg::GraphResources &)
                { vkCmdDraw(cmd, 3, 1, 0, 0); })
        .build();
}
} // namespace jhb
