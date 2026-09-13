#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <vector>

namespace jhb::rg
{
[[nodiscard]] bool isDepthFormat(VkFormat format);

enum class ImageType
{
    Image2D,
    Image2DArray,
    CubeMap
};

struct ImageDesc
{
    uint32_t width{1};
    uint32_t height{1};
    uint32_t depth{1};
    uint32_t arrayLayers{1};
    VkFormat format{VK_FORMAT_UNDEFINED};
    ImageType imageType{ImageType::Image2D};
    VkImageAspectFlags aspectFlags{VK_IMAGE_ASPECT_COLOR_BIT};
    VkImageUsageFlags usage{0};
    uint32_t mipLevels{1};
};

struct Image
{
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkImage image{VK_NULL_HANDLE};
    VkImageView imageView{VK_NULL_HANDLE};
    std::vector<VkImageView> layerViews;
    ImageDesc desc{};
    std::string debugName;

    [[nodiscard]] VkImageView viewForLayer(uint32_t layer) const;

    [[nodiscard]] VkExtent2D extent2D() const
    {
        return {desc.width, desc.height};
    }
};

struct Texture
{
    Image image;
    VkSampler sampler{VK_NULL_HANDLE};
    VkDescriptorImageInfo descriptorInfo{};
};

} // namespace jhb::rg
