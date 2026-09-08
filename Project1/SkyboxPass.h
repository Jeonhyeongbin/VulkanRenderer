#pragma once
#include "RenderGraph/RenderGraph.h"

namespace jhb
{
class SkyboxPass
{
  public:
    struct Resources
    {
        rg::TextureId environment, color, depth;
    };

    // Add after opaque geometry. The caller owns the cube and frame resources.
    void addPass(rg::RenderGraph &graph, const Resources &resources,
                 rg::RenderFunction drawCube) const;
};
} // namespace jhb
