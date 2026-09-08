#include "RenderGraph/RenderGraph.h"
#include "Model.h"

namespace jhb::rg
{
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
        {5, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Model::InstanceData, pos)},
        {6, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Model::InstanceData, rot)},
        {7, 1, VK_FORMAT_R32_SFLOAT, offsetof(Model::InstanceData, scale)},
        {8, 1, VK_FORMAT_R32_SFLOAT, offsetof(Model::InstanceData, roughness)},
        {9, 1, VK_FORMAT_R32_SFLOAT, offsetof(Model::InstanceData, metallic)},
        {10, 1, VK_FORMAT_R32_SFLOAT, offsetof(Model::InstanceData, r)},
        {11, 1, VK_FORMAT_R32_SFLOAT, offsetof(Model::InstanceData, g)},
        {12, 1, VK_FORMAT_R32_SFLOAT, offsetof(Model::InstanceData, b)}};
    desc_.vertexAttributes.insert(desc_.vertexAttributes.end(), instanceAttributes.begin(),
                                  instanceAttributes.end());
    return *this;
}
} // namespace jhb::rg
