#pragma once
#include "GBufferPass.h"

namespace jhb
{
class DeferredLightingPass
{
  public:
    struct Inputs
    {
        rg::TextureId brdfLut, irradiance, prefilteredEnvironment, shadowCube;
    };

    DeferredLightingPass(rg::RenderGraph &graph, VkExtent2D extent,
                         std::string name = "deferred-lighting");
    void addPass(const GBuffer &gbuffer, const Inputs &inputs);

    rg::TextureId output() const
    {
        return output_;
    }

  private:
    rg::RenderGraph &graph_;
    std::string name_;
    rg::TextureId output_{};
    rg::PipelineId pipeline_{};
};
} // namespace jhb
