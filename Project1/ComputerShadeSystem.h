#pragma once
#include "BaseRenderSystem.h"
namespace jhb {
	class ComputerShadeSystem 
	{
	public:
		ComputerShadeSystem(Device& device);
		~ComputerShadeSystem();
		

		void createPipeLineLayoutAndPipeline();

		void BuildComputeCommandBuffer();
		// indirect command 는 cpu에서 업데이트 할 매개체 버퍼
		// indricetCommands를 mapping을 통헤 Gpu메모리에 실제 올릴 IndirectCommandBuffer에 값을 씀.
		void SetupDescriptor();
	private:
		Device& device;
		std::unique_ptr<Buffer> IndirectCommandBuffer;
		std::vector<VkDrawIndexedIndirectCommand> indirectCommands;

		std::vector<VkSemaphore> computeSemaphores;
		std::vector<VkFence> icomputeFences;

		VkPipeline pipeline;
		VkPipelineLayout pipelinelayout;

		std::unique_ptr<Buffer> uboBuffer;

		VkDescriptorSet cullingDescriptorSet;
		VkDescriptorSet uboDescriptorSet;

		std::unique_ptr<DescriptorPool> computeDescriptorPool;
		std::unique_ptr<DescriptorSetLayout> computeDescriptorSetLayout;
		
		VkShaderModule computeShader;
	};
}

