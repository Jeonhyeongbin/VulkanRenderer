#version 450

layout(location = 0) in vec3 inPos;
layout(location = 5) in vec3 instancePos;

layout(location = 0) out vec3 outPos;
layout(location = 1) out vec3 outLightPos;

// Per-face data is owned and uploaded by RenderGraph.
layout(set = 3, binding = 0) uniform UBO
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
    // Match the current PBR shader's world-position transform.
    vec4 worldPosition = pushConsts.model * vec4(inPos + instancePos, 1.0);
    gl_Position = ubo.viewProjection * worldPosition;
    outPos = worldPosition.xyz;
    outLightPos = ubo.lightPosition.xyz;
}
