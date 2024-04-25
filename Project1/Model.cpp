
#include <iostream>
#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>


#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT


#include "Utils.hpp"
#include "Model.h"
#include "Device.h"
#include "JHBApplication.h"
#include <ktx.h>
#include <ktxvulkan.h>
#include "BaseRenderSystem.h"

namespace std{
	template <>
	struct hash<jhb::Vertex> {
		size_t operator()(jhb::Vertex const& vertex) const 
		{
			size_t seed = 0;
			jhb::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
			return seed;
		}
	};
}


jhb::Model::Model(Device& _device) : device { _device }
{
	//createVertexBuffer(builder->vertices);
	//createIndexBuffer(builder->indices);
}

jhb::Model::~Model()
{
}

void jhb::Model::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, int frameIndex, uint32_t instanceCount)
{
	if (!nodes.empty())
	{
		for (auto& node : nodes) {
			drawNode(commandBuffer, pipelineLayout, node, frameIndex);
		}
	}
	else {
		if (hasIndexBuffer)
		{
			vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, 0, 0);
		}
		else {
			vkCmdDraw(commandBuffer, vertexCount, instanceCount, 0, 0);
		}
	}
}

void jhb::Model::bind(VkCommandBuffer commandBuffer, VkBuffer* instancing)
{
	VkBuffer buffers[] = { vertexBuffer->getBuffer()};
	VkDeviceSize offsets[] = { 0 };
	// combine command buffer and vertex Buffer
	vkCmdBindVertexBuffers(commandBuffer, 0,  1, buffers, offsets);
	if (instancing)
	{
		vkCmdBindVertexBuffers(commandBuffer, 1, 1, instancing, offsets);
	}

	if (hasIndexBuffer)
	{
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
	}
}

void jhb::Model::createVertexBuffer(const std::vector<Vertex>& vertices)
{
	vertexCount = static_cast<uint32_t>(vertices.size());
	assert(vertexCount >= 3 && "Vertex count must be at least 3");
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
	uint32_t vertexSize = sizeof(vertices[0]);

	// this stagingBuffer user by Buffer Class replace down below secion where using VkBuffer and VkDeviceMemory
	Buffer stagingBuffer{
		device,
		vertexSize,
		vertexCount,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	}; 

	//this section replace down below annotation secion which using vkMapMemory and vkUnmapMemory
	stagingBuffer.map();
	stagingBuffer.writeToBuffer((void*)vertices.data());

	vertexBuffer = std::make_unique<Buffer>(
		device,
		vertexSize,
		vertexCount,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	// copy to staging buffer to vertexbuffer
	device.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize);
}

void jhb::Model::createIndexBuffer(const std::vector<uint32_t>& indices)
{
	indexCount = static_cast<uint32_t>(indices.size());
	hasIndexBuffer = indexCount > 0;
	if (!hasIndexBuffer)
	{
		return;
	}

	VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
	uint32_t indexSize = sizeof(indices[0]);

	Buffer stagingBuffer{
		device,
		indexSize,
		indexCount,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	};

	stagingBuffer.map();
	stagingBuffer.writeToBuffer((void*)indices.data());

	
	indexBuffer = std::make_unique<Buffer>(
		device,
		indexSize,
		indexCount,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	// copy to staging buffer to vertexbuffer
	// staging buffer used to only static data. ex) loading application stage. if data are frequently updated from host, then stop using this
	device.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
}

std::vector<VkVertexInputBindingDescription> jhb::Vertex::getBindingDescriptions()
{
	std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
	bindingDescriptions[0].binding = 0;
	bindingDescriptions[0].stride = sizeof(jhb::Vertex);
	bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> jhb::Vertex::getAttrivuteDescriptions()
{
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions(4);
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location= 0;
	attributeDescriptions[0].format= VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(Vertex, position);

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, color);

	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(Vertex, normal);

	attributeDescriptions[3].binding = 0;
	attributeDescriptions[3].location = 3;
	attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[3].offset = offsetof(Vertex, uv);

	attributeDescriptions[4].binding = 0;
	attributeDescriptions[4].location = 4;
	attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributeDescriptions[4].offset = offsetof(Vertex, tangent);

	return attributeDescriptions;
}

void jhb::Model::loadModel(const std::string& filepath)
{
	tinyobj::attrib_t attr; // position, color, normal, and texture coordinate
	std::vector<tinyobj::shape_t> shapes; //index values for each face element
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;
	// all there are used to store datas readed from wavefrontfile objs

	if (!tinyobj::LoadObj(&attr, &shapes, &materials, &warn, &err, filepath.c_str())) {
		throw std::runtime_error(warn+ err);
	}

	//vertices.clear();
	//indices.clear();

	std::unordered_map<Vertex, int> uniqueVertices{};
	for (const auto& shape : shapes)
	{
		for (const auto& index : shape.mesh.indices)
		{
			Vertex vertex{};

			// if negativ than there are no index 
			if (index.vertex_index >= 0)
			{
				vertex.position = {
					attr.vertices[3 * index.vertex_index + 0],
					attr.vertices[3 * index.vertex_index + 1],
					attr.vertices[3 * index.vertex_index + 2],
				};

				vertex.color = {
					attr.colors[3 * index.vertex_index + 0],
					attr.colors[3 * index.vertex_index + 1],
					attr.colors[3 * index.vertex_index + 2],
				};
			}
			// wavefrontfile doesn't support color value in vertex buffer but tinyobjloader can parse that

			if (index.normal_index >= 0)
			{
				vertex.normal = {
					attr.normals[3 * index.normal_index+ 0],
					attr.normals[3 * index.normal_index+ 1],
					attr.normals[3 * index.normal_index+ 2],
				};
			}

			if (index.texcoord_index >= 0)
			{
				vertex.uv = {
					attr.texcoords[2 * index.texcoord_index + 0],
					attr.texcoords[2 * index.texcoord_index + 1],
				};
			}

			if (uniqueVertices.count(vertex) == 0)
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(uniqueVertices[vertex]);
		}

	}
}

void jhb::Model::loadImages(tinygltf::Model& input)
{
	images.resize(input.images.size());
	for (size_t i = 0; i < input.images.size(); i++) {
		tinygltf::Image& glTFImage = input.images[i];
		images[i].loadKTXTexture(device, glTFImage.uri);
	}
}

void jhb::Model::loadTextures(tinygltf::Model& input)
{
	textures.resize(input.textures.size());
	for (size_t i = 0; i < input.textures.size(); i++) {
		textures[i].imageIndex = input.textures[i].source;
	}
}

void jhb::Model::loadMaterials(tinygltf::Model& input)
{
	materials.resize(input.materials.size());
	for (size_t i = 0; i < input.materials.size(); i++) {
		// We only read the most basic properties required for our sample
		tinygltf::Material glTFMaterial = input.materials[i];
		// Get the base color factor
		if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
			materials[i].baseColorFactor = glm::vec4{ static_cast<float>(*glTFMaterial.values["baseColorFactor"].ColorFactor().data())};
		}
		// Get base color texture index
		if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
			materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
		}
		// Get the normal map texture index
		if (glTFMaterial.additionalValues.find("normalTexture") != glTFMaterial.additionalValues.end()) {
			materials[i].normalTextureIndex = glTFMaterial.additionalValues["normalTexture"].TextureIndex();
		}
		// Get some additional material parameters that are used in this sample
		materials[i].alphaMode = glTFMaterial.alphaMode;
		materials[i].alphaCutOff = (float)glTFMaterial.alphaCutoff;
		materials[i].doubleSided = glTFMaterial.doubleSided;
	}
}

void jhb::Model::loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer)
{
	Node* node = new Node{};
	node->name = inputNode.name;
	node->parent = parent;

	// Get the local node matrix
	// It's either made up from translation, rotation, scale or a 4x4 matrix
	node->matrix = glm::mat4(1.0f);
	if (inputNode.translation.size() == 3) {
		node->matrix = glm::translate(node->matrix, glm::vec3(*inputNode.translation.data()));
	}
	if (inputNode.rotation.size() == 4) {
		glm::quat q = glm::quat{ glm::mat4(*inputNode.rotation.data())};
		node->matrix *= glm::mat4(q);
	}
	if (inputNode.scale.size() == 3) {
		node->matrix = glm::scale(node->matrix, glm::vec3(*inputNode.scale.data()));
	}
	if (inputNode.matrix.size() == 16) {
		node->matrix = glm::mat4(*inputNode.matrix.data());
	};

	// Load node's children
	if (inputNode.children.size() > 0) {
		for (size_t i = 0; i < inputNode.children.size(); i++) {
			loadNode(input.nodes[inputNode.children[i]], input, node, indexBuffer, vertexBuffer);
		}
	}

	// If the node contains mesh data, we load vertices and indices from the buffers
	// In glTF this is done via accessors and buffer views
	if (inputNode.mesh > -1) {
		const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
		// Iterate through all primitives of this node's mesh
		for (size_t i = 0; i < mesh.primitives.size(); i++) {
			const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
			uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
			uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
			uint32_t indexCount = 0;
			// Vertices
			{
				const float* positionBuffer = nullptr;
				const float* normalsBuffer = nullptr;
				const float* texCoordsBuffer = nullptr;
				const float* tangentsBuffer = nullptr;
				size_t vertexCount = 0;

				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					positionBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					vertexCount = accessor.count;
				}
				// Get buffer data for vertex normals
				if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					normalsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				// Get buffer data for vertex texture coordinates
				// glTF supports multiple sets, we only load the first one
				if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					texCoordsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				// POI: This sample uses normal mapping, so we also need to load the tangents from the glTF file
				if (glTFPrimitive.attributes.find("TANGENT") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TANGENT")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					tangentsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				// Append data to model's vertex buffer
				for (size_t v = 0; v < vertexCount; v++) {
					Vertex vert{};
					vert.position = glm::vec4(glm::vec3(positionBuffer[v * 3]), 1.0f);
					vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::vec3(normalsBuffer[v * 3]) : glm::vec3(0.0f)));
					vert.uv = texCoordsBuffer ? glm::vec2(texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
					vert.color = glm::vec3(1.0f);
					vert.tangent = tangentsBuffer ? glm::vec4(tangentsBuffer[v * 4]) : glm::vec4(0.0f);
					vertexBuffer.push_back(vert);
				}
			}
			// Indices
			{
				const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.indices];
				const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

				indexCount += static_cast<uint32_t>(accessor.count);

				// glTF supports different component types of indices
				switch (accessor.componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
					const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
					const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
					const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
					for (size_t index = 0; index < accessor.count; index++) {
						indexBuffer.push_back(buf[index] + vertexStart);
					}
					break;
				}
				default:
					std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
					return;
				}
			}

			Primitive primitive{};
			primitive.firstIndex = firstIndex;
			primitive.indexCount = indexCount;
			primitive.materialIndex = glTFPrimitive.material;
			node->mesh.primitives.push_back(primitive);
		}
	}

	if (parent) {
		parent->children.push_back(node);
	}
	else {
		nodes.push_back(node);
	}
}

void jhb::Model::drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, Node* node, int frameIndex)
{
	if (!node->visible) {
		return;
	}
	if (node->mesh.primitives.size() > 0) {
		// Pass the node's matrix via push constants
		// Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
		glm::mat4 nodeMatrix = node->matrix;
		Node* currentParent = node->parent;
		while (currentParent) {
			nodeMatrix = currentParent->matrix * nodeMatrix;
			currentParent = currentParent->parent;
		}
		// Pass the final matrix to the vertex shader using push constants
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), sizeof(glm::mat4), &nodeMatrix);
		for (Primitive& primitive : node->mesh.primitives) {
			if (primitive.indexCount > 0) {
				Material& material = materials[primitive.materialIndex];
				// POI: Bind the pipeline for the node's material
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 4, 1, &material.descriptorSets[frameIndex], 0, nullptr);
				vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
			}
		}
	}
	for (auto& child : node->children) {
		drawNode(commandBuffer, pipelineLayout, child, frameIndex);
	}
}

void jhb::Image::loadTexture2D(Device& device, const std::string& filepath)
{
	//int texWidth, texHeight, texChannels;
	//if (filepath.size() <= 0)
	//	return;

	//float* pixels = stbi_loadf(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	//VkDeviceSize imageSize = texWidth * texHeight * texChannels * 4; // 4byte per pixel

	//if (!pixels) {
	//	throw std::runtime_error("failed to load texture image!");
	//}

	//VkBuffer stagingBuffer;
	//VkDeviceMemory stagingBufferMemory;

	//device.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	//void* data;
	//vkMapMemory(device.getLogicalDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
	//memcpy(data, pixels, static_cast<size_t>(imageSize));
	//vkUnmapMemory(device.getLogicalDevice(), stagingBufferMemory);

	//stbi_image_free(pixels);

	//VkImageCreateInfo imageInfo{};
	//imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	//imageInfo.imageType = VK_IMAGE_TYPE_2D;
	//imageInfo.extent.width = static_cast<uint32_t>(texWidth);
	//imageInfo.extent.height = static_cast<uint32_t>(texHeight);
	//imageInfo.extent.depth = 1;
	//imageInfo.mipLevels = 1;
	//imageInfo.arrayLayers = 1;
	//imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	//imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	//imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	//imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	//imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	//imageInfo.flags = 0; // Optional
	//device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, deviceMemory);

	//VkImageSubresourceRange subresourceRange = {};
	//subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//subresourceRange.baseMipLevel = 0;
	//subresourceRange.levelCount = 1;
	//subresourceRange.layerCount = 1;

	//VkCommandBuffer commandBuffer = device.beginSingleTimeCommands();
	//device.transitionImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
	//device.endSingleTimeCommands(commandBuffer);

	//device.copyBufferToImage(stagingBuffer, image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1);

	//commandBuffer = device.beginSingleTimeCommands();
	//device.transitionImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);
	//device.endSingleTimeCommands(commandBuffer);

	//// create image view and image sampler
	//VkImageViewCreateInfo viewInfo{};
	//viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	//viewInfo.image = image;
	//viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	//viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	//viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//viewInfo.subresourceRange.baseMipLevel = 0;
	//viewInfo.subresourceRange.levelCount = 1;
	//viewInfo.subresourceRange.baseArrayLayer = 0;
	//viewInfo.subresourceRange.layerCount = 1;

	//if (vkCreateImageView(device.getLogicalDevice(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
	//	throw std::runtime_error("failed to create texture image view!");
	//}

	//VkSamplerCreateInfo samplerInfo{};
	//samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	//samplerInfo.magFilter = VK_FILTER_LINEAR;
	//samplerInfo.minFilter = VK_FILTER_LINEAR;

	//samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	//samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	//samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

	//VkPhysicalDeviceProperties properties{};
	//vkGetPhysicalDeviceProperties(device.getPhysicalDevice(), &properties);
	//samplerInfo.anisotropyEnable = VK_FALSE;
	//samplerInfo.maxAnisotropy = 0;
	//samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	//samplerInfo.unnormalizedCoordinates = VK_FALSE;
	//samplerInfo.compareEnable = VK_FALSE;
	//samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	//samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	//samplerInfo.mipLodBias = 0.0f;
	//samplerInfo.minLod = 0.0f;
	//samplerInfo.maxLod = 1;

	//if (vkCreateSampler(device.getLogicalDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
	//	throw std::runtime_error("failed to create texture sampler!");
	//}
}

void jhb::Image::loadKTXTexture(Device& device, const std::string& filepath, VkImageViewType imgViewType, int arrayCount)
{
	if (filepath.size() <= 0)
		return;

	ktxTexture* ktxTexture;
	ktxResult result = ktxTexture_CreateFromNamedFile(filepath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
	assert(result == KTX_SUCCESS);

	uint32_t width = ktxTexture->baseWidth;
	uint32_t height = ktxTexture->baseHeight;
	uint32_t mipLevels = ktxTexture->numLevels;

	ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
	ktx_size_t ktxTextureSize = ktxTexture_GetDataSize(ktxTexture);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	device.createBuffer(ktxTextureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(device.getLogicalDevice(), stagingBufferMemory, 0, ktxTextureSize, 0, &data);
	memcpy(data, ktxTextureData, ktxTextureSize);
	vkUnmapMemory(device.getLogicalDevice(), stagingBufferMemory);

	// Setup buffer copy regions for each face including all of its mip levels
	std::vector<VkBufferImageCopy> bufferCopyRegions;
	for (uint32_t face = 0; face < arrayCount; face++)
	{
		for (uint32_t level = 0; level < mipLevels; level++)
		{
			ktx_size_t offset;
			KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, level, 0, face, &offset);
			assert(result == KTX_SUCCESS);

			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = level;
			bufferCopyRegion.imageSubresource.baseArrayLayer = face;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> level;
			bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> level;
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;

			bufferCopyRegions.push_back(bufferCopyRegion);
		}
	}

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipLevels;
	imageInfo.arrayLayers = arrayCount;
	imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; // Optional
	device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, deviceMemory);

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = mipLevels;
	subresourceRange.layerCount = arrayCount;

	VkCommandBuffer commandBuffer = device.beginSingleTimeCommands();
	device.transitionImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

	vkCmdCopyBufferToImage(
		commandBuffer,
		stagingBuffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		static_cast<uint32_t>(bufferCopyRegions.size()),
		bufferCopyRegions.data());

	device.transitionImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);
	device.endSingleTimeCommands(commandBuffer);
	//generateMipmaps(textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mipLevels);

	// create image view and image sampler
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = imgViewType;
	viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = arrayCount;

	if (vkCreateImageView(device.getLogicalDevice(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture image view!");
	}

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;

	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(device.getPhysicalDevice(), &properties);
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 0;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = static_cast<float>(mipLevels);

	if (vkCreateSampler(device.getLogicalDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}
