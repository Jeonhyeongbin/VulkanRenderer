#version 450

layout (location = 0) in vec3 texCoord;
layout (location = 0) out vec4 outColor;

layout (set=2,binding = 0) uniform sampler2D skybox;

struct PointLight{
	vec4 position; // w is  just for allign
	vec4 color; // w is intensity
};

layout(set=0, binding = 0) uniform GlobalUbo{
	mat4 projection;
	mat4 view;
	mat4 invView;
	vec4 ambientLightColor;
	PointLight pointLights[10];
	int numLights;
} ubo;

layout(push_constant) uniform Push{
	mat4 modelMatrix;
	mat4 normalMatrix;
} push;

const vec2 invPi = vec2(0.1591549, 0.3183098862);

vec2 SampleSphericalMap(vec3 v)
{
	return vec2(atan(v.z, v.x), asin(v.y))* invPi + 0.5;
}

void main(){
	vec2 uv = SampleSphericalMap(normalize(texCoord));
	vec3 color = texture(skybox, uv).rgb;
	outColor = vec4(color, 1.0);
}
