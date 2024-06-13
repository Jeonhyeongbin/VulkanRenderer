
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
#include "Pipeline.h"

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


jhb::Model::Model(Device& _device) : device{ _device }
{
}

jhb::Model::Model(Device& _device, glm::mat4 modelMatrix) : device{ _device }, modelMatrix{modelMatrix}
{
	//createVertexBuffer(builder->vertices);
	//createIndexBuffer(builder->indices);
}

jhb::Model::~Model()
{
}

void jhb::Model::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, int frameIndex)
{
	if (!nodes.empty())
	{
		for (auto& node : nodes) {
			drawNode(commandBuffer, pipelineLayout, node, frameIndex);
		}
	}
	else {
		if (perModelPipeline)
		{
			perModelPipeline->bind(commandBuffer);
		}
		if (hasIndexBuffer)
		{
			vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, 0, 0);
		}
		else {
			vkCmdDraw(commandBuffer, vertexCount, instanceCount, 0, 0);
		}
	}
}

void jhb::Model::drawNoTexture(VkCommandBuffer buffer, VkPipeline pipeline, VkPipelineLayout pipelineLayout, int frameIndex)
{
	if (!nodes.empty())
	{
		for (auto& node : nodes) {
			drawNodeNotexture(buffer, pipeline, pipelineLayout, node);
		}
	}
	else {
		auto matrix = glm::mat4{ 1.f };
		vkCmdPushConstants(buffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 128, sizeof(glm::mat4), &matrix);
		vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		if (hasIndexBuffer)
		{
			vkCmdDrawIndexed(buffer, indexCount, instanceCount, 0, 0, 0);
		}
		else {
			vkCmdDraw(buffer, vertexCount, instanceCount, 0, 0);
		}
	}
}

void jhb::Model::drawInPickPhase(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VkPipeline pipeline, int frameIndex)
{
	if (!nodes.empty())
	{
		for (auto& node : nodes) {
			PickingPhasedrawNode(commandBuffer, pipelineLayout, node, frameIndex, pipeline);
		}
	}
	else {
		if (perModelPipeline)
		{
			perModelPipeline->bind(commandBuffer);
		}
		if (hasIndexBuffer)
		{
			vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, 0, 0);
		}
		else {
			vkCmdDraw(commandBuffer, vertexCount, instanceCount, 0, 0);
		}
	}
}

void jhb::Model::bind(VkCommandBuffer commandBuffer)
{
	VkBuffer buffers[] = { vertexBuffer->getBuffer()};
	VkDeviceSize offsets[] = { 0 };
	// combine command buffer and vertex Buffer
	vkCmdBindVertexBuffers(commandBuffer, 0,  1, buffers, offsets);
	if (instanceBuffer)
	{
		VkBuffer instance[] = {instanceBuffer->getBuffer()};
		vkCmdBindVertexBuffers(commandBuffer, 1, 1, instance, offsets);
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

void jhb::Model::createPipelineForModel(const std::string& vertFilepath, const std::string& fragFilepath, PipelineConfigInfo& configInfo)
{
	if (perModelPipeline == nullptr)
	{
		perModelPipeline = std::make_unique<Pipeline>(device, vertFilepath, fragFilepath, configInfo);
	}
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
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions(5);
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
	createVertexBuffer(vertices);
	createIndexBuffer(indices);
}

void jhb::Model::loadImages(tinygltf::Model& input)
{
	images.resize(input.images.size());
	for (size_t i = 0; i < input.images.size(); i++) {
		tinygltf::Image& glTFImage = input.images[i];
		bool isKtx = false;
		// Image points to an external ktx file
		if (glTFImage.uri.find_last_of(".") != std::string::npos) {
			if (glTFImage.uri.substr(glTFImage.uri.find_last_of(".") + 1) == "ktx") {
				isKtx = true;
			}
		}

		if (isKtx)
		{
			images[i].loadKTXTexture(device, path + "/" + glTFImage.uri);
		}
		else
		{
			images[i].loadTexture2D(device, path + "/" + glTFImage.uri);
		}
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
			materials[i].baseColorFactor = glm::vec4{ static_cast<float>(glTFMaterial.values["baseColorFactor"].ColorFactor()[0]),static_cast<float>(glTFMaterial.values["baseColorFactor"].ColorFactor()[1])
			, static_cast<float>(glTFMaterial.values["baseColorFactor"].ColorFactor()[2]), static_cast<float>(glTFMaterial.values["baseColorFactor"].ColorFactor()[3])
			};
		}
		// Get base color texture index
		if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
			materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
		}

		if (glTFMaterial.values.find("roughnessFactor") != glTFMaterial.values.end()) {
			materials[i].roughnessFactor = static_cast<float>(glTFMaterial.values["roughnessFactor"].Factor());
		}
		if (glTFMaterial.values.find("metallicFactor") != glTFMaterial.values.end()) {
			materials[i].metallicFactor = static_cast<float>(glTFMaterial.values["metallicFactor"].Factor());
		}
		if (glTFMaterial.additionalValues.find("emissiveFactor") != glTFMaterial.additionalValues.end()) {
			materials[i].emissiveFactor = glm::vec4(glTFMaterial.additionalValues["emissiveFactor"].ColorFactor()[0], 
				glTFMaterial.additionalValues["emissiveFactor"].ColorFactor()[1], glTFMaterial.additionalValues["emissiveFactor"].ColorFactor()[2], 1.0f);
		}

		if (glTFMaterial.values.find("metallicRoughnessTexture") != glTFMaterial.values.end()) {
			materials[i].metallicRoughnessTextureIndex = static_cast<float>(glTFMaterial.values["metallicRoughnessTexture"].TextureIndex());
		}

		if (glTFMaterial.additionalValues.find("emissiveTexture") != glTFMaterial.additionalValues.end()) {
			materials[i].emissiveTextureIndex = glTFMaterial.additionalValues["emissiveTexture"].TextureIndex();
		}
		if (glTFMaterial.additionalValues.find("occlusionTexture") != glTFMaterial.additionalValues.end()) {
			materials[i].occlusionTextureIndex = glTFMaterial.additionalValues["occlusionTexture"].TextureIndex();
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

void jhb::Image::generateMipmap(Device& device, VkImage image, int mipLevels, uint32_t width, uint32_t height)
{
	// Generate the mip chain (glTF uses jpg and png, so we need to create this manually)
	VkCommandBuffer blitCmd = device.beginSingleTimeCommands();
	VkImageSubresourceRange mipSubRange = {};
	VkImageMemoryBarrier imageMemoryBarrier{};
	for (uint32_t i = 1; i < mipLevels; i++) {
		VkImageBlit imageBlit{};

		imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.srcSubresource.layerCount = 1;
		imageBlit.srcSubresource.mipLevel = i - 1;
		imageBlit.srcOffsets[1].x = int32_t(width >> (i - 1));
		imageBlit.srcOffsets[1].y = int32_t(height >> (i - 1));
		imageBlit.srcOffsets[1].z = 1;

		imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.dstSubresource.layerCount = 1;
		imageBlit.dstSubresource.mipLevel = i;
		imageBlit.dstOffsets[1].x = int32_t(width >> i);
		imageBlit.dstOffsets[1].y = int32_t(height >> i);
		imageBlit.dstOffsets[1].z = 1;


		mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		mipSubRange.baseMipLevel = i-1;
		mipSubRange.levelCount = 1;
		mipSubRange.layerCount = 1;

		{
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			imageMemoryBarrier.image = image;
			imageMemoryBarrier.subresourceRange = mipSubRange;
			vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
		}

		vkCmdBlitImage(blitCmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);

		{
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageMemoryBarrier.image = image;
			imageMemoryBarrier.subresourceRange = mipSubRange;
			vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
		}
	}
	imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	
	mipSubRange.baseMipLevel = mipLevels - 1;
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = mipSubRange;
	vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

	device.endSingleTimeCommands(blitCmd);
}

void jhb::Image::updateDescriptor()
{
	descriptor.sampler = sampler;
	descriptor.imageView = view;
	descriptor.imageLayout = imageLayout;
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
	}

	// make inverse root model matrix
	inverseRootModelMatrix = glm::mat4(1.0f);
	if (inputNode.translation.size() == 3) {
		inverseRootModelMatrix = glm::translate(inverseRootModelMatrix, -glm::vec3(*inputNode.translation.data()));
	}
	if (inputNode.rotation.size() == 4) {
		glm::quat q = glm::quat{ glm::mat4(*inputNode.rotation.data()) };
		inverseRootModelMatrix *= glm::transpose(glm::mat4(q));
	}
	if (inputNode.scale.size() == 3) {
		inverseRootModelMatrix = glm::scale(inverseRootModelMatrix, glm::vec3(*inputNode.scale.data()));
	}
	if (inputNode.matrix.size() == 16) {
		inverseRootModelMatrix = glm::inverse(glm::mat4(*inputNode.matrix.data()));
	}
	inverseRootModelMatrix = glm::inverse(modelMatrix);
	rootModelMatrix = modelMatrix * node->matrix;
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
				int posByteStride;
				int normByteStride;

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
					posByteStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
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
					// if model has tangent, dont need to calculate tangent vector
					hasTangent = true;
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TANGENT")->second];
					const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
					tangentsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				// Append data to model's vertex buffer
				for (size_t v = 0; v < vertexCount; v++) {
					Vertex vert{};
					vert.position = glm::vec4(glm::vec3(positionBuffer[3*v], positionBuffer[3*v+1], positionBuffer[3*v+2]), 1.0f);
					vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::vec3(normalsBuffer[3 * v], normalsBuffer[3 * v + 1], normalsBuffer[3 * v + 2]) : glm::vec3(0.0f)));
					vert.uv = texCoordsBuffer ? glm::vec2(texCoordsBuffer[v * 2], texCoordsBuffer[v * 2 + 1]) : glm::vec3(0.0f);
					vert.color = glm::vec3(1.f);
					vert.tangent = tangentsBuffer ? glm::vec4(tangentsBuffer[v * 4], tangentsBuffer[v * 4 + 1], tangentsBuffer[v * 4 + 2], tangentsBuffer[v * 4 + 3]) : glm::vec4(0.0f);
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
		glm::mat4 nodeMatrix = modelMatrix * pickedObjectRotationMatrix * node->matrix;
		Node* currentParent = node->parent;
		while (currentParent) {
			nodeMatrix = currentParent->matrix * nodeMatrix;
			currentParent = currentParent->parent;
		}
		// Pass the final matrix to the vertex shader using push constants
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
		for (Primitive& primitive : node->mesh.primitives) {
			if (primitive.indexCount > 0) {
				Material& material = materials[primitive.materialIndex];
				// POI: Bind the pipeline for the node's material
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &material.descriptorSets[frameIndex], 0, nullptr);
				vkCmdDrawIndexed(commandBuffer, primitive.indexCount, instanceCount, primitive.firstIndex, 0, 0);
			}
		}
	}
	for (auto& child : node->children) {
		drawNode(commandBuffer, pipelineLayout, child, frameIndex);
	}
	
}

void jhb::Model::drawNodeNotexture(VkCommandBuffer commandBuffer, VkPipeline pipeline, VkPipelineLayout pipelineLayout, Node* node)
{
	if (!node->visible) {
		return;
	}
	if (node->mesh.primitives.size() > 0) {
		// Pass the node's matrix via push constants
		// Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
		glm::mat4 nodeMatrix = modelMatrix * pickedObjectRotationMatrix * node->matrix;
		Node* currentParent = node->parent;
		while (currentParent) {
			nodeMatrix = currentParent->matrix * nodeMatrix;
			currentParent = currentParent->parent;
		}
		// Pass the final matrix to the vertex shader using push constants
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 128, sizeof(glm::mat4), &nodeMatrix);
		for (Primitive& primitive : node->mesh.primitives) {
			if (primitive.indexCount > 0) {
				Material& material = materials[primitive.materialIndex];
				// POI: Bind the pipeline for the node's material
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
				vkCmdDrawIndexed(commandBuffer, primitive.indexCount, instanceCount, primitive.firstIndex, 0, 0);
			}
		}
	}
	for (auto& child : node->children) {
		drawNodeNotexture(commandBuffer, pipeline, pipelineLayout, child);
	}
}

void jhb::Image::loadTexture2D(Device& device, const std::string& filepath)
{
	int texWidth, texHeight, texChannels;

	if (filepath.size() <= 0)
		return;

	auto pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4; // 4byte per pixel
	int mipleves = static_cast<uint32_t>(floor(log2(max(texWidth, texHeight))) + 1.0);
	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	device.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
	void* data;
	vkMapMemory(device.getLogicalDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(device.getLogicalDevice(), stagingBufferMemory);

	stbi_image_free(pixels);

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = static_cast<uint32_t>(texWidth);
	imageInfo.extent.height = static_cast<uint32_t>(texHeight);
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = mipleves;
	imageInfo.arrayLayers = 1;
	imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0; // Optional
	device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, deviceMemory);

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = mipleves;
	subresourceRange.layerCount = 1;

	VkCommandBuffer commandBuffer = device.beginSingleTimeCommands();
	device.transitionImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

	device.copyBufferToImage(commandBuffer, stagingBuffer, image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1);
	device.endSingleTimeCommands(commandBuffer);
	generateMipmap(device, image, mipleves, texWidth, texHeight);

	// create image view and image sampler
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipleves;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(device.getLogicalDevice(), &viewInfo, nullptr, &view) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture image view!");
	}

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;

	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

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
	samplerInfo.maxLod = mipleves;

	if (vkCreateSampler(device.getLogicalDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}

	updateDescriptor();
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

	updateDescriptor();
}

void jhb::Model::PickingPhasedrawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, Node* node, int frameIndex, VkPipeline pipeline)
{
	if (!node->visible) {
		return;
	}
	if (node->mesh.primitives.size() > 0) {
		// Pass the node's matrix via push constants
		// Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
		glm::mat4 nodeMatrix = modelMatrix * node->matrix;
		Node* currentParent = node->parent;
		
		while (currentParent) {
			nodeMatrix = currentParent->matrix * nodeMatrix;
			currentParent = currentParent->parent;
		}
		// Pass the final matrix to the vertex shader using push constants
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
		for (Primitive& primitive : node->mesh.primitives) {
			if (primitive.indexCount > 0) {
				Material& material = materials[primitive.materialIndex];
				// POI: Bind the pipeline for the node's material
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
				vkCmdDrawIndexed(commandBuffer, primitive.indexCount, instanceCount, primitive.firstIndex, 0, 0);
			}
		}
	}
	for (auto& child : node->children) {
		drawNode(commandBuffer, pipelineLayout, child, frameIndex);
	}
}

void jhb::Model::calculateTangent(glm::vec2 uv1, glm::vec2 uv2, glm::vec2 uv3, glm::vec3 pos1, glm::vec3 pos2, glm::vec3 pos3, glm::vec4& tangent)
{
	glm::vec3 edge1 = pos2 - pos1;
	glm::vec3 edge2 = pos3 - pos1;
	glm::vec2 deltaUV1 = uv2 - uv1;
	glm::vec2 deltaUV2 = uv3 - uv1;

	float det = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
	if (det == 0.0f)
	{
		tangent.x = 0.f;
		tangent.y = 0.f;
		tangent.z = 0.f;
		return;
	}

	float f = 1.0f / det;

	tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
	tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
	tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
}

void jhb::Model::createObjectSphere(const std::vector<Vertex> vertices)
{
	for (auto vertex : vertices)
	{
		if (vertex.position.x < sphere.mincoordinate.x)
			sphere.mincoordinate.x = vertex.position.x;
		if (vertex.position.y < sphere.mincoordinate.y)
			sphere.mincoordinate.y = vertex.position.y;
		if (vertex.position.z < sphere.mincoordinate.z)
			sphere.mincoordinate.z = vertex.position.z;
		if (vertex.position.x > sphere.maxcoordinate.x)
			sphere.maxcoordinate.x = vertex.position.x;
		if (vertex.position.y > sphere.maxcoordinate.y)
			sphere.maxcoordinate.y = vertex.position.y;
		if (vertex.position.z > sphere.maxcoordinate.z)
			sphere.maxcoordinate.z = vertex.position.z;
	}
}

void jhb::Model::updateInstanceBuffer(uint32_t _instanceCount, float offsetX, float offsetZ, float roughness, float metallic)
{
	instanceCount = _instanceCount;
	if (instanceBuffer == nullptr)
	{
		instanceBuffer = std::make_unique<Buffer>(device, sizeof(InstanceData), instanceCount, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	std::vector<InstanceData> instanceData;
	instanceData.resize(instanceCount);

	for (float i = 0; i < instanceCount; i++)
	{
		auto rotate = glm::rotate(glm::mat4(1.f), (i * glm::two_pi<float>() / instanceCount), { 0.f, 0.f, 1.f });
		glm::vec4 tmp{ offsetX, 0.f, offsetZ, 1};
		instanceData[i].pos = glm::vec3(rotate * tmp);
		instanceData[i].r = 0.5f;
		instanceData[i].g = 0.5f;
		instanceData[i].b = 0.5f;
		instanceData[i].roughness = roughness;
		instanceData[i].metallic = metallic;
	}

	Buffer stagingBuffer(device, sizeof(InstanceData), 64, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	stagingBuffer.map();
	stagingBuffer.writeToBuffer(instanceData.data(), instanceBuffer->getBufferSize(), 0);

	device.copyBuffer(stagingBuffer.getBuffer(), instanceBuffer->getBuffer(), instanceBuffer->getBufferSize());
	stagingBuffer.unmap();
}
