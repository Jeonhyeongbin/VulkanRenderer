#include "Image.h"
#include <stdexcept>

namespace jhb::rg
{
bool isDepthFormat(VkFormat f)
{
    return f == VK_FORMAT_D16_UNORM || f == VK_FORMAT_D32_SFLOAT ||
           f == VK_FORMAT_X8_D24_UNORM_PACK32 || f == VK_FORMAT_D24_UNORM_S8_UINT ||
           f == VK_FORMAT_D32_SFLOAT_S8_UINT || f == VK_FORMAT_D16_UNORM_S8_UINT;
}

VkImageView Image::viewForLayer(uint32_t layer) const
{
    if (layer >= layerViews.size())
    {
        throw std::out_of_range("RenderGraph image layer view does not exist");
    }
    return layerViews[layer];
}

} // namespace jhb::rg
