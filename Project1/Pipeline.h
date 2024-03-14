#pragma once

#include <string>
#include <vector>

namespace jhb {
	struct PipelineConfigInfo {};
	class Pipeline
	{
	public:
		Pipeline(const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo& configInfo);
	private:
		static std::vector<char> readFile(const std::string& filepath);

		void createGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath);
	};
}
