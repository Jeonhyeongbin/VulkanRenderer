#version 450

layout (location = 0) in vec3 texCoord;
layout (location = 1) in vec4 fragpos;

layout (set=1,binding = 0) uniform samplerCube skybox;

struct PointLight{
	vec4 position; // w is  just for allign
	vec4 color; // w is intensity
} light;

layout(set=0, binding = 0) uniform GlobalUbo{
	mat4 projection;
	mat4 view;
	mat4 invView;
	vec4 ambientLightColor;
	PointLight pointLights[10];
	int numLights;
	float exposure;
	float gamma;
} ubo;

#define NEAR_PLANE 0.1f
#define FAR_PLANE 2

float linearDepth(float depth)
{
	float z = depth * 2.0f - 1.0f; 
	return (2.0f * NEAR_PLANE * FAR_PLANE) / (FAR_PLANE + NEAR_PLANE - z * (FAR_PLANE - NEAR_PLANE));	
}

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 outPosition;
layout (location = 2) out vec4 outNormal;
layout (location = 3) out vec4 outAlbedo;
layout (location = 4) out vec4 outMaterial;
layout (location = 5) out vec4 outEmmisive;


void main() {
	outPosition = fragpos;

	vec3 N = vec3(0.f, 0.f,0.f);
	outNormal = vec4(N, 1.0);

	outAlbedo.rgb = texture(skybox, texCoord).rgb;
	outAlbedo.a = -1;

	// Store linearized depth in alpha component
	outPosition.a = linearDepth(gl_FragCoord.z);
	outMaterial = vec4(0);
	outEmmisive.rgb = vec3(0);

	// Write color attachments to avoid undefined behaviour (validation error)
	outColor = vec4(0.0);
}
