#pragma once

#include "Device.h"
#include "Buffer.h"
#include "Pipeline.h"

#define GLM_FORCE_RADIANS // not use degree;
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>

#include "SwapChain.h"

#define STB_IMAGE_IMPLEMENTATION_WRITE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

namespace jhb {
	struct Vertex {
		glm::vec3 position{};
		glm::vec3 color{};
		glm::vec3 normal{};
		glm::vec2 uv{};
		glm::vec4 tangent{};

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

	struct Sphere {
		glm::vec3 center;
		float radius;

		glm::vec4 mincoordinate{glm::vec3{(std::numeric_limits<float>::max)()}, 1};
		glm::vec4 maxcoordinate{glm::vec3{(std::numeric_limits<float>::min)()}, 1};
	};

	// A primitive contains the data for a single draw call
	struct Primitive {
		uint32_t firstIndex;
		uint32_t indexCount;
		int32_t materialIndex;
	};

	// 재질마다 각기 다른 파이프라인을 쓰도록 (alpha나 재질이 양면일수도 있으므로 )
	struct Material {
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		glm::vec4 emissiveFactor = glm::vec4(1.0f);

		uint32_t baseColorTextureIndex;
		uint32_t normalTextureIndex;
		uint32_t emissiveTextureIndex;
		uint32_t occlusionTextureIndex;
		uint32_t metallicRoughnessTextureIndex;

		float roughnessFactor;
		float metallicFactor;

		std::string alphaMode = "OPAQUE";
		float alphaCutOff;
		bool doubleSided = false;
		std::vector<VkDescriptorSet> descriptorSets{SwapChain::MAX_FRAMES_IN_FLIGHT}; // same type descriptor set for each frame
		std::unique_ptr<class Pipeline> pipeline = nullptr;
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
		void generateMipmap(Device& device, VkImage image, int miplevels, uint32_t width, uint32_t height);
		void updateDescriptor();
	};

	// A glTF texture stores a reference to the image and a sampler
	struct Texture {
		int32_t imageIndex;
	};


	class Model{
	public:
		struct InstanceData {
			glm::vec3 pos;
			glm::vec3 rot;
			float scale{ 0.0f };
			float roughness = 0.0f;
			float metallic = 0.0f;
			float r, g, b;
			float radius;
		};

		Model(Device& device);
		Model(Device& device, glm::mat4 modelMatrix);
		~Model();

		Model(const Model&) = delete;
		Model& operator=(const Model&) = delete;

		Image& getTexture(int idx) {
			return images[idx];
		}

		//static std::unique_ptr<Model> createModelFromFile(Device& device, const std::string& Modelfilepath, const std::string& texturefilepath);
		void draw(VkCommandBuffer buffer, VkPipelineLayout pipelineLayout, int frameIndex);
		void drawNoTexture(VkCommandBuffer buffer, VkPipeline pipeline, VkPipelineLayout pipelineLayout, int frameIndex);
		void drawIndirect(VkCommandBuffer commandBuffer, const Buffer& indirectCommandBuffer, VkPipelineLayout pipelineLayout, int frameIndex);

		void drawInPickPhase(VkCommandBuffer buffer, VkPipelineLayout pipelineLayout, VkPipeline pipeline, int frameIndex);
		void bind(VkCommandBuffer buffer);

		void createVertexBuffer(const std::vector<Vertex>& vertices);
		void createIndexBuffer(const std::vector<uint32_t>& indices);
		void createPipelineForModel(const std::string& vertFilepath, const std::string& fragFilepath, class PipelineConfigInfo& configInfo);
		void createGraphicsPipelinePerMaterial(const std::string& vertFilepath, const std::string& fragFilepath, PipelineConfigInfo& configInfo);

	public:
		void loadModel(const std::string& filepath);
		void loadImages(tinygltf::Model& input);
		void loadTextures(tinygltf::Model& input);
		void loadMaterials(tinygltf::Model& input);
		void loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer);
		void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, Node* node, int frameIndex);
		void buildIndriectNode(Node* node, std::vector<VkDrawIndexedIndirectCommand>& indirectCommandsBuffer);
		void drawNodeNotexture(VkCommandBuffer commandBuffer, VkPipeline pipeline, VkPipelineLayout pipelineLayout, Node* node);
		void buildIndirectCommand(std::vector<VkDrawIndexedIndirectCommand>& indirectCommandBuffer);

		void PickingPhasedrawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, Node* node, int frameIndex, VkPipeline pipeline);
		void calculateTangent(glm::vec2 uv1, glm::vec2 uv2, glm::vec2 uv3, glm::vec3 pos1, glm::vec3 pos2, glm::vec3 pos3, glm::vec4& tangent);
		void createObjectSphere(const std::vector<Vertex> vertices);
		void updateInstanceBuffer(uint32_t _instanceCount, float offsetX, float offsetZ, float roughness =0, float metallic=0);

	public:
		// only for no gftl model
		std::unique_ptr<class Pipeline> noTexturePipeline = nullptr;
	public:

	private:
		Device& device;

		std::vector<Vertex> vertices;
		std::vector<glm::vec3> vertices_p;
		std::unique_ptr<jhb::Buffer> vertexBuffer;
		uint32_t vertexCount;

		bool hasIndexBuffer = false;
		
		std::vector<uint32_t> indices;
		std::unique_ptr<jhb::Buffer> indexBuffer;
		uint32_t indexCount;

	public:
		uint32_t instanceCount = 1;
		std::unique_ptr<Buffer> instanceBuffer = nullptr;
		std::vector<InstanceData> instanceData;

	public:
		std::vector<Material> materials;
		std::vector<Node*> nodes;
		std::vector<Image> images{ Image{} };
		std::vector<Texture> textures;
		std::string path;

	public:
		Sphere sphere;
		glm::mat4 rootModelMatrix{1.f};
		glm::mat4 inverseRootModelMatrix{1.f};
		glm::mat4 pickedObjectRotationMatrix{1.f};
		glm::mat4 prevPickedObjectRotationMatrix{ 1.f };

	public:
		bool hasTangent = false;
	};
}