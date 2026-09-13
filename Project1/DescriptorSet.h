#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace jhb::rg
{
constexpr uint32_t kDescriptorSetIndexBindless = 0;
constexpr uint32_t kDescriptorSetIndexView = 1;
constexpr uint32_t kDescriptorSetIndexInputResources = 2;

struct DescriptorBinding
{
    uint32_t set{0};
    uint32_t binding{0};
    VkDescriptorType type{VK_DESCRIPTOR_TYPE_MAX_ENUM};
};

struct DescriptorSet
{
    VkDevice device{VK_NULL_HANDLE};
    VkDescriptorSet handle{VK_NULL_HANDLE};
    VkDescriptorPool pool{VK_NULL_HANDLE};

    // Owns its pool and set, but borrows a pipeline-owned layout.
    static DescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout,
                                  const std::vector<VkDescriptorType> &types,
                                  VkDescriptorPoolCreateFlags flags = 0);
    void writeBuffer(uint32_t binding, const VkDescriptorBufferInfo &info,
                     VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) const;
    void writeImage(uint32_t binding, const VkDescriptorImageInfo &info,
                    VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) const;

    DescriptorSet() = default;
    ~DescriptorSet();
    DescriptorSet(const DescriptorSet &) = delete;
    DescriptorSet &operator=(const DescriptorSet &) = delete;
    DescriptorSet(DescriptorSet &&other) noexcept;
    DescriptorSet &operator=(DescriptorSet &&other) noexcept;
    void reset();
};
} // namespace jhb::rg
