#include "ShadowPass.h"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace jhb
{
void ShadowPass::addPass(rg::RenderGraph &graph, const Resources &r, const Light &light,
                         rg::RenderFunction drawScene) const
{
    if (!drawScene)
    {
        throw std::invalid_argument("Shadow requires a scene draw callback");
    }
    if (!std::isfinite(light.nearPlane) || !std::isfinite(light.farPlane) ||
        light.nearPlane <= 0.0f || light.farPlane <= light.nearPlane ||
        !std::isfinite(light.position.x) || !std::isfinite(light.position.y) ||
        !std::isfinite(light.position.z))
    {
        throw std::invalid_argument("Shadow requires a finite light and 0 < near < far");
    }

    const auto &cube = graph.resources().texture(r.cube).texture.image.desc;
    const auto &depth = graph.resources().texture(r.depth).texture.image.desc;
    if (cube.imageType != rg::ImageType::CubeMap || cube.arrayLayers != 6 || cube.width == 0 ||
        cube.width != cube.height || cube.format != VK_FORMAT_R32_SFLOAT || cube.mipLevels != 1 ||
        cube.aspectFlags != VK_IMAGE_ASPECT_COLOR_BIT ||
        (cube.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) !=
            (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT) ||
        depth.imageType != rg::ImageType::Image2D || depth.arrayLayers != 1 ||
        depth.width != cube.width || depth.height != cube.height || depth.mipLevels != 1 ||
        depth.aspectFlags != VK_IMAGE_ASPECT_DEPTH_BIT ||
        (depth.format != VK_FORMAT_D32_SFLOAT && depth.format != VK_FORMAT_D16_UNORM) ||
        !(depth.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
    {
        throw std::invalid_argument("Shadow requires an R32F cube and matching depth-only image");
    }

    // Keep the original cube face rotations (+X, -X, +Y, -Y, +Z, -Z).
    const auto rotate = [](float degrees, glm::vec3 axis)
    { return glm::rotate(glm::mat4(1.0f), glm::radians(degrees), axis); };
    const auto flipX = rotate(180.0f, {1, 0, 0});
    const std::array<glm::mat4, 6> views = {rotate(90.0f, {0, 1, 0}) * flipX,
                                            rotate(-90.0f, {0, 1, 0}) * flipX,
                                            rotate(-90.0f, {1, 0, 0}),
                                            rotate(90.0f, {1, 0, 0}),
                                            flipX,
                                            rotate(180.0f, {0, 0, 1})};
    auto projection =
        glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, light.nearPlane, light.farPlane);
    // RenderGraph uses a negative-height viewport; preserve cube sampling orientation.
    projection[1][1] *= -1.0f;
    const auto translation = glm::translate(glm::mat4(1.0f), -light.position);
    const auto pipeline = rg::PipelineDesc::builder()
                              .vertexPath("shaders/shadowOffscreen.vert.spv")
                              .fragmentPath("shaders/shadowOffscreen.frag.spv")
                              .defaultMeshVertexInput()
                              .depthTest(true)
                              .depthWrite(true);
    const VkClearColorValue unoccluded{{std::numeric_limits<float>::max(), 0, 0, 0}};

    for (uint32_t face = 0; face < views.size(); ++face)
    {
        const UniformData uniforms{projection * views[face] * translation,
                                   glm::vec4(light.position, 1.0f)};
        graph.addPassFromDesc("shadow/" + std::to_string(face), pipeline)
            .writeLayer(r.cube, face, unoccluded)
            .depthAttachment(r.depth)
            .uniforms("ubo", uniforms)
            .render(drawScene)
            .build();
    }
}
} // namespace jhb
