#version 450
#extension GL_GOOGLE_include_directive : require
#include "instanceRotation.glsl"

layout(location = 0) in vec3 position;
layout(location = 3) in vec2 uv;
layout(location = 5) in vec3 instancePos;
layout(location = 6) in vec3 instanceRot;
layout(location = 0) flat out uint objectId;
layout(location = 1) out vec2 outUV;

struct PointLight
{
    vec4 position;
    vec4 color;
};

layout(set = 1, binding = 0) uniform GlobalUbo
{
    mat4 projection;
    mat4 view;
    mat4 invView;
    vec4 ambientLightColor;
    PointLight pointLights[10];
    int numLights;
} ubo;

layout(push_constant) uniform Push
{
    mat4 model;
    layout(offset = 80) uint baseId;
} push;

void main()
{
    vec4 worldPosition = push.model * vec4(position * instanceRotation(instanceRot) + instancePos, 1.0);
    gl_Position = ubo.projection * ubo.view * worldPosition;
    objectId = push.baseId == 0u ? 0u : push.baseId + uint(gl_InstanceIndex);
    outUV = uv;
}
