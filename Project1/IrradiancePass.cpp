#include "IrradiancePass.h"
#include "IBLPassUtil.h"
#include <cstddef>

namespace jhb
{
namespace
{
struct PushConstants
{
    glm::mat4 mvp;
    float deltaPhi;
    float deltaTheta;
};

static_assert(offsetof(PushConstants, deltaPhi) == 64 && offsetof(PushConstants, deltaTheta) == 68);
} // namespace

void IrradiancePass::addPass(rg::RenderGraph &graph, const Resources &r,
                             rg::RenderFunction drawCube) const
{
    if (!drawCube)
    {
        throw std::invalid_argument("Irradiance requires a cube callback");
    }
    ibl::validateCubeFilter(graph, r.environment, r.scratch, r.output,
                            VK_FORMAT_R32G32B32A32_SFLOAT);
    const auto desc = graph.resources().texture(r.output).texture.image.desc;
    const auto matrices = ibl::faceMatrices();
    const auto pipeline = rg::PipelineDesc::builder()
                              .vertexPath("shaders/filtercube.vert.spv")
                              .fragmentPath("shaders/irradiancecube.frag.spv")
                              .defaultVertexInput();
    for (uint32_t mip = 0; mip < desc.mipLevels; ++mip)
    {
        const auto size = std::max(1u, desc.width >> mip);
        for (uint32_t face = 0; face < 6; ++face)
        {
            const PushConstants push{matrices[face], glm::radians(2.0f),
                                     glm::radians(90.0f) / 64.0f};
            graph
                .addPassFromDesc("irradiance/" + std::to_string(mip) + "/" + std::to_string(face),
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
