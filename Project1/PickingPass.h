#pragma once
#include "RenderGraph/RenderGraph.h"

namespace jhb
{
class PickingPass
{
  public:
    struct Resources
    {
        rg::TextureId ids, depth;
    };

    struct Readback
    {
        rg::BufferId buffer;
        VkOffset2D pixel;
    };

    // Output: R32_UINT, zero = background. Caller owns readback buffers and fence scheduling.
    void addPass(rg::RenderGraph &graph, const Resources &resources, rg::RenderFunction drawObjects,
                 std::optional<Readback> readback = std::nullopt) const;
    // Call before each model draw. IDs for its instances are baseId + gl_InstanceIndex.
    static void pushBaseId(VkCommandBuffer commandBuffer, VkPipelineLayout layout, uint32_t baseId);
    // Window coordinates and image texels both use a top-left origin. No Y flip.
    static std::optional<VkOffset2D> pixel(double x, double y, int windowWidth, int windowHeight,
                                           VkExtent2D extent);
};
} // namespace jhb
