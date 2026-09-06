#include "DeferredLightingPass.h"
#include <stdexcept>

namespace jhb
{
DeferredLightingPass::DeferredLightingPass(rg::RenderGraph &graph, VkExtent2D extent,
                                           std::string name)
    : graph_{graph}, name_{std::move(name)}
{
    if (!extent.width || !extent.height)
    {
        throw std::invalid_argument("Invalid lighting extent");
    }
    rg::ImageDesc image;
    image.width = extent.width;
    image.height = extent.height;
    image.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    output_ = graph_.createTexture(
        name_ + "/" + std::to_string(extent.width) + "x" + std::to_string(extent.height), image);
    rg::PipelineDesc pipeline;
    pipeline.vertexPath = "shaders/deferedPBR.vert.spv";
    pipeline.fragmentPath = "shaders/deferredGraphLighting.frag.spv";
    pipeline.colorAttachmentFormats = {image.format};
    pipeline.bindings = {
        {1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL_GRAPHICS}};
    for (uint32_t i = 0; i < 9; ++i)
    {
        pipeline.bindings.push_back(
            {rg::kDescriptorSetIndexInputResources, i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});
    }
    pipeline_ = graph_.createPipeline(std::move(pipeline));
}

void DeferredLightingPass::addPass(const GBuffer &g, const Inputs &inputs)
{
    graph_.addPass(name_, pipeline_)
        .read(g.position)
        .read(g.normal)
        .read(g.albedo)
        .read(g.material)
        .read(g.emissive)
        .read(inputs.brdfLut)
        .read(inputs.irradiance)
        .read(inputs.prefilteredEnvironment)
        .read(inputs.shadowCube)
        .write(output_)
        .render([](VkCommandBuffer cb, const rg::RenderPass &, const rg::GraphResources &)
                { vkCmdDraw(cb, 3, 1, 0, 0); })
        .build();
}
} // namespace jhb
