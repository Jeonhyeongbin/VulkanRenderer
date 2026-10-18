#include "materialResources.glsl"

void applyAlphaMask(vec2 uv)
{
    if (material.metallicRoughnessAlpha.z == 1.0)
    {
        float finalAlpha = texture(samplerColorMap, uv).a * material.baseColor.a;
        if (finalAlpha < material.metallicRoughnessAlpha.w)
        {
            discard;
        }
    }
}
