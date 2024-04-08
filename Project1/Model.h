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
		struct Vertex2D {
			glm::vec2 xy;
		};

		struct ImageBuffer
		{


		};

		struct Builder {
		public:
			Builder(Device&);
			~Builder();

		public:
			std::vector<Vertex> vertices{};
			std::vector<uint32_t> indices{};
			void loadModel(const std::string& filepath);
			void loadTexture2D(const std::string& filepath);
			void loadTextrue3D(const std::vector<std::string>& filepaths);

			void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

		public:
			VkSampler textureSampler;
			VkImageView textureImageview;
		private:
			VkImage textureImage;
			VkDeviceMemory textureImageMemory;
			Device& device;
		};

		Model(Device& device, std::unique_ptr<Builder> builder);
		~Model();

		Model(const Model&) = delete;
		Model& operator=(const Model&) = delete;

		static std::unique_ptr<Model> createModelFromFile(Device& device, const std::string& Modelfilepath, const std::string& texturefilepath);

		void draw(VkCommandBuffer buffer, uint32_t instancCount = 1);
		void bind(VkCommandBuffer buffer, VkBuffer* instancing = nullptr);

		void createVertexBuffer(const std::vector<Vertex>& vertices);
		void createIndexBuffer(const std::vector<uint32_t>& indices);
	public:
		std::unique_ptr<Builder> builder;

	private:
		Device& device;

		std::unique_ptr<jhb::Buffer> vertexBuffer;
		uint32_t vertexCount;

		bool hasIndexBuffer = false;
		
		std::unique_ptr<jhb::Buffer> indexBuffer;
		uint32_t indexCount;
	};
}