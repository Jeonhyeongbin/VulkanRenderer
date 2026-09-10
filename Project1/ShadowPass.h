#pragma once
#include "RenderGraph/RenderGraph.h"
#include <glm/glm.hpp>

namespace jhb
{
class ShadowPass
{
  public:
    struct Resources
    {
        rg::TextureId cube, depth;
    };

    struct Light
    {
        glm::vec3 position{0.0f};
        float nearPlane{0.1f};
        float farPlane{1024.0f};
    };

    struct UniformData
    {
        glm::mat4 viewProjection;
        glm::vec4 lightPosition;
    };

    // Declares six faces; the caller owns images and supplies shadow-casting geometry.
    void addPass(rg::RenderGraph &graph, const Resources &resources, const Light &light,
                 rg::RenderFunction drawScene) const;
};
} // namespace jhb
