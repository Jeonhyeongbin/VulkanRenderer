#pragma once
#include "RenderGraph/RenderGraph.h"

namespace jhb
{
class IrradiancePass
{
  public:
    struct Resources
    {
        rg::TextureId environment, scratch, output;
    };

    void addPass(rg::RenderGraph &graph, const Resources &resources,
                 rg::RenderFunction drawCube) const;
};
} // namespace jhb
