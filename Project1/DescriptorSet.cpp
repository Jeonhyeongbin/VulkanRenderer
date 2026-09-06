#include "DescriptorSet.h"
#include <utility>

namespace jhb::rg
{
DescriptorSet::~DescriptorSet()
{
    reset();
}

DescriptorSet::DescriptorSet(DescriptorSet &&b) noexcept
{
    *this = std::move(b);
}

DescriptorSet &DescriptorSet::operator=(DescriptorSet &&b) noexcept
{
    if (this != &b)
    {
        reset();
        device = std::exchange(b.device, VK_NULL_HANDLE);
        handle = std::exchange(b.handle, VK_NULL_HANDLE);
        pool = std::exchange(b.pool, VK_NULL_HANDLE);
    }
    return *this;
}

void DescriptorSet::reset()
{
    if (pool)
    {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
    pool = VK_NULL_HANDLE;
    handle = VK_NULL_HANDLE;
    device = VK_NULL_HANDLE;
}
} // namespace jhb::rg
