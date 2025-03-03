#pragma once
#include "BaseRenderSystem.h"

namespace jhb {
	class MousePickingRenderSystem : public BaseRenderSystem
	{
	public:
		MousePickingRenderSystem(Device& device, VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag);
		MousePickingRenderSystem(Device& device, const std::vector<VkDescriptorSetLayout>& globalSetLayOut, const std::string& vert, const std::string& frag);
		~MousePickingRenderSystem();

		MousePickingRenderSystem(const MousePickingRenderSystem&) = delete;
		MousePickingRenderSystem(MousePickingRenderSystem&&) = delete;
		MousePickingRenderSystem& operator=(const MousePickingRenderSystem&) = delete;

	public:
		void renderMousePickedObjToOffscreen(VkCommandBuffer cmd,  const std::vector<VkDescriptorSet>& descriptorsets,
			int frameIndex, Buffer* pickingUbo
		);

	private:
		void createPipeline(VkRenderPass renderPass, const std::string& vert, const std::string& frag) override;
		void createRenderPass();

	public:
		void createOffscreenFrameBuffers();
	public: 
		void destroyOffscreenFrameBuffer();
	public:
		// offscreen with object index info pixels;
		std::vector<VkImage> offscreenImage{SwapChain::MAX_FRAMES_IN_FLIGHT};
		std::vector<VkDeviceMemory> offscreenMemory{SwapChain::MAX_FRAMES_IN_FLIGHT};
		std::vector<VkImageView> offscreenImageView{SwapChain::MAX_FRAMES_IN_FLIGHT};
		std::vector<VkFramebuffer> offscreenFrameBuffer{SwapChain::MAX_FRAMES_IN_FLIGHT};
		VkRenderPass pickingRenderpass;
	};
}
