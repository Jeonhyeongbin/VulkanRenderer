#pragma once
#include "RenderGraph/RenderGraph.h"

namespace jhb
{
class ImGuiPass
{
  public:
    // Caller records ImGui with a pipeline configured for this target's format.
    void addPass(rg::RenderGraph &graph, rg::TextureId color, rg::RenderFunction drawUI) const
    {
        graph.addExternalGraphicsPass("imgui", color, std::move(drawUI));
    }
};
} // namespace jhb
