#pragma once
#include "RenderGraph/RenderGraph.h"

namespace jhb
{
struct GBuffer
{
    rg::TextureId position, normal, albedo, material, emissive, depth;
};

// A single geometry pass; scene ownership stays with the caller.
class GBufferPass
{
  public:
    GBufferPass(rg::RenderGraph &graph, VkExtent2D extent, rg::PipelineDesc pipeline,
                VkFormat depthFormat = VK_FORMAT_D32_SFLOAT, std::string name = "gbuffer");
    void addPass(rg::RenderFunction drawGeometry);

    const GBuffer &output() const
    {
        return gbuffer_;
    }

  private:
    rg::RenderGraph &graph_;
    std::string name_;
    GBuffer gbuffer_{};
    rg::PipelineId geometryPipeline_{};
};
} // namespace jhb
