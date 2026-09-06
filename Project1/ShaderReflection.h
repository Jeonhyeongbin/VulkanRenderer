#pragma once

namespace jhb::rg
{
struct PipelineDesc;

// Resolves shader bindings and push constants without creating Vulkan objects.
// Throws on missing/invalid shaders or incompatible cross-stage declarations.
void reflectPipelineInterface(PipelineDesc &desc);
} // namespace jhb::rg
