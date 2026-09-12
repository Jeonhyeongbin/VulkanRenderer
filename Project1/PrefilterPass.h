#pragma once
#include "RenderGraph/RenderGraph.h"

namespace jhb
{
class PrefilterPass
{
  public:
    struct Resources
    {
        rg::TextureId environment, scratch, output;
    };

    void addPass(rg::RenderGraph &graph, const Resources &resources, rg::RenderFunction drawCube,
                 uint32_t sampleCount = 32) const;
};
} // namespace jhb
