#version 450

layout(location=0) in vec3 pos;
layout(location=0) out vec3 texCoord;

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

void main(){
  texCoord = pos;
	mat4 view =  mat4(mat3(ubo.view));
  gl_Position = ubo.projection * view * vec4(pos.xyz, 1);
}
