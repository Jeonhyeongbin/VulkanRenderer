#include "PrefilterPass.h"
#include "IBLPassUtil.h"
#include <cstddef>

namespace jhb
{
namespace
{
struct PushConstants
{
    glm::mat4 mvp;
    float roughness;
    uint32_t numSamples;
};

static_assert(offsetof(PushConstants, roughness) == 64 &&
              offsetof(PushConstants, numSamples) == 68);
} // namespace

void PrefilterPass::addPass(rg::RenderGraph &graph, const Resources &r, rg::RenderFunction drawCube,
                            uint32_t sampleCount) const
{
    if (!drawCube || sampleCount == 0)
    {
        throw std::invalid_argument("Prefilter requires a cube callback and nonzero sample count");
    }
    ibl::validateCubeFilter(graph, r.environment, r.scratch, r.output,
                            VK_FORMAT_R16G16B16A16_SFLOAT);
    const auto desc = graph.resources().texture(r.output).texture.image.desc;
    const auto matrices = ibl::faceMatrices();
    const auto pipeline = rg::PipelineDesc::builder()
                              .vertexPath("shaders/filtercube.vert.spv")
                              .fragmentPath("shaders/prefilterenvmap.frag.spv")
                              .defaultVertexInput();
    for (uint32_t mip = 0; mip < desc.mipLevels; ++mip)
    {
        const auto size = std::max(1u, desc.width >> mip);
        for (uint32_t face = 0; face < 6; ++face)
        {
            const PushConstants push{matrices[face], ibl::mipRoughness(mip, desc.mipLevels),
                                     sampleCount};
            graph
                .addPassFromDesc("prefilter/" + std::to_string(mip) + "/" + std::to_string(face),
                                 pipeline)
                .read(r.environment)
                .write(r.scratch)
                .render(
                    [push, size, drawCube](VkCommandBuffer cmd, const rg::RenderPass &pass,
                                           const rg::GraphResources &resources)
                    {
                        ibl::setFilterViewport(cmd, size);
                        vkCmdPushConstants(cmd, resources.pipeline(pass.pipelineHandle).layout,
                                           VK_SHADER_STAGE_VERTEX_BIT |
                                               VK_SHADER_STAGE_FRAGMENT_BIT,
                                           0, 72, &push);
                        drawCube(cmd, pass, resources);
                    })
                .copyImage(r.scratch, r.output, ibl::faceCopy(size, face, mip))
                .build();
        }
    }
}
} // namespace jhb
