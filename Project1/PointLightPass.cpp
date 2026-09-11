#include "PointLightPass.h"
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace jhb
{
namespace
{
struct PushConstants
{
    glm::vec4 position;
    glm::vec4 color;
    float radius;
};

static_assert(offsetof(PushConstants, position) == 0 && offsetof(PushConstants, color) == 16 &&
              offsetof(PushConstants, radius) == 32);
// Shader range excludes any trailing C++ alignment padding.
constexpr uint32_t pushSize = 36;
} // namespace

void PointLightPass::addPass(rg::RenderGraph &graph, const Resources &r,
                             const std::vector<Scene::PointLightData> &lights) const
{
    if (lights.empty())
    {
        return;
    }
    std::vector<PushConstants> draws;
    draws.reserve(lights.size());
    for (const auto &light : lights)
    {
        if (!std::isfinite(light.radius) || light.radius <= 0.0f ||
            !std::isfinite(light.intensity) || light.intensity < 0.0f ||
            !std::isfinite(light.position.x) || !std::isfinite(light.position.y) ||
            !std::isfinite(light.position.z) || !std::isfinite(light.color.x) ||
            !std::isfinite(light.color.y) || !std::isfinite(light.color.z))
        {
            throw std::invalid_argument("Point light requires finite data and a positive radius");
        }
        draws.push_back({glm::vec4(light.position, 1.0f), glm::vec4(light.color, light.intensity),
                         light.radius});
    }

    graph
        .addPassFromDesc("point-lights", rg::PipelineDesc::builder()
                                             .vertexPath("shaders/point_light.vert.spv")
                                             .fragmentPath("shaders/point_light.frag.spv")
                                             .depthTest(true)
                                             .depthWrite(true))
        .loadWrite(r.color)
        .loadDepthAttachment(r.depth)
        .render(
            [draws = std::move(draws)](VkCommandBuffer cmd, const rg::RenderPass &pass,
                                       const rg::GraphResources &resources)
            {
                const auto layout = resources.pipeline(pass.pipelineHandle).layout;
                for (const auto &push : draws)
                {
                    vkCmdPushConstants(cmd, layout,
                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                       pushSize, &push);
                    vkCmdDraw(cmd, 6, 1, 0, 0);
                }
            })
        .build();
}
} // namespace jhb
