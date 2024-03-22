#pragma once
#include "Camera.h"

#include <vulkan/vulkan.h>

namespace jhb {
	struct FrameInfo
	{
		int frameIndex;
		float frameTime;
		VkCommandBuffer commandBuffer;
		Camera& camera;
		VkDescriptorSet globaldDescriptorSet;
	};
}

