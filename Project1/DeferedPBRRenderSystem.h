#pragma once

#include "RenderGraph/RenderGraph.h"

namespace jhb {

// Registers deferred passes into the caller's frame graph.
// Scene ownership, frame submission and presentation belong to the caller.
class DeferedPBRRenderSystem {
public:
    struct GBuffer {
        rg::TextureId position, normal, albedo, material, emissive, depth;
    };

    struct LightingInputs {
        rg::TextureId brdfLut;
        rg::TextureId irradiance;
        rg::TextureId prefilteredEnvironment;
        rg::TextureId shadowCube;
    };

    // vertexBindings/vertexAttributes must match the scene's mesh/instance data.
    // The Device pipeline factory must enable depth test/write for geometry and
    // disable both for lighting. Geometry uses a vertex mat4 push constant.
    DeferedPBRRenderSystem(rg::RenderGraph& graph, VkExtent2D extent,
                           rg::PipelineDesc geometryPipeline,
                           VkFormat depthFormat = VK_FORMAT_D32_SFLOAT,
                           std::string name = "deferred");

    // Call once after graph.newFrame()/clear(). The callback records mesh draws
    // inside the already-open geometry pass. It may bind material sets at set 3
    // and push model matrices, but must not begin/end rendering.
    // Capture frame/scene data with a lifetime covering graph.render().
    void addPasses(const LightingInputs& lighting, rg::RenderFunction drawGeometry);

    [[nodiscard]] const GBuffer& gbuffer() const { return gbuffer_; }
    [[nodiscard]] rg::TextureId output() const { return output_; }

private:
    rg::RenderGraph& graph_;
    std::string name_;
    GBuffer gbuffer_{};
    rg::TextureId output_{};
    rg::PipelineId geometryPipeline_{};
    rg::PipelineId lightingPipeline_{};
};

} // namespace jhb
