// Matches Scene::GpuMaterial (std430, 96 bytes).
struct MaterialData
{
    vec4 baseColor;
    vec4 emissive;
    vec4 metallicRoughnessAlpha; // metallic, roughness, alpha-mask enabled, cutoff
    vec4 normalOcclusion;       // normal scale, AO strength, double-sided, unused
    uint baseColorTexture;
    uint normalTexture;
    uint occlusionTexture;
    uint emissiveTexture;
    uint metallicRoughnessTexture;
    uint padding0;
    uint padding1;
    uint padding2;
};

// Matches kBindlessTextureCapacity. All entries are initialized before rendering.
layout(set = 0, binding = 0) uniform sampler2D materialTextures[1024];
layout(std430, set = 0, binding = 1) readonly buffer Materials
{
    MaterialData values[];
} materialBuffer;

layout(push_constant) uniform MaterialIndex
{
    layout(offset = 64) uint index;
} materialPush;

// Keep the existing shading expressions; only resource selection changes.
// One material index per draw; the material buffer is immutable while rendering.
#define material materialBuffer.values[materialPush.index]
#define samplerColorMap materialTextures[material.baseColorTexture]
#define samplerNormalMap materialTextures[material.normalTexture]
#define samplerOcclusionMap materialTextures[material.occlusionTexture]
#define samplerEmissiveMap materialTextures[material.emissiveTexture]
#define samplerMetallicRoughnessMap materialTextures[material.metallicRoughnessTexture]
