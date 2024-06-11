#pragma once
#include "BaseRenderSystem.h"

namespace jhb {
	class MousePickingRenderSystem : public BaseRenderSystem
	{
	public:
		MousePickingRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag, const std::vector<VkPushConstantRange>& pushConstanRange);
		~MousePickingRenderSystem();

		MousePickingRenderSystem(const MousePickingRenderSystem&) = delete;
		MousePickingRenderSystem(MousePickingRenderSystem&&) = delete;
		MousePickingRenderSystem& operator=(const MousePickingRenderSystem&) = delete;

	public:
		void renderMousePickedObjToOffscreen(VkCommandBuffer cmd, jhb::GameObject::Map& gameObjects, const std::vector<VkDescriptorSet>& descriptorsets,
			int frameIndex, Buffer* pickingUbo
		);

	private:
		void createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag) override;
	};
}
