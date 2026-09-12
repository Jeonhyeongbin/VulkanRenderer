#pragma once
#include "RenderGraph/RenderGraph.h"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <algorithm>
#include <stdexcept>

// Shared math/validation only; no resource owner or rendering system wrapper.
namespace jhb::ibl
{
inline void validateCubeFilter(const rg::RenderGraph &graph, rg::TextureId environment,
                               rg::TextureId scratch, rg::TextureId output, VkFormat format)
{
    if (environment == scratch || environment == output || scratch == output)
    {
        throw std::invalid_argument("IBL environment, scratch and output must be distinct");
    }
    const auto &source = graph.resources().texture(environment).texture.image.desc;
    const auto &temp = graph.resources().texture(scratch).texture.image.desc;
    const auto &cube = graph.resources().texture(output).texture.image.desc;
    uint32_t maxMips = 0;
    for (auto size = cube.width; size; size >>= 1)
    {
        ++maxMips;
    }
    if (source.imageType != rg::ImageType::CubeMap || source.arrayLayers != 6 || !source.width ||
        source.width != source.height || !source.mipLevels ||
        source.aspectFlags != VK_IMAGE_ASPECT_COLOR_BIT ||
        !(source.usage & VK_IMAGE_USAGE_SAMPLED_BIT) || cube.imageType != rg::ImageType::CubeMap ||
        cube.arrayLayers != 6 || !cube.width || cube.width != cube.height || !cube.mipLevels ||
        cube.mipLevels > maxMips || cube.format != format ||
        cube.aspectFlags != VK_IMAGE_ASPECT_COLOR_BIT ||
        (cube.usage & (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) !=
            (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT) ||
        temp.imageType != rg::ImageType::Image2D || temp.arrayLayers != 1 || temp.mipLevels != 1 ||
        temp.format != format || temp.aspectFlags != VK_IMAGE_ASPECT_COLOR_BIT ||
        temp.width != cube.width || temp.height != cube.height ||
        (temp.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) !=
            (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
    {
        throw std::invalid_argument("Invalid IBL cube filter resource descriptions");
    }
}

inline std::array<glm::mat4, 6> faceMatrices()
{
    const auto rotate = [](float angle, glm::vec3 axis)
    { return glm::rotate(glm::mat4(1.0f), glm::radians(angle), axis); };
    const auto flip = rotate(180.0f, {1, 0, 0});
    std::array<glm::mat4, 6> matrices = {rotate(90.0f, {0, 1, 0}) * flip,
                                         rotate(-90.0f, {0, 1, 0}) * flip,
                                         rotate(-90.0f, {1, 0, 0}),
                                         rotate(90.0f, {1, 0, 0}),
                                         flip,
                                         rotate(180.0f, {0, 0, 1})};
    const auto projection = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, 0.1f, 512.0f);
    for (auto &matrix : matrices)
    {
        matrix = projection * matrix;
    }
    return matrices;
}

inline float mipRoughness(uint32_t mip, uint32_t mipCount)
{
    return mipCount > 1 ? float(mip) / float(mipCount - 1) : 0.0f;
}

inline VkImageCopy faceCopy(uint32_t size, uint32_t face, uint32_t mip)
{
    VkImageCopy copy{};
    copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, face, 1};
    copy.extent = {size, size, 1};
    return copy;
}

inline void setFilterViewport(VkCommandBuffer cmd, uint32_t size)
{
    // Retain the legacy cube convention: positive viewport, unflipped projection.
    VkViewport viewport{0, 0, float(size), float(size), 0, 1};
    VkRect2D scissor{{0, 0}, {size, size}};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}
} // namespace jhb::ibl
