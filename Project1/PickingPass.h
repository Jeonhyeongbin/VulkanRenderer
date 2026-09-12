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

    // Output: R32_UINT, zero = background. Readback belongs to the caller.
    void addPass(rg::RenderGraph &graph, const Resources &resources,
                 rg::RenderFunction drawObjects) const;
    // Call before each model draw. IDs for its instances are baseId + gl_InstanceIndex.
    static void pushBaseId(VkCommandBuffer commandBuffer, VkPipelineLayout layout, uint32_t baseId);
};
} // namespace jhb
