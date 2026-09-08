#version 450

layout(location = 0) in vec3 pos;
layout(location = 0) out vec3 texCoord;

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

void main()
{
    texCoord = pos;
    mat4 view = mat4(mat3(ubo.view));
    vec4 clip = ubo.projection * view * vec4(pos, 1.0);
    // Standard depth: draw at the far plane, preserving opaque geometry.
    gl_Position = clip.xyww;
}
