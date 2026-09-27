#version 450

layout(location = 0) in vec3 texCoord;
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform samplerCube skybox;
struct PointLight { vec4 position; vec4 color; };
layout(set = 1, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 invView;
    vec4 ambientLightColor;
    PointLight pointLights[10];
    int numLights;
    float exposure;
    float gamma;
} ubo;

vec3 tonemap(vec3 x)
{
    return ((x * (0.15 * x + 0.05) + 0.004) /
            (x * (0.15 * x + 0.50) + 0.06)) - 0.02 / 0.30;
}

void main()
{
    // pisa_cube.ktx stores sky toward -Y; Scene uses +Y up.
    vec3 color = texture(skybox, texCoord * vec3(1.0, -1.0, 1.0)).rgb;
    color = tonemap(max(color, vec3(0.0)) * ubo.exposure) / tonemap(vec3(11.2));
    outColor = vec4(color, 1.0);
}
