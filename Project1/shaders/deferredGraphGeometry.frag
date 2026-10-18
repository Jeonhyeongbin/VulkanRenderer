#version 450
#extension GL_GOOGLE_include_directive : require
#include "materialResources.glsl"

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

// Scene supplies neutral fallback textures for absent maps.
layout (constant_id = 2) const bool isMetallicRoughness = true;
layout (constant_id = 3) const bool isEmissive = true;
layout (constant_id = 4) const bool isOcclusion = true;

#define NEAR_PLANE 0.1f
#define FAR_PLANE 1024

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
	vec3 linOut = mix(pow((srgbIn.rgb + 0.055) / 1.055, vec3(2.4)),
                     srgbIn.rgb / 12.92, lessThanEqual(srgbIn.rgb, vec3(0.04045)));
	return vec4(linOut,srgbIn.w);
}

float linearDepth(float depth)
{
	float z = depth * 2.0f - 1.0f; 
	return (2.0f * NEAR_PLANE * FAR_PLANE) / (FAR_PLANE + NEAR_PLANE - z * (FAR_PLANE - NEAR_PLANE));	
}

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;
layout (location = 3) out vec4 outMaterial;
layout (location = 4) out vec4 outEmmisive;

vec3 calculateNormal()
{
	vec3 N = normalize(fragNormalWorld);
    if (material.normalOcclusion.z > 0.5 && !gl_FrontFacing) N = -N;
	vec3 T;
    vec3 B;
    if (abs(fragtangent.w) > 0.5 && dot(fragtangent.xyz, fragtangent.xyz) > 1e-10)
    {
        T = normalize(fragtangent.xyz - N * dot(N, fragtangent.xyz));
        B = cross(N, T) * sign(fragtangent.w);
    }
    else
    {
        // glTF meshes without TANGENT: derive the basis from the actual UVs.
        vec3 dx = dFdx(fragPosWorld), dy = dFdy(fragPosWorld);
        vec2 du = dFdx(fraguv), dv = dFdy(fraguv);
        float det = du.x * dv.y - du.y * dv.x;
        if (abs(det) < 1e-10) return N;
        vec3 rawT = (dx * dv.y - dy * du.y) / det;
        vec3 rawB = (dy * du.x - dx * dv.x) / det;
        T = rawT - N * dot(N, rawT);
        if (dot(T, T) < 1e-10) return N;
        T = normalize(T);
        B = cross(N, T) * (dot(cross(N, T), rawB) < 0.0 ? -1.0 : 1.0);
    }
	mat3 TBN = mat3(T, B, N);

	vec3 normaltext = texture(samplerNormalMap, fraguv).xyz * 2.0 - 1.0;
    normaltext.xy *= material.normalOcclusion.x;
    normaltext = normalize(normaltext);
	return TBN*normaltext;
}

void main() {
    outMaterial = vec4(1.0, 1.0, 0.0, 1.0);
    outEmmisive = vec4(0.0);
	outAlbedo = SRGBtoLINEAR(texture(samplerColorMap, fraguv)) * vec4(fragColor, 1.0) * material.baseColor;
	if (material.metallicRoughnessAlpha.z > 0.5) {
		if (outAlbedo.a < material.metallicRoughnessAlpha.w) {
			discard;
		}
	}

	outPosition = vec4(fragPosWorld, 1.0);
	outNormal = vec4(normalize(calculateNormal())*0.5+0.5,0);

	// Store linearized depth in alpha component
	outPosition.a = linearDepth(gl_FragCoord.z);

	if(isMetallicRoughness)
	{
		outMaterial.gb = texture(samplerMetallicRoughnessMap, fraguv).gb * material.metallicRoughnessAlpha.yx;
	}
	else{
		outMaterial.b = 0;
		outMaterial.g = 1;
	}
	
	outMaterial.r=1;
	if(isOcclusion)
	{
		outMaterial.r = mix(1.0, texture(samplerOcclusionMap, fraguv).r, material.normalOcclusion.y);
	}

	outEmmisive.rgb = vec3(0);
	if(isEmissive)
	{
		outEmmisive.rgb = SRGBtoLINEAR(texture(samplerEmissiveMap, fraguv)).rgb * material.emissive.rgb;
	}
	
	// Write color attachments to avoid undefined behaviour (validation error)
}
