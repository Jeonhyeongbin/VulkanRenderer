#pragma once
#include <vulkan/vulkan.h>

namespace jhb::rg
{
struct DescriptorSet
{
    VkDevice device{VK_NULL_HANDLE};
    VkDescriptorSet handle{VK_NULL_HANDLE};
    VkDescriptorPool pool{VK_NULL_HANDLE};

    DescriptorSet() = default;
    ~DescriptorSet();
    DescriptorSet(const DescriptorSet &) = delete;
    DescriptorSet &operator=(const DescriptorSet &) = delete;
    DescriptorSet(DescriptorSet &&other) noexcept;
    DescriptorSet &operator=(DescriptorSet &&other) noexcept;
    void reset();
};
} // namespace jhb::rg
