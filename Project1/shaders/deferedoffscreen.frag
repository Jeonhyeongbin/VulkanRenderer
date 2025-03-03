#version 450

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec3 fragPosWorld;
layout (location = 2) in vec3 fragNormalWorld;
layout (location = 3) in vec2 fraguv;
layout (location = 4) in vec4 fragtangent;
layout (location = 5) in float fragroughness;
layout (location = 6) in float fragmetallic;
layout (location = 7) in float fr;
layout (location = 8) in float fg;
layout (location = 9) in float fb;
layout (location = 10) in vec3 lightPos;

layout (set = 2, binding = 0) uniform sampler2D samplerColorMap;
layout (set = 2, binding = 1) uniform sampler2D samplerNormalMap;
layout (set = 2, binding = 2) uniform sampler2D samplerOcclusionMap;
layout (set = 2, binding = 3) uniform sampler2D samplerEmissiveMap;
layout (set = 2, binding = 4) uniform sampler2D samplerMetallicRoughnessMap;


layout (constant_id = 0) const bool ALPHA_MASK = false;
layout (constant_id = 1) const float ALPHA_MASK_CUTOFF = 0.0f;
layout (constant_id = 2) const bool isMetallicRoughness = false;
layout (constant_id = 3) const bool isEmissive = false;
layout (constant_id = 4) const bool isOcclusion = false;

#define NEAR_PLANE 0.1f
#define FAR_PLANE 1024

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
	vec3 linOut = pow(srgbIn.xyz,vec3(2.2));
	return vec4(linOut,srgbIn.w);
}

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

vec3 calculateNormal()
{
	vec3 N = normalize(fragNormalWorld);
	vec3 T = normalize(fragtangent.xyz);
	vec3 B = cross(N, T)*fragtangent.w;
	mat3 TBN = mat3(T, B, N);
	return TBN*normalize(texture(samplerNormalMap, fraguv).xyz*2-1);
}

void main() {
	outAlbedo = texture(samplerColorMap, fraguv) * vec4(fragColor, 1.0);
	if (ALPHA_MASK) {
		if (outAlbedo.a < ALPHA_MASK_CUTOFF) {
			discard;
		}
	}

	outPosition = vec4(fragPosWorld, 1.0);
	outNormal = vec4(calculateNormal(),0);

	// Store linearized depth in alpha component
	outPosition.a = linearDepth(gl_FragCoord.z);

	if(isMetallicRoughness)
	{
		outMaterial.gb = texture(samplerMetallicRoughnessMap, fraguv).gb;
	}
	else{
		outMaterial.b = 0;
		outMaterial.g = 1;
	}
	
	outMaterial.r = texture(samplerOcclusionMap, fraguv).r;
	outEmmisive.rgb = texture(samplerEmissiveMap , fraguv).rgb;

	// Write color attachments to avoid undefined behaviour (validation error)
	outColor = vec4(0.0);
}
