#pragma once
#include "RenderGraph/RenderGraph.h"

namespace jhb
{
class PBRPass
{
  public:
    struct Resources
    {
        rg::TextureId brdfLut, irradiance, prefilteredEnvironment, shadowCube;
        rg::TextureId color, depth;
    };

    // The caller owns resource creation and resize. This class only declares the pass.
    void addPass(rg::RenderGraph &graph, const Resources &resources,
                 rg::RenderFunction drawScene) const;
};
} // namespace jhb
