#version 450

layout(location = 0) out float outFragColor;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inLightPos;

void main()
{
    // Keep the existing R32F world-space distance representation for PBR.
    outFragColor = length(inPos - inLightPos);
}
