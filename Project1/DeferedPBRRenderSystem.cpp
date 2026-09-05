#include "DeferedPBRRenderSystem.h"

#include <stdexcept>
#include <utility>

namespace jhb {

DeferedPBRRenderSystem::DeferedPBRRenderSystem(
    rg::RenderGraph& graph, VkExtent2D extent, rg::PipelineDesc geometryPipeline,
    VkFormat depthFormat, std::string name)
    : graph_{graph}, name_{std::move(name)} {
    if (extent.width == 0 || extent.height == 0 || name_.empty()) {
        throw std::invalid_argument("Deferred requires a nonzero extent and a resource name");
    }
    if (depthFormat != VK_FORMAT_D32_SFLOAT && depthFormat != VK_FORMAT_D16_UNORM) {
        throw std::invalid_argument("Deferred currently requires a depth-only format");
    }

    // Graph cache is keyed by name. Include resolution to avoid reusing images
    // with an old extent when the caller recreates this renderer after resize.
    const auto resourceName = name_ + "/" + std::to_string(extent.width) + "x" +
                              std::to_string(extent.height) + "/";
    const auto attachment = [&](const char* suffix, VkFormat format, bool depth = false) {
        rg::ImageDesc desc;
        desc.width = extent.width;
        desc.height = extent.height;
        desc.format = format;
        desc.aspectFlags = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        desc.usage = depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                           : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        return graph_.createTexture(resourceName + suffix + "/" + std::to_string(format), desc);
    };

    gbuffer_.position = attachment("position", VK_FORMAT_R16G16B16A16_SFLOAT);
    gbuffer_.normal = attachment("normal", VK_FORMAT_R16G16B16A16_SFLOAT);
    gbuffer_.albedo = attachment("albedo", VK_FORMAT_R16G16B16A16_SFLOAT);
    gbuffer_.material = attachment("material", VK_FORMAT_R16G16B16A16_SFLOAT);
    gbuffer_.emissive = attachment("emissive", VK_FORMAT_R16G16B16A16_SFLOAT);
    gbuffer_.depth = attachment("depth", depthFormat);
    // The inherited lighting shader applies tone mapping/gamma; this is a
    // display-encoded output, not an HDR input for bloom.
    output_ = attachment("lit", VK_FORMAT_R16G16B16A16_SFLOAT);

    geometryPipeline.type = rg::PipelineType::Graphics;
    geometryPipeline.vertexPath = "shaders/deferredGraphGeometry.vert.spv";
    geometryPipeline.fragmentPath = "shaders/deferredGraphGeometry.frag.spv";
    geometryPipeline.computePath.clear();
    geometryPipeline.colorAttachmentFormats.assign(5, VK_FORMAT_R16G16B16A16_SFLOAT);
    geometryPipeline.depthStencilAttachmentFormat = depthFormat;
    geometryPipeline_ = graph_.createPipeline(std::move(geometryPipeline));

    rg::PipelineDesc lightingPipeline;
    lightingPipeline.vertexPath = "shaders/deferedPBR.vert.spv";
    lightingPipeline.fragmentPath = "shaders/deferredGraphLighting.frag.spv";
    lightingPipeline.colorAttachmentFormats = {VK_FORMAT_R16G16B16A16_SFLOAT};
    lightingPipeline_ = graph_.createPipeline(std::move(lightingPipeline));
}

void DeferedPBRRenderSystem::addPasses(
    const LightingInputs& lighting, rg::RenderFunction drawGeometry) {
    if (!drawGeometry) {
        throw std::invalid_argument("Deferred geometry draw callback is required");
    }

    graph_.addPass(name_ + "/geometry", geometryPipeline_)
        .write(gbuffer_.position)
        .write(gbuffer_.normal)
        .write(gbuffer_.albedo)
        .write(gbuffer_.material)
        .write(gbuffer_.emissive)
        .depthAttachment(gbuffer_.depth)
        .render(std::move(drawGeometry))
        .build();

    // Order is the shader's set 2 binding order. read() generates both sampled
    // descriptors and write-to-read image barriers for the G-buffer.
    graph_.addPass(name_ + "/lighting", lightingPipeline_)
        .read(gbuffer_.position)
        .read(gbuffer_.normal)
        .read(gbuffer_.albedo)
        .read(gbuffer_.material)
        .read(gbuffer_.emissive)
        .read(lighting.brdfLut)
        .read(lighting.irradiance)
        .read(lighting.prefilteredEnvironment)
        .read(lighting.shadowCube)
        .write(output_)
        .render([](VkCommandBuffer commandBuffer, const rg::RenderPass&,
                   const rg::GraphResources&) {
            vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        })
        .build();
}

} // namespace jhb
