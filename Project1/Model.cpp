
#include <iostream>
#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include "Utils.hpp"
#include "Model.h"

namespace std{
	template <>
	struct hash<jhb::Model::Vertex> {
		size_t operator()(jhb::Model::Vertex const& vertex) const 
		{
			size_t seed = 0;
			jhb::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
			return seed;
		}
	};
}


jhb::Model::Model(Device& _device, const Builder& builder) : device{ _device }
{
	createVertexBuffer(builder.vertices);
	createIndexBuffer(builder.indices);
}

jhb::Model::~Model()
{
}

std::unique_ptr<jhb::Model> jhb::Model::createModelFromFile(Device& device, const std::string& filepath)
{
	Builder builder{};
	builder.loadModel(filepath);
	
	return std::make_unique<Model>(device, builder);
}

void jhb::Model::draw(VkCommandBuffer commandBuffer)
{
	if (hasIndexBuffer)
	{
		vkCmdDrawIndexed(commandBuffer, indexCount, 1,0,0,0);
	}
	else {
		vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
	}
}

void jhb::Model::bind(VkCommandBuffer commandBuffer)
{
	VkBuffer buffers[] = { vertexBuffer->getBuffer()};
	VkDeviceSize offsets[] = { 0 };
	// combine command buffer and vertex Buffer
	vkCmdBindVertexBuffers(commandBuffer, 0,  1, buffers, offsets);

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

	/* replaced by upper secion which used Buffer class
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	// VK_BUFFER_USAGE_TRANSFER_SRC_BIT means this buffer just used by memory transfer operation -> tools for sort of copy opertation -> meditator buffer??
	// it called staging buffer.
	device.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory
		);
		*/

	//this section replace down below annotation secion which using vkMapMemory and vkUnmapMemory
	stagingBuffer.map();
	stagingBuffer.writeToBuffer((void*)vertices.data());

	/*void* data;
	vkMapMemory(device.getLogicalDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(device.getLogicalDevice(), stagingBufferMemory);*/

	vertexBuffer = std::make_unique<Buffer>(
		device,
		vertexSize,
		vertexCount,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	// VK_BUFFER_USAGE_TRANSFER_DST_BIT : tells vulkan that this vertex buffer using optimal device local memory
	/*device.createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBuffer, vertexBufferMemory
	);*/

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

	/*
	// VK_BUFFER_USAGE_TRANSFER_DST_BIT : tells vulkan that this vertex buffer using optimal device local memory
	device.createBuffer(bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBuffer, indexBufferMemory
	);*/

	// copy to staging buffer to vertexbuffer
	// staging buffer used to only static data. ex) loading application stage. if data are frequently updated from host, then stop using this
	device.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
}

std::vector<VkVertexInputBindingDescription> jhb::Model::Vertex::getBindingDescriptions()
{
	std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
	bindingDescriptions[0].binding = 0;
	bindingDescriptions[0].stride = sizeof(Vertex);
	bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> jhb::Model::Vertex::getAttrivuteDescriptions()
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

	return attributeDescriptions;
}

void jhb::Model::Builder::loadModel(const std::string& filepath)
{
	tinyobj::attrib_t attr; // position, color, normal, and texture coordinate
	std::vector<tinyobj::shape_t> shapes; //index values for each face element
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;
	// all there are used to store datas readed from wavefrontfile objs

	if (!tinyobj::LoadObj(&attr, &shapes, &materials, &warn, &err, filepath.c_str())) {
		throw std::runtime_error(warn+ err);
	}

	vertices.clear();
	indices.clear();

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
	std::cout << vertices.size() << std::endl;
}
