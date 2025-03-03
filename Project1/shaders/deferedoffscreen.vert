#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(location=2) in vec3 normal;
layout(location=3) in vec2 uv;
layout(location=4) in vec4 tangent;
layout (location = 5) in vec3 instancePos;
layout (location = 6) in vec3 instanceRot;
layout (location = 7) in float instanceScale;
layout (location = 8) in float roughness;
layout (location = 9) in float metallic;
layout (location = 10) in float r;
layout (location = 11) in float g;
layout (location = 12) in float b;

layout(location=0) out vec3 fragColor;
layout(location=1) out vec3 fragPosWorld;
layout(location=2) out vec3 fragNormalWorld;
layout(location=3) out vec2 fraguv;
layout(location=4) out vec4 fragTangent;
layout (location = 5) out float fragroughness;
layout (location = 6) out float fragmetallic;
layout (location = 7) out float fr;
layout (location = 8) out float fg;
layout (location = 9) out float fb;
layout (location = 10) out vec3 outlightpos;

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
	mat4 model;
} push;

void main(){
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

	fragroughness = roughness;
	fragmetallic = metallic;
	fr = r;
	fg = g;
	fb = b;
	vec4 positionWorld = push.model* vec4(position*rotMat + instancePos, 1.0);
	gl_Position =  ubo.projection * ubo.view * positionWorld;
	fraguv = uv;
	fragNormalWorld = normalize(transpose(mat3(push.model)) * normal);
	fragTangent = vec4(normalize(mat3(push.model)* tangent.xyz), tangent.w);
	fragPosWorld = positionWorld.xyz;
	fragColor = color;
	outlightpos = ubo.pointLights[0].position.xyz;
}