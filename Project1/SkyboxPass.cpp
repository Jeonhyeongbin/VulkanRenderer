#include "SkyboxPass.h"
#include <stdexcept>

namespace jhb
{
void SkyboxPass::addPass(rg::RenderGraph &graph, const Resources &r,
                         rg::RenderFunction drawCube) const
{
    if (!drawCube)
    {
        throw std::invalid_argument("Skybox requires a cube draw callback");
    }

    graph
        .addPassFromDesc("skybox", rg::PipelineDesc::builder()
                                       .vertexPath("shaders/skybox.vert.spv")
                                       .fragmentPath("shaders/skybox.frag.spv")
                                       .defaultVertexInput()
                                       .depthTest(true)
                                       .depthWrite(false))
        .read(r.environment)
        .loadWrite(r.color)
        .loadDepthAttachment(r.depth)
        .render(std::move(drawCube))
        .build();
}
} // namespace jhb
