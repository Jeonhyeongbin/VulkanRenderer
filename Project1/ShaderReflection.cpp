#include "ShaderReflection.h"
#include "RenderGraph/RenderGraph.h"
#include "External/SPIRV-Reflect/spirv_reflect.h"

#include <fstream>
#include <map>
#include <stdexcept>
#ifdef ANDROID
#include "AndroidAssets.h"
#endif

namespace jhb::rg
{
namespace
{
std::vector<uint32_t> readShader(const std::string &path)
{
    std::vector<uint32_t> words;
#ifdef ANDROID
    auto *asset = AAssetManager_open(getAndroidAssetManager(), path.c_str(), AASSET_MODE_BUFFER);
    if (!asset)
    {
        throw std::runtime_error("Missing reflection shader: " + path);
    }
    const auto bytes = AAsset_getLength(asset);
    if (bytes <= 0 || bytes % 4)
    {
        AAsset_close(asset);
        throw std::runtime_error("Invalid SPIR-V byte count: " + path);
    }
    words.resize(bytes / 4);
    const auto read = AAsset_read(asset, words.data(), bytes);
    AAsset_close(asset);
    if (read != bytes)
    {
        throw std::runtime_error("Failed to read reflection shader: " + path);
    }
#else
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        throw std::runtime_error("Missing reflection shader: " + path);
    }
    const auto bytes = file.tellg();
    if (bytes <= 0 || static_cast<size_t>(bytes) % 4)
    {
        throw std::runtime_error("Invalid SPIR-V byte count: " + path);
    }
    words.resize(static_cast<size_t>(bytes) / 4);
    file.seekg(0);
    if (!file.read(reinterpret_cast<char *>(words.data()), bytes))
    {
        throw std::runtime_error("Failed to read reflection shader: " + path);
    }
#endif
    return words;
}

struct Module
{
    SpvReflectShaderModule data{};

    explicit Module(const std::vector<uint32_t> &words)
    {
        if (spvReflectCreateShaderModule(words.size() * sizeof(uint32_t), words.data(), &data) !=
            SPV_REFLECT_RESULT_SUCCESS)
        {
            throw std::runtime_error("SPIR-V reflection failed");
        }
    }

    ~Module()
    {
        spvReflectDestroyShaderModule(&data);
    }

    Module(const Module &) = delete;
    Module &operator=(const Module &) = delete;
};
} // namespace

void reflectPipelineInterface(PipelineDesc &desc)
{
    std::map<std::pair<uint32_t, uint32_t>, PipelineBinding> bindings;
    uint32_t pushSize = 0;
    VkShaderStageFlags pushStages = 0;
    const auto reflect = [&](const std::string &path, VkShaderStageFlagBits stage)
    {
        if (path.empty())
        {
            return;
        }
        const auto words = readShader(path);
        Module module(words);
        if (static_cast<VkShaderStageFlags>(module.data.shader_stage) != stage)
        {
            throw std::runtime_error("Shader stage does not match pipeline: " + path);
        }

        for (uint32_t i = 0; i < module.data.descriptor_binding_count; ++i)
        {
            const auto &b = module.data.descriptor_bindings[i];
            if (b.count == 0)
            {
                throw std::runtime_error("Runtime descriptor arrays are not supported");
            }
            const auto key = std::make_pair(b.set, b.binding);
            auto existing = bindings.find(key);
            if (existing != bindings.end())
            {
                if (existing->second.type != static_cast<VkDescriptorType>(b.descriptor_type) ||
                    existing->second.count != b.count)
                {
                    throw std::runtime_error("Incompatible shader descriptor declarations");
                }
                existing->second.stages |= stage;
            }
            else
            {
                bindings.emplace(key,
                                 PipelineBinding{b.set, b.binding,
                                                 static_cast<VkDescriptorType>(b.descriptor_type),
                                                 b.count, static_cast<VkShaderStageFlags>(stage),
                                                 b.name ? b.name : ""});
            }
        }
        for (uint32_t i = 0; i < module.data.push_constant_block_count; ++i)
        {
            const auto &block = module.data.push_constant_blocks[i];
            pushSize = (std::max)(pushSize, block.offset + block.size);
            pushStages |= stage;
        }
    };

    if (desc.type == PipelineType::Compute)
    {
        reflect(desc.computePath, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    else
    {
        reflect(desc.vertexPath, VK_SHADER_STAGE_VERTEX_BIT);
        reflect(desc.fragmentPath, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    // Preserve explicitly supplied compatible layouts (including broader stage masks).
    for (const auto &explicitBinding : desc.bindings)
    {
        const auto it = bindings.find({explicitBinding.set, explicitBinding.binding});
        if (it == bindings.end() || it->second.type != explicitBinding.type ||
            it->second.count != explicitBinding.count ||
            (it->second.stages & explicitBinding.stages) != it->second.stages)
        {
            throw std::runtime_error("Explicit descriptor layout conflicts with shader reflection");
        }
        it->second = explicitBinding;
    }
    std::vector<PipelineBinding> resolved;
    for (auto &entry : bindings)
    {
        auto &binding = entry.second;
        // The existing external GlobalUbo layout is shared by all graphics passes.
        if (desc.type == PipelineType::Graphics && binding.set == kDescriptorSetIndexView)
        {
            binding.stages |= VK_SHADER_STAGE_ALL_GRAPHICS;
        }
        resolved.push_back(std::move(binding));
    }
    desc.bindings = std::move(resolved);
    desc.pushConstantSize = pushSize;
    desc.pushConstantStages = pushStages;
}
} // namespace jhb::rg
