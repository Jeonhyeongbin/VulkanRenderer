#pragma once
#include "RenderGraph/RenderGraph.h"

namespace jhb
{
class BRDFLutPass
{
  public:
    void addPass(rg::RenderGraph &graph, rg::TextureId output) const;
};
} // namespace jhb
