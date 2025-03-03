#pragma once
#include "BaseRenderSystem.h"
namespace jhb {
	class ComputerShadeSystem 
	{
		struct InstanceBoundedSphere {
			glm::vec3 center;
			float radius;
		};

		struct UniformData {
			glm::mat4 projection;
			glm::mat4 view;
			glm::vec4 frustumPlanes[6];
		} uniformData;

		class frustum
		{
		public:
			enum side { LEFT = 0, RIGHT = 1, TOP = 2, BOTTOM = 3, BACK = 4, FRONT = 5 };
			std::array<glm::vec4, 6> planes;

			void update(glm::mat4 matrix)
			{
				planes[LEFT].x = matrix[0].w + matrix[0].x;
				planes[LEFT].y = matrix[1].w + matrix[1].x;
				planes[LEFT].z = matrix[2].w + matrix[2].x;
				planes[LEFT].w = matrix[3].w + matrix[3].x;

				planes[RIGHT].x = matrix[0].w - matrix[0].x;
				planes[RIGHT].y = matrix[1].w - matrix[1].x;
				planes[RIGHT].z = matrix[2].w - matrix[2].x;
				planes[RIGHT].w = matrix[3].w - matrix[3].x;

				planes[TOP].x = matrix[0].w - matrix[0].y;
				planes[TOP].y = matrix[1].w - matrix[1].y;
				planes[TOP].z = matrix[2].w - matrix[2].y;
				planes[TOP].w = matrix[3].w - matrix[3].y;

				planes[BOTTOM].x = matrix[0].w + matrix[0].y;
				planes[BOTTOM].y = matrix[1].w + matrix[1].y;
				planes[BOTTOM].z = matrix[2].w + matrix[2].y;
				planes[BOTTOM].w = matrix[3].w + matrix[3].y;

				planes[BACK].x = matrix[0].w + matrix[0].z;
				planes[BACK].y = matrix[1].w + matrix[1].z;
				planes[BACK].z = matrix[2].w + matrix[2].z;
				planes[BACK].w = matrix[3].w + matrix[3].z;

				planes[FRONT].x = matrix[0].w - matrix[0].z;
				planes[FRONT].y = matrix[1].w - matrix[1].z;
				planes[FRONT].z = matrix[2].w - matrix[2].z;
				planes[FRONT].w = matrix[3].w - matrix[3].z;

				for (auto i = 0; i < planes.size(); i++)
				{
					float length = sqrtf(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
					planes[i] /= length;
				}
			}

			bool checkSphere(glm::vec3 pos, float radius)
			{
				for (auto i = 0; i < planes.size(); i++)
				{
					if ((planes[i].x * pos.x) + (planes[i].y * pos.y) + (planes[i].z * pos.z) + planes[i].w <= -radius)
					{
						return false;
					}
				}
				return true;
			}
		};

	public:
		ComputerShadeSystem(Device& device);
		~ComputerShadeSystem();
		
	private:
		void createPipeLineLayoutAndPipeline();
		void BuildComputeCommandBuffer();

		// indirect command 는 cpu에서 업데이트 할 매개체 버퍼
		// indricetCommands를 mapping을 통헤 Gpu메모리에 실제 올릴 IndirectCommandBuffer에 값을 씀.


		// computeshader 초기화 -> helmet이나 다른 게임 오브젝트들에서  여기 indirectCommands에 VkDrawIndexedIndirectCommand추가 .
	public:
		void SetupDescriptor();
		void UpdateUniform(uint32_t framIndex, glm::mat4 view, glm::mat4 projection);
	private:
		Device& device;

		std::vector<std::unique_ptr<Buffer>> IndirectCommandBuffer;
		std::vector<std::unique_ptr<Buffer>> uboBuffer;
		std::unique_ptr<Buffer> instanceBuffer;

		std::vector<VkDrawIndexedIndirectCommand> indirectCommands;

		VkPipeline pipeline;
		VkPipelineLayout pipelinelayout;

		std::vector<std::unique_ptr<Buffer>> computeUboBuffer;

		std::vector<VkDescriptorSet> descriptorSet;

		std::array<std::unique_ptr<DescriptorPool>, 3> discriptorPool{};

		std::unique_ptr<DescriptorPool> computeDescriptorPool;
		std::unique_ptr<DescriptorSetLayout> computeDescriptorSetLayout;
		
		VkShaderModule computeShader;
	public:
		uint32_t ojectCount;
		std::vector<VkCommandBuffer> computeCommandBuffers;
	};
}

