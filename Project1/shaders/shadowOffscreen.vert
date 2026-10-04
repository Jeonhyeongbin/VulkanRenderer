#version 450
#extension GL_GOOGLE_include_directive : require
#include "instanceRotation.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 3) in vec2 uv;
layout(location = 5) in vec3 instancePos;
layout(location = 6) in vec3 instanceRot;

layout(location = 0) out vec3 outPos;
layout(location = 1) out vec3 outLightPos;
layout(location = 2) out vec2 outUV;

// Per-face data is owned and uploaded by RenderGraph.
layout(set = 4, binding = 0) uniform UBO
{
    mat4 viewProjection;
    vec4 lightPosition;
} ubo;

layout(push_constant) uniform PushConsts
{
    mat4 model;
} pushConsts;

void main()
{
    vec4 worldPosition = pushConsts.model * vec4(inPos * instanceRotation(instanceRot) + instancePos, 1.0);
    gl_Position = ubo.viewProjection * worldPosition;
    outPos = worldPosition.xyz;
    outLightPos = ubo.lightPosition.xyz;
    outUV = uv;
}
