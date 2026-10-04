// Same set 3 layout as G-buffer material descriptors; only base color is sampled here.
layout(set = 3, binding = 0) uniform sampler2D samplerColorMap;
layout(set = 3, binding = 1) uniform sampler2D samplerNormalMap;
layout(set = 3, binding = 2) uniform sampler2D samplerOcclusionMap;
layout(set = 3, binding = 3) uniform sampler2D samplerEmissiveMap;
layout(set = 3, binding = 4) uniform sampler2D samplerMetallicRoughnessMap;

layout(push_constant) uniform AlphaConstants
{
    layout(offset = 64) float baseColorAlpha;
    float alphaMaskEnabled;
    float alphaCutoff;
    float padding;
} material;

void applyAlphaMask(vec2 uv)
{
    if (material.alphaMaskEnabled == 1.0)
    {
        float finalAlpha = texture(samplerColorMap, uv).a * material.baseColorAlpha;
        if (finalAlpha < material.alphaCutoff)
        {
            discard;
        }
    }
}
