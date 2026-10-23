#include "DescriptorSet.h"
#include <utility>
#include <map>

namespace jhb::rg
{
VkResult DescriptorSet::allocate(VkDevice device, VkDescriptorSetLayout layout,
                                 const std::vector<VkDescriptorType> &types,
                                 DescriptorSet &output,
                                 VkDescriptorPoolCreateFlags flags)
{
    DescriptorSet result;
    result.device = device;

    std::map<VkDescriptorType, uint32_t> counts;
    for (auto type : types)
    {
        ++counts[type];
    }

    std::vector<VkDescriptorPoolSize> sizes;
    for (auto [type, count] : counts)
    {
        sizes.push_back({type, count});
    }

    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.flags = flags;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
    poolInfo.pPoolSizes = sizes.data();

    auto allocationResult = vkCreateDescriptorPool(device, &poolInfo, nullptr, &result.pool);
    if (allocationResult != VK_SUCCESS)
    {
        return allocationResult;
    }

    VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc.descriptorPool = result.pool;
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts = &layout;

    allocationResult = vkAllocateDescriptorSets(device, &alloc, &result.handle);
    if (allocationResult != VK_SUCCESS)
    {
        return allocationResult;
    }

    output = std::move(result);
    return VK_SUCCESS;
}

void DescriptorSet::writeBuffer(uint32_t binding, const VkDescriptorBufferInfo &info,
                                VkDescriptorType type) const
{
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = handle;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &info;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void DescriptorSet::writeImage(uint32_t binding, const VkDescriptorImageInfo &info,
                               VkDescriptorType type) const
{
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = handle;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &info;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

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
