#include "Model.h"

jhb::Model::Model(Device& _device, const Builder& builder) : device{ _device }
{
	createVertexBuffer(builder.vertices);
	createIndexBuffer(builder.indices);
}

jhb::Model::~Model()
{
	vkDestroyBuffer(device.getLogicalDevice(), vertexBuffer, nullptr);
	vkFreeMemory(device.getLogicalDevice(), vertexBufferMemory, nullptr);

	if (hasIndexBuffer)
	{
		vkDestroyBuffer(device.getLogicalDevice(), indexBuffer, nullptr);
		vkFreeMemory(device.getLogicalDevice(), indexBufferMemory, nullptr);
	}
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
	VkBuffer buffers[] = { vertexBuffer };
	VkDeviceSize offsets[] = { 0 };
	// combine command buffer and vertex Buffer
	vkCmdBindVertexBuffers(commandBuffer, 0,  1, buffers, offsets);

	if (hasIndexBuffer)
	{
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	}
}

void jhb::Model::createVertexBuffer(const std::vector<Vertex>& vertices)
{
	vertexCount = static_cast<uint32_t>(vertices.size());
	assert(vertexCount >= 3 && "Vertex count must be at least 3");
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	// VK_BUFFER_USAGE_TRANSFER_SRC_BIT means this buffer just used by memory transfer operation -> tools for sort of copy opertation -> meditator buffer??
	// it called staging buffer.
	device.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory
		);
	void* data;
	vkMapMemory(device.getLogicalDevice(), vertexBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(device.getLogicalDevice(), vertexBufferMemory);

	// VK_BUFFER_USAGE_TRANSFER_DST_BIT : tells vulkan that this vertex buffer using optimal device local memory
	device.createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBuffer, vertexBufferMemory
	);

	// copy to staging buffer to vertexbuffer
	device.copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

	vkDestroyBuffer(device.getLogicalDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device.getLogicalDevice(), stagingBufferMemory, nullptr);
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
	device.createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		indexBuffer, indexBufferMemory
	);
	void* data;
	vkMapMemory(device.getLogicalDevice(), indexBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(device.getLogicalDevice(), indexBufferMemory);
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
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location= 0;
	attributeDescriptions[0].format= VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(Vertex, position);


	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, color);

	return attributeDescriptions;
}
