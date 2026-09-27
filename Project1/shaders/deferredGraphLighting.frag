#version 450


layout (location = 0) in vec2 fraguv;

#define EPSILON 0.15
#define SHADOW_OPACITY 0.1

layout (set = 2, binding = 0) uniform sampler2D inputPosition;
layout (set = 2, binding = 1) uniform sampler2D inputNormal;
layout (set = 2, binding = 2) uniform sampler2D inputAlbedo;
layout (set = 2, binding = 3) uniform sampler2D inputMaterial;
layout (set = 2, binding = 4) uniform sampler2D inputEmmisive;


layout (set = 2, binding = 5) uniform sampler2D samplerBRDFLUT;
layout (set = 2, binding = 6) uniform samplerCube samplerIrradiance;
layout (set = 2, binding = 7) uniform samplerCube prefilteredMap;
layout (set = 2, binding = 8) uniform samplerCube shadowMap;

struct PointLight{
	vec4 position; // w is  just for allign
	vec4 color; // w is intensity
} light;

layout(set=1, binding = 0) uniform GlobalUbo{
	mat4 projection;
	mat4 view;
	mat4 invView;
	vec4 ambientLightColor;
	PointLight pointLights[10];
	int numLights;
	float exposure;
	float gamma;
} ubo;

layout (location = 0) out vec4 outColor;

#define PI 3.1415926535897932384626433832795

// From http://filmicgames.com/archives/75
vec3 Uncharted2Tonemap(vec3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom);
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 SpecularAndDiffuseContribution(vec3 L, vec3 V, vec3 N, vec3 F0, float metallic, float roughness, vec4 LColor, vec4 albedo)
{
	float distanceSquared = max(dot(L, L), 0.0001);
	L *= inversesqrt(distanceSquared);
	// Precalculate vectors and dot products
	vec3 H = normalize (V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = max(dot(N, V), 0.001);
	float dotNL = max(dot(N, L), 0.0);

	vec3 color = vec3(0.0);

	float attenuation = 1.0 / distanceSquared;
	vec3 radiance = LColor.rgb * LColor.w * attenuation;

	if (dotNL > 0.0) {
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness);
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(max(dot(V, H), 0.0), F0);
		vec3 spec = D * F * G / (4.0 * dotNL * dotNV);
		vec3 diffuse = albedo.rgb * (1.0 - metallic) / PI;
		diffuse*=(1.0 - F);
		color += (diffuse + spec) * radiance* dotNL;
	}

	return color;
}

vec3 getIBLContribution(vec3 V, vec3 N, vec3 R,float roughness, float metallic, vec3 baseColor)
{
	vec3 f0 = vec3(0.04);

	vec3 diffuseColor = baseColor.rgb * (vec3(1.0) - f0) * (1.0 - metallic);
	vec3 specularColor = mix(f0, baseColor.rgb, metallic); 

	// Use the generated prefilter chain's actual mip count.
	float lod = roughness * float(textureQueryLevels(prefilteredMap) - 1);

	vec2 brdf = texture(samplerBRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;

	// Match the skybox's conversion from Scene directions to pisa cube directions.
	vec3 specularLight = textureLod(prefilteredMap, R * vec3(1, -1, 1), lod).rgb;
	vec3 diffuseLight = texture(samplerIrradiance, N * vec3(1, -1, 1)).rgb;

	// Specular reflectance
	vec3 specular = specularLight * (specularColor * brdf.x + brdf.y);
	vec3 diffuse = diffuseLight * diffuseColor;

	return specular + diffuse;
}

void main() {
    if (texelFetch(inputMaterial, ivec2(gl_FragCoord.xy), 0).a == 0.0) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

	vec3 cameraPosWorld = ubo.invView[3].xyz;
	vec3 occlusionMaterialRoughness = texelFetch(inputMaterial, ivec2(gl_FragCoord.xy), 0).rgb;
	vec3 fragPosWorld = texelFetch(inputPosition, ivec2(gl_FragCoord.xy), 0).xyz;
	float metallic = occlusionMaterialRoughness.b;
	float roughness = clamp(occlusionMaterialRoughness.g, 0.045, 1.0);
	float occulsion = texelFetch(inputMaterial, ivec2(gl_FragCoord.xy), 0).r;

	vec3 N = normalize(texelFetch(inputNormal, ivec2(gl_FragCoord.xy), 0).xyz*2-1);
	vec3 V = normalize(cameraPosWorld - fragPosWorld);
	vec3 R = reflect(-V, N);

	vec3 F0 = vec3(0.04);

	vec4 albedo = texelFetch(inputAlbedo, ivec2(gl_FragCoord.xy), 0);

	F0 = mix(F0, albedo.rgb, metallic);


	vec3 Lo = vec3(0.0);

	// per light
	vec3 iblColor = getIBLContribution(V, N, R, roughness, metallic, albedo.rgb);

	// Shadow
	vec3 lightVec = fragPosWorld - ubo.pointLights[0].position.xyz;
	float dist = length(lightVec);
	lightVec /= max(dist, 0.0001);

    float shadow = 0.f;
	int range =1;
	// Check if fragment is in shadow
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			for (int z = -range; z <= range; z++)
			{
				float sampledDist = texture(shadowMap, normalize(lightVec + vec3(x,y, z) *(0.01))).r; 
				shadow += (dist - EPSILON <= sampledDist ) ? 1.0 : SHADOW_OPACITY;
			}
		}
	}
	shadow /=27;

    // Only light 0 owns the current shadow cube. Preserve unshadowed IBL/emission.
    for (int i = 0; i < min(ubo.numLights, 10); ++i) {
        vec3 L = ubo.pointLights[i].position.xyz - fragPosWorld;
        Lo += SpecularAndDiffuseContribution(L, V, N, F0, metallic, roughness,
                                             ubo.pointLights[i].color, albedo)
              * (i == 0 ? shadow : 1.0);
    }
    vec3 emission = texelFetch(inputEmmisive, ivec2(gl_FragCoord.xy), 0).rgb;
    vec3 color = Lo + iblColor * occulsion + emission;
    color = Uncharted2Tonemap(max(color, vec3(0.0)) * ubo.exposure);
    color /= Uncharted2Tonemap(vec3(11.2));
    // Keep linear output; the sRGB swapchain performs the display conversion.
    outColor = vec4(color, 1.0);
}
