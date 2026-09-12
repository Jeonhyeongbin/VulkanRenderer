#version 450

layout(location = 0) in vec3 position;
layout(location = 5) in vec3 instancePos;
layout(location = 0) flat out uint objectId;

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
    layout(offset = 64) uint baseId;
} push;

void main()
{
    // Match the current PBR geometry transform.
    vec4 worldPosition = push.model * vec4(position + instancePos, 1.0);
    gl_Position = ubo.projection * ubo.view * worldPosition;
    objectId = push.baseId + uint(gl_InstanceIndex);
}
