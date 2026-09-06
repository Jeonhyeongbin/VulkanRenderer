#include "PBRPass.h"
#include <stdexcept>

namespace jhb
{
void PBRPass::addPass(rg::RenderGraph &graph, const Resources &r,
                                rg::RenderFunction drawScene) const
{
    if (!drawScene)
    {
        throw std::invalid_argument("PBR requires a scene draw callback");
    }

    graph
        .addPassFromDesc("pbr", rg::PipelineDesc::builder()
                                    .vertexPath("shaders/pbr.vert.spv")
                                    .fragmentPath("shaders/pbr.frag.spv")
                                    .defaultMeshVertexInput()
                                    .depthTest(true)
                                    .depthWrite(true))
        .read(r.brdfLut)
        .read(r.irradiance)
        .read(r.prefilteredEnvironment)
        .read(r.shadowCube)
        .write(r.color)
        .depthAttachment(r.depth)
        .render(std::move(drawScene))
        .build();
}
} // namespace jhb
