#pragma once
#include "Camera.h"
#include "GameObject.h"
#include <vulkan/vulkan.h>

namespace jhb {
	constexpr static int MaxLights = 10;
	struct PointLight {
		glm::vec4 position{};
		glm::vec4 color{};
	};

	struct GlobalUbo {
		glm::mat4 projection{1.f};
		glm::mat4 view{1.f};
		glm::mat4 inverseView{1.f};
		glm::vec4 ambientLightColor{1.f, 1.f, 1.f, .02f};
		PointLight pointLights[MaxLights];
		int numLights;
		float exposure = 4.5f;
		float gamma = 2.2f;
	};

	struct PickingUbo {
		int objindex;
	};

	struct FrameInfo
	{
		int frameIndex;
		float frameTime;
		VkCommandBuffer commandBuffer;
		Camera& camera;
		VkDescriptorSet globaldDescriptorSet;
		VkDescriptorSet pbrImageSamplerDescriptorSet;
		VkDescriptorSet skyBoxImageSamplerDecriptorSet;
		VkDescriptorSet shadowMapDescriptorSet;
	};
}

