#pragma once

#include "BaseRenderSystem.h"

namespace jhb {
	class BRDFLUTRenderSystem : public BaseRenderSystem {
	public:
		BRDFLUTRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange);
		~BRDFLUTRenderSystem();

		BRDFLUTRenderSystem(const BRDFLUTRenderSystem&) = delete;
		BRDFLUTRenderSystem(BRDFLUTRenderSystem&&) = delete;
		BRDFLUTRenderSystem& operator=(const BRDFLUTRenderSystem&) = delete;

		void bindPipeline(VkCommandBuffer cmd)
		{
			pipeline->bind(cmd);
		}

		void renderBRDFLUTImage(VkCommandBuffer cmd, GameObject& gameObject, std::vector<VkDescriptorSet> descSets);
	private:

		// render pass only used to create pipeline
		// render system doest not store render pass, beacuase render system's life cycle is not tie to render pass
		void createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag) override;
	};
}