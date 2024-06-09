#version 450

layout(location=0) in vec3 inPos;
layout(location=1) in vec3 color;
layout(location=2) in vec3 normal;
layout(location=3) in vec2 uv;
layout(location=4) in vec4 tangent;

layout (location = 0) out vec4 outPos;
layout (location = 1) out vec3 outLightPos;

layout (binding = 0) uniform UBO 
{
	mat4 projection;
	mat4 view; 
	mat4 model;
	vec4 lightPos;
} ubo;

layout(push_constant) uniform PushConsts 
{
	mat4 model;
	mat4 view;
	layout(offset=128) mat4 gltfmodel;
} pushConsts;

 
void main()
{
	gl_Position = ubo.projection * pushConsts.view *  pushConsts.model * pushConsts.gltfmodel  * vec4(inPos, 1.0);

	outPos = vec4(inPos, 1.0);	
	outLightPos = ubo.lightPos.xyz; 
}