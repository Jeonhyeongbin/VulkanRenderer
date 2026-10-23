#include "Pipeline.h"
#include <algorithm>
#include <stdexcept>
#include "Model.h"

namespace jhb::rg
{
VkResult Pipeline::allocateDescriptorSet(VkDevice device, uint32_t setIndex, DescriptorSet &output,
                                              VkDescriptorPoolCreateFlags flags) const
{
    if (setIndex >= descriptorSetLayouts.size() ||
        descriptorSetLayouts[setIndex] == VK_NULL_HANDLE)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    const auto setLayout = descriptorSetLayouts[setIndex];
    std::vector<VkDescriptorType> types;
    for (const auto &binding : desc.bindings)
    {
        if (binding.set == setIndex)
        {
            types.insert(types.end(), binding.count, binding.type);
        }
    }
    return DescriptorSet::allocate(device, setLayout, types, output, flags);
}

namespace
{
template <typename T, typename Predicate>
bool vectorEquals(const std::vector<T> &left, const std::vector<T> &right, Predicate predicate)
{
    return left.size() == right.size() &&
           std::equal(left.begin(), left.end(), right.begin(), predicate);
}

} // namespace

bool PipelineDesc::operator==(const PipelineDesc &other) const
{
    const bool bindingsEqual = vectorEquals(
        vertexBindings, other.vertexBindings, [](const auto &a, const auto &b)
        { return a.binding == b.binding && a.stride == b.stride && a.inputRate == b.inputRate; });
    const bool attributesEqual =
        vectorEquals(vertexAttributes, other.vertexAttributes,
                     [](const auto &a, const auto &b)
                     {
                         return a.location == b.location && a.binding == b.binding &&
                                a.format == b.format && a.offset == b.offset;
                     });
    return vertexPath == other.vertexPath && fragmentPath == other.fragmentPath &&
           computePath == other.computePath && bindingsEqual && attributesEqual &&
           colorAttachmentFormats == other.colorAttachmentFormats &&
           depthStencilAttachmentFormat == other.depthStencilAttachmentFormat &&
           type == other.type && stateHash == other.stateHash && depthTest == other.depthTest &&
           depthWrite == other.depthWrite && depthCompare == other.depthCompare &&
           cullMode == other.cullMode && bindings == other.bindings &&
           pushConstantSize == other.pushConstantSize &&
           pushConstantStages == other.pushConstantStages && reflectShaders == other.reflectShaders;
}

PipelineDescBuilder &PipelineDescBuilder::defaultVertexInput()
{
    desc_.vertexBindings = {{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}};
    desc_.vertexAttributes = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
                              {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)},
                              {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
                              {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)},
                              {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent)}};
    return *this;
}

PipelineDescBuilder &PipelineDescBuilder::defaultMeshVertexInput()
{
    defaultVertexInput();
    desc_.vertexBindings.push_back({1, sizeof(Model::InstanceData), VK_VERTEX_INPUT_RATE_INSTANCE});
    const std::vector<VkVertexInputAttributeDescription> instanceAttributes = {
        // GLSL mat4 occupies four consecutive vec4 locations (its columns).
        {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Model::InstanceData, transform)},
        {6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Model::InstanceData, transform) + sizeof(glm::vec4)},
        {7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Model::InstanceData, transform) + 2 * sizeof(glm::vec4)},
        {8, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Model::InstanceData, transform) + 3 * sizeof(glm::vec4)},
        {9, 1, VK_FORMAT_R32_SFLOAT, offsetof(Model::InstanceData, roughness)},
        {10, 1, VK_FORMAT_R32_SFLOAT, offsetof(Model::InstanceData, metallic)},
        {11, 1, VK_FORMAT_R32_SFLOAT, offsetof(Model::InstanceData, r)},
        {12, 1, VK_FORMAT_R32_SFLOAT, offsetof(Model::InstanceData, g)},
        {13, 1, VK_FORMAT_R32_SFLOAT, offsetof(Model::InstanceData, b)}};
    desc_.vertexAttributes.insert(desc_.vertexAttributes.end(), instanceAttributes.begin(),
                                  instanceAttributes.end());
    return *this;
}
} // namespace jhb::rg
