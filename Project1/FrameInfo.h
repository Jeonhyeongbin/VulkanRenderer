#pragma once
#include "Camera.h"
#include "GameObject.h"
#include <vulkan/vulkan.h>

namespace jhb {
	struct FrameInfo
	{
		int frameIndex;
		float frameTime;
		VkCommandBuffer commandBuffer;
		Camera& camera;
		VkDescriptorSet globaldDescriptorSet;
		jhb::GameObject::Map& gameObjects; // this make any render system access to all game object;
	};
}

