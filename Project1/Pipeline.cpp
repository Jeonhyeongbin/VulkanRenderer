#include "Pipeline.h"

#include <fstream>
#include <stdexcept>
#include <iostream>

namespace jhb {
	Pipeline::Pipeline(const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo& configInfo)
	{
		createGraphicsPipeline(vertFilepath, fragFilepath, configInfo);
	}

	std::vector<char> jhb::Pipeline::readFile(const std::string& filepath)
	{
		std::ifstream file{filepath, std::ios::ate | std::ios::binary }; //ate flag : open file and close immdietly , binary flag : read file binary format for prevent unwanted convert from text to binary

		if (!file.is_open())
		{
			throw std::runtime_error("failed to open file: " + filepath);
		}

		size_t fileSize = static_cast<size_t>(file.tellg());
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();
		return buffer;
	}

	void jhb::Pipeline::createGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo& configInfo)
	{
		auto vertCode = readFile(vertFilepath);
		auto fragCode = readFile(fragFilepath);

		std::cout << "Vertex Shader Code Size: " << vertCode.size() << '\n';
		std::cout << "Fragment Shader Code Size: " << vertCode.size() << '\n';
	}
}