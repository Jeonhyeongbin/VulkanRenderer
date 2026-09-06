#pragma once
#include "RenderGraph/RenderGraph.h"

namespace jhb
{
class PresentPass
{
  public:
    PresentPass(rg::RenderGraph &graph, VkFormat format) : graph_{graph}
    {
        rg::PipelineDesc desc;
        desc.vertexPath = "shaders/deferedPBR.vert.spv";
        desc.fragmentPath = "shaders/graphPresent.frag.spv";
        desc.colorAttachmentFormats = {format};
        desc.bindings = {
            {rg::kDescriptorSetIndexInputResources, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER}};
        pipeline_ = graph_.createPipeline(std::move(desc));
    }

    void addPass(rg::TextureId source)
    {
        graph_.addPass("present", pipeline_)
            .read(source)
            .presentationPass()
            .render([](VkCommandBuffer cb, const rg::RenderPass &, const rg::GraphResources &)
                    { vkCmdDraw(cb, 3, 1, 0, 0); })
            .build();
    }

  private:
    rg::RenderGraph &graph_;
    rg::PipelineId pipeline_;
};
} // namespace jhb
