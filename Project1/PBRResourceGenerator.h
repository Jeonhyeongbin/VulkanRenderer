#pragma once
#include "BaseRenderSystem.h"

namespace jhb {
	class PBRResourceGenerator : public BaseRenderSystem
	{
	public:
		PBRResourceGenerator(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange);
		~PBRResourceGenerator();

		PBRResourceGenerator(const PBRResourceGenerator&) = delete;
		PBRResourceGenerator(PBRResourceGenerator&&) = delete;
		PBRResourceGenerator& operator=(const PBRResourceGenerator&) = delete;

		void bindPipeline(VkCommandBuffer cmd)
		{
			pipeline->bind(cmd);
		}

		void generateBRDFLUT(VkCommandBuffer cmd, GameObject& gameObject);
		void generateIrradianceCube(VkCommandBuffer cmd, GameObject& gameObject, std::vector<VkDescriptorSet> descSets, const jhb::IrradiencePushBlock& push);
		void generatePrefilteredCube(VkCommandBuffer cmd, GameObject& gameObject, std::vector<VkDescriptorSet> descSets, const jhb::PrefileterPushBlock& push);
	private:
		void createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag) override;
	};
}
