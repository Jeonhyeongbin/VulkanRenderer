#version 450
#extension GL_GOOGLE_include_directive : require
#include "materialAlpha.glsl"

layout(location = 0) out float outFragColor;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inLightPos;
layout(location = 2) in vec2 inUV;

void main()
{
    applyAlphaMask(inUV);
    // Keep the existing R32F world-space distance representation for PBR.
    outFragColor = length(inPos - inLightPos);
}
