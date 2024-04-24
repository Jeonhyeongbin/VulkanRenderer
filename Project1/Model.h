#pragma once

#include "Device.h"
#include "Buffer.h"

#define GLM_FORCE_RADIANS // not use degree;
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>

#include "tiny_gltf.h"

namespace jhb {
	struct Vertex {
		glm::vec3 position{};
		glm::vec3 color{};
		glm::vec3 normal{};
		glm::vec2 uv{};
		glm::vec4 tangent;

		// binding은 여러 버텍스 버퍼를 사용할때
		// attribute는 한 버텍스 안에서 메모리구조를 정의할때
		static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
		static std::vector<VkVertexInputAttributeDescription> getAttrivuteDescriptions();

		bool operator==(const Vertex& other) const {
			return position == other.position && color == other.color && normal == other.normal && uv == other.uv && tangent == other.tangent;
		}
	};
	struct Vertex2D {
		glm::vec2 xy;
	};

	// A primitive contains the data for a single draw call
	struct Primitive {
		uint32_t firstIndex;
		uint32_t indexCount;
		int32_t materialIndex;
	};

	// 재질마다 각기 다른 파이프라인을 쓰도록 (비록 바인딩 되는 쉐이더는 같더라도 texture가 달라지므로)
	struct Material {
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		uint32_t baseColorTextureIndex;
		uint32_t normalTextureIndex;
		std::string alphaMode = "OPAQUE";
		float alphaCutOff;
		bool doubleSided = false;
		VkDescriptorSet descriptorSet;
		VkPipeline pipeline;
	};

	struct Mesh {
		std::vector<Primitive> primitives;
	};

	struct Node {
		Node* parent;
		std::vector<Node*> children;
		Mesh mesh;
		glm::mat4 matrix;
		std::string name;
		bool visible = true;
		~Node() {
			for (auto& child : children) {
				delete child;
			}
		}
	};

	// Contains the texture for a single glTF image
	// Images may be reused by texture objects and are as such separated
	struct Image {
		VkImage               image;
		VkImageLayout         imageLayout;
		VkDeviceMemory        deviceMemory;
		VkImageView           view;
		uint32_t              width, height;
		uint32_t              mipLevels;
		uint32_t              layerCount;
		VkDescriptorImageInfo descriptor;
		VkSampler             sampler;

		void loadTexture2D(Device& device, const std::string& filepath);
		void loadKTXTexture(Device& device, const std::string& filepath, VkImageViewType imgViewType = VK_IMAGE_VIEW_TYPE_2D, int arrayCount = 1);
	};

	// A glTF texture stores a reference to the image and a sampler
	struct Texture {
		int32_t imageIndex;
	};


	class Model{
	public:
		Model(Device& device);
		~Model();

		Model(const Model&) = delete;
		Model& operator=(const Model&) = delete;

		Image& getTexture(int idx) {
			return images[idx];
		}

		static std::unique_ptr<Model> createModelFromFile(Device& device, const std::string& Modelfilepath, const std::string& texturefilepath);
		void draw(VkCommandBuffer buffer, uint32_t instancCount = 1);
		void bind(VkCommandBuffer buffer, VkBuffer* instancing = nullptr);

		void createVertexBuffer(const std::vector<Vertex>& vertices);
		void createIndexBuffer(const std::vector<uint32_t>& indices);

	public:
		void loadModel(const std::string& filepath);
		void loadImages(tinygltf::Model& input);
		void loadTextures(tinygltf::Model& input);
		void loadMaterials(tinygltf::Model& input);
		void loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer);
		void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, Node* node);

	private:
		Device& device;

		std::vector<Vertex> vertices;
		std::unique_ptr<jhb::Buffer> vertexBuffer;
		uint32_t vertexCount;

		bool hasIndexBuffer = false;
		
		std::vector<uint32_t> indices;
		std::unique_ptr<jhb::Buffer> indexBuffer;
		uint32_t indexCount;
	private:
		std::vector<Material> materials;
		std::vector<Node*> nodes;
		std::vector<Image> images;
		std::vector<Texture> textures;
	};
}