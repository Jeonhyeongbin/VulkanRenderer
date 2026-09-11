#pragma once
#include "RenderGraph/RenderGraph.h"
#include "Scene.h"

namespace jhb
{
class PointLightPass
{
  public:
    struct Resources
    {
        rg::TextureId color, depth;
    };

    // Draws opaque light markers, not scene illumination. Captures a frame snapshot.
    void addPass(rg::RenderGraph &graph, const Resources &resources,
                 const std::vector<Scene::PointLightData> &lights) const;
};
} // namespace jhb
