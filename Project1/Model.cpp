#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <cmath>  // std::log2 명시적 포함

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
#ifdef ANDROID
#include "AndroidAssets.h"
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "VulkanRenderer", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "VulkanRenderer", __VA_ARGS__)
#endif
#include "Device.h"
#include "JHBApplication.h"
#include <ktx.h>
#include <ktxvulkan.h>
#include "BaseRenderSystem.h"
#include "Pipeline.h"
#include <random>
#include <algorithm>

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

jhb::Model::Model(Device& _device, glm::mat4 modelMatrix) : device{ _device }
{
	//createVertexBuffer(builder->vertices);
	//createIndexBuffer(builder->indices);
}

jhb::Model::~Model()
{
    for (auto& image : images)
    {
        if (image.sampler) vkDestroySampler(device.getLogicalDevice(), image.sampler, nullptr);
        if (image.view) vkDestroyImageView(device.getLogicalDevice(), image.view, nullptr);
        if (image.image) vkDestroyImage(device.getLogicalDevice(), image.image, nullptr);
        if (image.deviceMemory) vkFreeMemory(device.getLogicalDevice(), image.deviceMemory, nullptr);
    }
    for (auto* node : nodes) delete node;
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
		if (noTexturePipeline)
		{
			noTexturePipeline->bind(commandBuffer);
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

void jhb::Model::drawIndirect(VkCommandBuffer commandBuffer, const Buffer& indirectCommandBuffer, VkPipelineLayout pipelineLayout, int frameIndex)
{
	if (!nodes.empty())
	{
		for (auto& node : nodes) {
			drawNode(commandBuffer, pipelineLayout, node, frameIndex);
		}
	}
	else {
		if (noTexturePipeline)
		{
			noTexturePipeline->bind(commandBuffer);
		}
		if (hasIndexBuffer)
		{
			vkCmdDrawIndexedIndirect(commandBuffer, indirectCommandBuffer.getBuffer(),0 , 1, sizeof(VkDrawIndirectCommand));
		}
		//else {
		//	vkCmdDraw(commandBuffer, vertexCount, instanceCount, 0, 0);
		//}
	}
}

void jhb::Model::buildIndirectCommand(std::vector<VkDrawIndexedIndirectCommand>& indirectCommandBuffer)
{
	if (!nodes.empty())
	{
		for (auto& node : nodes) {
			buildIndriectNode(node, indirectCommandBuffer);
		}
	}
	else {
		if (hasIndexBuffer)
		{
			indirectCommandBuffer.push_back(VkDrawIndexedIndirectCommand{ indexCount, instanceCount, 0,0,0 });
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
		if (noTexturePipeline)
		{
			noTexturePipeline->bind(commandBuffer);
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
	// pipeline for no gltf model or no texture
	if (noTexturePipeline == nullptr)
	{
		noTexturePipeline = std::make_unique<Pipeline>(device, vertFilepath, fragFilepath, configInfo);
	}
}

void jhb::Model::createGraphicsPipelinePerMaterial(const std::string& vertFilepath, const std::string& fragFilepath, PipelineConfigInfo& configInfo)
{
	for (auto& material : materials)
	{
		if (material.pipeline == nullptr)
		{
			material.pipeline = std::make_unique<Pipeline>(device, vertFilepath, fragFilepath, configInfo, material);
		}
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

void jhb::Model::loadImages(tinygltf::Model& input, VkSamplerAddressMode samplerMode)
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
			images[i].loadTexture2D(device, path + "/" + glTFImage.uri, samplerMode);
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
		materials[i].normalScale = float(glTFMaterial.normalTexture.scale);
		materials[i].occlusionStrength = float(glTFMaterial.occlusionTexture.strength);
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
		node->matrix = glm::translate(node->matrix, glm::vec3(inputNode.translation[0], inputNode.translation[1], inputNode.translation[2]));
	}
	if (inputNode.rotation.size() == 4) {
		glm::quat q{float(inputNode.rotation[3]), float(inputNode.rotation[0]), float(inputNode.rotation[1]), float(inputNode.rotation[2])};
		node->matrix *= glm::mat4(q);
	}
	if (inputNode.scale.size() == 3) {
		node->matrix = glm::scale(node->matrix, glm::vec3(inputNode.scale[0], inputNode.scale[1], inputNode.scale[2]));
	}
	if (inputNode.matrix.size() == 16) {
		for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row)
                node->matrix[column][row] = float(inputNode.matrix[column * 4 + row]);
	}

	// rootModelMatrix is Scene placement, not a copy of a glTF node transform.
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
			if (glTFPrimitive.indices < 0 ||
				static_cast<size_t>(glTFPrimitive.indices) >= input.accessors.size())
			{
				std::cerr << "Skipping glTF primitive: missing or invalid index accessor in "
					<< inputNode.name << std::endl;
				continue;
			}
			uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
			uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
			uint32_t indexCount = 0;
			// Vertices
			{
				int posByteStride;
				int normByteStride;

				const float* positionBuffer = nullptr;
				const float* normalsBuffer = nullptr;
				const float* texCoordsBuffer1 = nullptr;
				const float* texCoordsBuffer2 = nullptr;
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
					texCoordsBuffer1 = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}
				if (glTFPrimitive.attributes.find("TEXCOORD_1") != glTFPrimitive.attributes.end()) {
					const tinygltf::Accessor& uvAccessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_1")->second];
					const tinygltf::BufferView& uvView = input.bufferViews[uvAccessor.bufferView];
					texCoordsBuffer2 = reinterpret_cast<const float*>(&(input.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
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
					vert.uv = texCoordsBuffer1 ? glm::vec2(texCoordsBuffer1[v * 2], texCoordsBuffer1[v * 2 + 1]) : glm::vec3(0.0f);
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
			primitive.materialIndex = glTFPrimitive.material >= 0
                ? glTFPrimitive.material : static_cast<int32_t>(materials.size() - 1);
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

void jhb::Model::drawGraphGeometryOnly(VkCommandBuffer commandBuffer,
                                      VkPipelineLayout pipelineLayout)
{
    if (!instanceBuffer)
    {
        throw std::invalid_argument("Graph shadow geometry requires an instance buffer");
    }
    bind(commandBuffer);
    if (nodes.empty())
    {
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(rootModelMatrix), &rootModelMatrix);
        if (hasIndexBuffer)
        {
            vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, 0, 0);
        }
        else
        {
            vkCmdDraw(commandBuffer, vertexCount, instanceCount, 0, 0);
        }
        return;
    }

    const auto drawNode = [&](const auto &self, const Node *node,
                              const glm::mat4 &parentTransform) -> void
    {
        if (!node || !node->visible)
        {
            return;
        }
        const auto modelMatrix = parentTransform * node->matrix;
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(modelMatrix), &modelMatrix);
        for (const auto &primitive : node->mesh.primitives)
        {
            if (primitive.indexCount != 0)
            {
                vkCmdDrawIndexed(commandBuffer, primitive.indexCount, instanceCount,
                                 primitive.firstIndex, 0, 0);
            }
        }
        for (const auto *child : node->children)
        {
            self(self, child, modelMatrix);
        }
    };
    for (const auto *node : nodes)
    {
        drawNode(drawNode, node, rootModelMatrix);
    }
}

void jhb::Model::drawGraphGeometry(VkCommandBuffer commandBuffer,
                                    VkPipelineLayout pipelineLayout, int frameIndex,
                                    bool pushMaterialConstants)
{
    if (frameIndex < 0 || frameIndex >= SwapChain::MAX_FRAMES_IN_FLIGHT)
    {
        throw std::out_of_range("Invalid material descriptor frame index");
    }
    if (nodes.empty())
    {
        throw std::invalid_argument("Graph PBR currently requires a glTF model with materials");
    }

    bind(commandBuffer);

    const auto drawNode = [&](const auto& self, const Node* node,
                              const glm::mat4& parentTransform) -> void
    {
        if (!node || !node->visible)
        {
            return;
        }

        const glm::mat4 modelMatrix = parentTransform * node->matrix;
        const VkShaderStageFlags stages = pushMaterialConstants
            ? VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            : VK_SHADER_STAGE_VERTEX_BIT;
        vkCmdPushConstants(commandBuffer, pipelineLayout, stages,
                           0, sizeof(modelMatrix), &modelMatrix);

        for (const auto& primitive : node->mesh.primitives)
        {
            if (primitive.indexCount == 0)
            {
                continue;
            }

            const auto& material = materials.at(primitive.materialIndex);
            if (pushMaterialConstants)
            {
                // Matches the four vec4s at offset 64 in the G-buffer fragment shader.
                const std::array<glm::vec4, 4> constants = {
                    material.baseColorFactor, material.emissiveFactor,
                    glm::vec4(material.metallicFactor, material.roughnessFactor,
                              material.alphaMode == "MASK" ? 1.0f : 0.0f, material.alphaCutOff),
                    glm::vec4(material.normalScale, material.occlusionStrength,
                              material.doubleSided ? 1.0f : 0.0f, 0.0f)};
                static_assert(sizeof(constants) == 64);
                vkCmdPushConstants(commandBuffer, pipelineLayout, stages,
                                   64, sizeof(constants), constants.data());
            }
            const auto set = material.descriptorSets.at(frameIndex);
            if (set == VK_NULL_HANDLE)
            {
                throw std::logic_error("Graph geometry requires prepared material descriptors");
            }

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayout, 3, 1, &set, 0, nullptr);
            vkCmdDrawIndexed(commandBuffer, primitive.indexCount, instanceCount,
                             primitive.firstIndex, 0, 0);
        }

        for (const auto* child : node->children)
        {
            self(self, child, modelMatrix);
        }
    };

    for (const auto* node : nodes)
    {
        drawNode(drawNode, node, rootModelMatrix);
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
		glm::mat4 nodeMatrix =  node->matrix *rootModelMatrix;
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
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material.pipeline->getPipeline());
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &material.descriptorSets[frameIndex], 0, nullptr);
				vkCmdDrawIndexed(commandBuffer, primitive.indexCount, instanceCount, primitive.firstIndex, 0, 0);
			}
		}
	}
	for (auto& child : node->children) {
		drawNode(commandBuffer, pipelineLayout, child, frameIndex);
	}
	
}

void jhb::Model::buildIndriectNode(Node* node, std::vector<VkDrawIndexedIndirectCommand>& indirectCommandsBuffer)
{
	if (!node->visible) {
		return;
	}
	if (node->mesh.primitives.size() > 0) {
		for (Primitive& primitive : node->mesh.primitives) {
			if (primitive.indexCount > 0) {
				indirectCommandsBuffer.push_back(VkDrawIndexedIndirectCommand{primitive.indexCount, instanceCount, primitive.firstIndex});
				//vkCmdDrawIndexed(commandBuffer, primitive.indexCount, instanceCount, primitive.firstIndex, 0, 0);
			}
		}
	}
	for (auto& child : node->children) {
		buildIndriectNode(child, indirectCommandsBuffer);
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
		glm::mat4 nodeMatrix =  node->matrix * rootModelMatrix;
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

void jhb::Image::loadTexture2D(Device& device, const std::string& filepath, VkSamplerAddressMode samplerMode)
{
	int texWidth = 0, texHeight = 0, texChannels = 0;

	if (filepath.size() <= 0)
		return;

#ifdef ANDROID
	// Android: APK assets는 stbi_load()로 직접 열 수 없음 → AAssetManager로 읽어서 메모리에서 디코딩
	AAssetManager* mgr = jhb::getAndroidAssetManager();
	AAsset* asset = AAssetManager_open(mgr, filepath.c_str(), AASSET_MODE_BUFFER);
	if (!asset) {
		throw std::runtime_error("failed to open texture asset: " + filepath);
	}
	size_t fileSize = static_cast<size_t>(AAsset_getLength(asset));
	std::vector<uint8_t> fileData(fileSize);
	AAsset_read(asset, fileData.data(), fileSize);
	AAsset_close(asset);
	auto pixels = stbi_load_from_memory(fileData.data(), static_cast<int>(fileSize),
	                                    &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
#else
	auto pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
#endif

	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth) * texHeight * 4;
	int mipleves = static_cast<uint32_t>(floor(log2(std::max(texWidth, texHeight))) + 1.0);

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
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
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
	vkDestroyBuffer(device.getLogicalDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device.getLogicalDevice(), stagingBufferMemory, nullptr);
	generateMipmap(device, image, mipleves, texWidth, texHeight);

	// create image view and image sampler
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
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

	samplerInfo.addressModeU = samplerMode;
	samplerInfo.addressModeV = samplerMode;
	samplerInfo.addressModeW = samplerMode;

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

	ktxTexture* ktxTex;

#ifdef ANDROID
	// Android: APK assets에서 AAssetManager로 파일을 읽은 뒤 메모리로 KTX 생성
	try {
		AAssetManager* mgr = jhb::getAndroidAssetManager();
		if (!mgr) {
			LOGE("[KTX] AAssetManager is null, skipping: %s", filepath.c_str());
			return;
		}

		AAsset* asset = AAssetManager_open(mgr, filepath.c_str(), AASSET_MODE_BUFFER);
		if (!asset) {
			LOGE("[KTX] AAssetManager_open failed: %s", filepath.c_str());
			return;
		}

		size_t fileSize = static_cast<size_t>(AAsset_getLength(asset));
		LOGI("[KTX] Loading %s (%zu bytes)", filepath.c_str(), fileSize);
		std::vector<uint8_t> fileBuffer(fileSize);
		AAsset_read(asset, fileBuffer.data(), fileSize);
		AAsset_close(asset);

		ktxResult result = ktxTexture_CreateFromMemory(
			fileBuffer.data(), fileBuffer.size(),
			KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTex);
		if (result != KTX_SUCCESS) {
			LOGE("[KTX] ktxTexture_CreateFromMemory failed: result=%d, file=%s", result, filepath.c_str());
			return;
		}
		LOGI("[KTX] Loaded OK: %dx%d mips=%d file=%s",
			ktxTex->baseWidth, ktxTex->baseHeight, ktxTex->numLevels, filepath.c_str());
	}
	catch (const std::exception& e) {
		LOGE("[KTX] Exception during load: %s (file=%s)", e.what(), filepath.c_str());
		return;
	}
	catch (...) {
		LOGE("[KTX] Unknown exception during load (file=%s)", filepath.c_str());
		return;
	}

#else
	ktxResult result = ktxTexture_CreateFromNamedFile(
		filepath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTex);
	if (result != KTX_SUCCESS)
	{
		throw std::runtime_error("Failed to load KTX texture: " + filepath);
	}
#endif

#ifdef ANDROID
	// Vulkan 작업 전체를 try/catch로 감싸 예외가 앱을 죽이는 것을 방지
	try {
#endif

	uint32_t width     = ktxTex->baseWidth;
	uint32_t height    = ktxTex->baseHeight;
	uint32_t mipLevels = ktxTex->numLevels;

	ktx_uint8_t* ktxData     = ktxTexture_GetData(ktxTex);
	ktx_size_t   ktxDataSize = ktxTexture_GetDataSize(ktxTex);

#ifdef ANDROID
	if (!ktxData || ktxDataSize == 0) {
		LOGE("[KTX] ktxTexture_GetData returned null or zero size: %s", filepath.c_str());
		ktxTexture_Destroy(ktxTex);
		return;
	}
	LOGI("[KTX] ktxData=%p size=%zu", (void*)ktxData, (size_t)ktxDataSize);
#endif

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	device.createBuffer(ktxDataSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(device.getLogicalDevice(), stagingBufferMemory, 0, ktxDataSize, 0, &data);
	memcpy(data, ktxData, ktxDataSize);
	vkUnmapMemory(device.getLogicalDevice(), stagingBufferMemory);

	// Setup buffer copy regions for each face including all of its mip levels
	std::vector<VkBufferImageCopy> bufferCopyRegions;
	for (uint32_t face = 0; face < static_cast<uint32_t>(arrayCount); face++)
	{
		for (uint32_t level = 0; level < mipLevels; level++)
		{
			ktx_size_t offset;
			KTX_error_code res = ktxTexture_GetImageOffset(ktxTex, level, 0, face, &offset);
			assert(res == KTX_SUCCESS);

			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel       = level;
			bufferCopyRegion.imageSubresource.baseArrayLayer = face;
			bufferCopyRegion.imageSubresource.layerCount     = 1;
			bufferCopyRegion.imageExtent.width               = ktxTex->baseWidth  >> level;
			bufferCopyRegion.imageExtent.height              = ktxTex->baseHeight >> level;
			bufferCopyRegion.imageExtent.depth               = 1;
			bufferCopyRegion.bufferOffset                    = offset;
			bufferCopyRegions.push_back(bufferCopyRegion);
		}
	}

	ktxTexture_Destroy(ktxTex);

	VkImageCreateInfo imageInfo{};
	imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType     = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width  = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth  = 1;
	imageInfo.mipLevels     = mipLevels;
	imageInfo.arrayLayers   = arrayCount;
	imageInfo.format        = VK_FORMAT_R16G16B16A16_SFLOAT;
	imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	device.createImageWithInfo(imageInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, deviceMemory);

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount   = mipLevels;
	subresourceRange.layerCount   = arrayCount;

	VkCommandBuffer commandBuffer = device.beginSingleTimeCommands();
	device.transitionImageLayout(commandBuffer, image,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

	vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		static_cast<uint32_t>(bufferCopyRegions.size()),
		bufferCopyRegions.data());

	device.transitionImageLayout(commandBuffer, image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);
	device.endSingleTimeCommands(commandBuffer);

	vkDestroyBuffer(device.getLogicalDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device.getLogicalDevice(), stagingBufferMemory, nullptr);

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image    = image;
	viewInfo.viewType = imgViewType;
	viewInfo.format   = VK_FORMAT_R16G16B16A16_SFLOAT;
	viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel   = 0;
	viewInfo.subresourceRange.levelCount     = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount     = arrayCount;
	if (vkCreateImageView(device.getLogicalDevice(), &viewInfo, nullptr, &view) != VK_SUCCESS)
		throw std::runtime_error("failed to create texture image view!");

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType               = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter           = VK_FILTER_LINEAR;
	samplerInfo.minFilter           = VK_FILTER_LINEAR;
	samplerInfo.addressModeU        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW        = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable    = VK_FALSE;
	samplerInfo.maxAnisotropy       = 0;
	samplerInfo.borderColor         = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable       = VK_FALSE;
	samplerInfo.compareOp           = VK_COMPARE_OP_NEVER;
	samplerInfo.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias          = 0.0f;
	samplerInfo.minLod              = 0.0f;
	samplerInfo.maxLod              = static_cast<float>(mipLevels);
	if (vkCreateSampler(device.getLogicalDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
		throw std::runtime_error("failed to create texture sampler!");

	this->width = width;
	this->height = height;
	this->mipLevels = mipLevels;
	this->layerCount = static_cast<uint32_t>(arrayCount);
	this->format = imageInfo.format;
	imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	updateDescriptor();

#ifdef ANDROID
	LOGI("[KTX] Vulkan setup complete: %s", filepath.c_str());
	} // try (Vulkan 작업)
	catch (const std::exception& e) {
		LOGE("[KTX] Vulkan exception: %s  file=%s", e.what(), filepath.c_str());
	}
	catch (...) {
		LOGE("[KTX] Vulkan unknown exception  file=%s", filepath.c_str());
	}
#endif
}

void jhb::Model::PickingPhasedrawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, Node* node, int frameIndex, VkPipeline pipeline)
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

	// todo : ���� �߽ɰ� ������ ���ϱ�.
	sphere.center = { (sphere.maxcoordinate.x + sphere.mincoordinate.x) / 2, (sphere.maxcoordinate.y + sphere.mincoordinate.y) / 2, (sphere.maxcoordinate.z + sphere.mincoordinate.z) / 2 };
	sphere.radius = (float)(sqrt(pow(abs(sphere.maxcoordinate.x - sphere.mincoordinate.x), 2) + pow(abs(sphere.maxcoordinate.x - sphere.mincoordinate.x), 2) + pow(abs(sphere.maxcoordinate.x - sphere.mincoordinate.x), 2)));
}

void jhb::Model::updateInstanceBuffer(uint32_t _instanceCount, const std::vector<glm::vec3>& positions, const std::vector<glm::vec3>& rotations, float roughness, float metallic)
{
	if (_instanceCount == 0 || positions.size() != _instanceCount ||
		rotations.size() != _instanceCount)
	{
		std::cerr << "Instance count must be nonzero and match positions and rotations." << std::endl;
		return;
	}

	// Called before recording draws. Finish previous uses before overwriting or replacing the buffer.
	if (instanceBuffer && vkDeviceWaitIdle(device.getLogicalDevice()) != VK_SUCCESS)
	{
		std::cerr << "Failed to wait before updating instance buffer." << std::endl;
		return;
	}

	const VkDeviceSize dataSize = static_cast<VkDeviceSize>(sizeof(InstanceData)) * _instanceCount;
	if (!instanceBuffer || instanceBuffer->getBufferSize() < dataSize)
	{
		auto newBuffer = std::make_unique<Buffer>(device, sizeof(InstanceData), _instanceCount,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		if (newBuffer->map() != VK_SUCCESS)
		{
			std::cerr << "Failed to map instance buffer." << std::endl;
			return;
		}
		instanceBuffer = std::move(newBuffer);
	}

	instanceCount = _instanceCount;
	instanceData.resize(instanceCount);
	for (uint32_t i = 0; i < instanceCount; ++i)
	{
		instanceData[i].r = static_cast<float>(i); // instance id offset
		instanceData[i].pos = positions[i];
		instanceData[i].rot = rotations[i];
	}

	instanceBuffer->writeToBuffer(instanceData.data(), dataSize, 0);
	if (instanceBuffer->flush() != VK_SUCCESS)
	{
		std::cerr << "Failed to flush instance buffer." << std::endl;
	}
}
