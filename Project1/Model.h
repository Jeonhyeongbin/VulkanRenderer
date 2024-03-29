#pragma once

#include "Device.h"
#include "Buffer.h"

#define GLM_FORCE_RADIANS // not use degree;
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>

namespace jhb {
	class Model
	{
	public:
		struct Vertex {
			glm::vec3 position{};
			glm::vec3 color{};
			glm::vec3 normal{};
			glm::vec2 uv{};

			// binding은 여러 버텍스 버퍼를 사용할때
			// attribute는 한 버텍스 안에서 메모리구조를 정의할때
			static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
			static std::vector<VkVertexInputAttributeDescription> getAttrivuteDescriptions();

			bool operator==(const Vertex& other) const {
				return position == other.position && color == other.color && normal == other.normal && uv == other.uv;
			}
		};

		struct Builder {
			std::vector<Vertex> vertices{};
			std::vector<uint32_t> indices{};

			void loadModel(const std::string& filepath);
		};

		Model(Device& device, const Builder& builder);
		~Model();

		Model(const Model&) = delete;
		Model& operator=(const Model&) = delete;

		static std::unique_ptr<Model> createModelFromFile(Device& device, const std::string& filepath);

		void draw(VkCommandBuffer buffer);
		void bind(VkCommandBuffer buffer);

		void createVertexBuffer(const std::vector<Vertex>& vertices);
		void createIndexBuffer(const std::vector<uint32_t>& indices);

	private:
		Device& device;

		std::unique_ptr<jhb::Buffer> vertexBuffer;
		uint32_t vertexCount;

		bool hasIndexBuffer = false;
		
		std::unique_ptr<jhb::Buffer> indexBuffer;
		uint32_t indexCount;
	};
}