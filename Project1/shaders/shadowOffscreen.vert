#version 450

layout(location=0) in vec3 inPos;
layout(location=1) in vec3 color;
layout(location=2) in vec3 normal;
layout(location=3) in vec2 uv;
layout(location=4) in vec4 tangent;
layout (location = 5) in vec3 instancePos;
layout (location = 6) in vec3 instanceRot;

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
	mat3 mx, my, mz;
	
	// rotate around x
	float s = sin(instanceRot.x );
	float c = cos(instanceRot.x);

	mx[0] = vec3(c, s, 0.0);
	mx[1] = vec3(-s, c, 0.0);
	mx[2] = vec3(0.0, 0.0, 1.0);
	
	// rotate around y
	s = sin(instanceRot.y);
	c = cos(instanceRot.y);

	my[0] = vec3(c, 0.0, s);
	my[1] = vec3(0.0, 1.0, 0.0);
	my[2] = vec3(-s, 0.0, c);
	
	// rot around z
	s = sin(instanceRot.z);
	c = cos(instanceRot.z);	
	
	mz[0] = vec3(1.0, 0.0, 0.0);
	mz[1] = vec3(0.0, c, s);
	mz[2] = vec3(0.0, -s, c);
	
	mat3 rotMat = mz * my * mx;

	gl_Position = ubo.projection * pushConsts.view* ubo.model * pushConsts.gltfmodel * vec4(inPos*rotMat + instancePos, 1.0);

	outPos = pushConsts.gltfmodel* vec4(inPos, 1.0);	
	outLightPos = ubo.lightPos.xyz; 
}