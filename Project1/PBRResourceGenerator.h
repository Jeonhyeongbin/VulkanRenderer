#pragma once
#include "BaseRenderSystem.h"

namespace jhb {
	class PBRResourceGenerator
	{
	public:
		PBRResourceGenerator(Device& device, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::vector<VkDescriptorSet>& descSets);
		~PBRResourceGenerator();

		PBRResourceGenerator(const PBRResourceGenerator&) = delete;
		PBRResourceGenerator(PBRResourceGenerator&&) = delete;
		PBRResourceGenerator& operator=(const PBRResourceGenerator&) = delete;

		void createCube();
		void createPBRResource();
		void generateBRDFLUT();
		void generateIrradianceCube();
		void generatePrefilteredCube();

	private:
		Device& device;

		VkImage preFilterCubeImg;
		VkImage IrradianceCubeImg;
		VkImage lutBrdfImg;

		VkDeviceMemory preFilterCubeMemory;
		VkDeviceMemory IrradianceCubeMemory;
		VkDeviceMemory lutBrdfMemory;
	public:
		VkImageView lutBrdfView;
		VkImageView preFilterCubeImgView;
		VkImageView IrradianceCubeImgView;

		VkSampler preFilterCubeSampler;
		VkSampler IrradianceCubeSampler;
		VkSampler lutBrdfSampler;

		std::unique_ptr<GameObject> cube;
		
	private:
		VkPipelineLayout brdfPipelinelayout;
		VkPipelineLayout irradiancePipelinelayout;
		VkPipelineLayout prefilterPipelinelayout;

		std::unique_ptr<Pipeline> brdfPipeline;
		std::unique_ptr<Pipeline> irradiancePipeline;
		std::unique_ptr<Pipeline> prefilterPipeline;

	private:
		std::vector<VkDescriptorSetLayout> descSetlayouts;
		std::vector<VkDescriptorSet> descSets;
	};
}
