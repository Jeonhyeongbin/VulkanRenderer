#version 450


layout (location = 0) in vec2 fraguv;

#define EPSILON 0.15
#define SHADOW_OPACITY 0.5

layout (set = 0, input_attachment_index = 0, binding = 0) uniform subpassInput inputPosition;
layout (set = 0, input_attachment_index = 1, binding = 1) uniform subpassInput inputNormal;
layout (set = 0, input_attachment_index = 2, binding = 2) uniform subpassInput inputAlbedo;
layout (set = 0, input_attachment_index = 3, binding = 3) uniform subpassInput inputMaterial;
layout (set = 0, input_attachment_index = 4, binding = 4) uniform subpassInput inputEmmisive;


layout (set = 2, binding = 0) uniform sampler2D samplerBRDFLUT;
layout (set = 2, binding = 1) uniform samplerCube samplerIrradiance;
layout (set = 2, binding = 2) uniform samplerCube prefilteredMap;
layout (set = 3, binding = 0) uniform samplerCube shadowMap;

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

layout (constant_id = 0) const bool ALPHA_MASK = false;
layout (constant_id = 1) const float ALPHA_MASK_CUTOFF = 0.0f;
layout (constant_id = 2) const bool isMetallicRoughness = true;

layout (location = 0) out vec4 outColor;

#define PI 3.1415926535897932384626433832795

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
	vec3 linOut = pow(srgbIn.xyz,vec3(2.2));
	return vec4(linOut,srgbIn.w);
}

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
vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 prefilteredReflection(vec3 R, float roughness)
{
	const float MAX_REFLECTION_LOD = 5; // todo: param/const
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	vec3 a = textureLod(prefilteredMap, R, lodf).rgb;
	vec3 b = textureLod(prefilteredMap, R, lodc).rgb;
	return mix(a, b, lod - lodf);
}

vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, float metallic, float roughness, vec4 LColor, vec4 albedo)
{
	// Precalculate vectors and dot products
	vec3 H = normalize (V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);

	// Light color fixed
	vec3 lightColor = ubo.pointLights[0].color.xyz;

	vec3 color = vec3(0.0);

	float dist = length(L);
	float attenuation = 1.0 / (dist * dist);
	vec3 radiance = LColor.rgb * LColor.w * attenuation;

	if (dotNL > 0.0) {
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness);
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, F0);
		vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);
		vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
		color += (kD * albedo.rgb / PI + spec) * radiance* dotNL;
	}

	return color;
}

// vec3 calculateNormal()
// {
// 	vec3 tangentNormal = texture(samplerNormalMap, fraguv).xyz;

// 	vec3 N = normalize(fragNormalWorld);
// 	vec3 T = normalize(fragtangent.xyz);
// 	vec3 B = normalize(cross(N, T));
// 	mat3 TBN = mat3(T, B, N);

// 	return normalize(TBN * tangentNormal);
// }

void main() {
	vec3 cameraPosWorld = ubo.invView[3].xyz;
	vec3 fragPosWorld = subpassLoad(inputPosition).rgb;
	float metallic = 0;
	float roughness = 1;
	float occulsion = subpassLoad(inputMaterial).r;
	vec3 N = subpassLoad(inputNormal).rgb*2.0-1;

	if(isMetallicRoughness)
	{
		metallic=subpassLoad(inputMaterial).b;
		float roughness = subpassLoad(inputMaterial).g;
	}

	vec3 V = normalize(cameraPosWorld - fragPosWorld);
	vec3 R = reflect(-V, N);

	vec3 F0 = vec3(0.04);

	vec4 albedo = subpassLoad(inputAlbedo);
	if(albedo.a == -1)
	{
		outColor = vec4(albedo.rgb, 1);
	}
	else{
	F0 = mix(F0, albedo.rgb, metallic);

	if (ALPHA_MASK) {
		if (albedo.a < ALPHA_MASK_CUTOFF) {
			discard;
		}
	}
	vec3 occlusionMaterialRoughness = subpassLoad(inputMaterial).rgb;
	vec3 Lo = vec3(0.0);
	for(int i = 0; i < ubo.pointLights.length(); i++) {
		vec3 L = normalize(ubo.pointLights[i].position.xyz - fragPosWorld);
		Lo += specularContribution(L, V, N, F0, metallic, roughness, ubo.pointLights[i].color, albedo);
	}

	vec2 brdf = texture(samplerBRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
	vec3 reflection = prefilteredReflection(R, roughness).rgb;
	vec3 irradiance = texture(samplerIrradiance, N).rgb;

	// Diffuse based on irradiance
	vec3 diffuse = irradiance * albedo.rgb;

	vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);

	// Specular reflectance
	vec3 specular = reflection * (F * brdf.x + brdf.y);

	// Ambient part
	vec3 kD = 1.0 - F;
	kD *= 1.0 - roughness;
	//vec3 ambient = (kD * diffuse + specular) * occulsion;
	vec3 ambient = (kD * diffuse + specular);
	

	vec3 color = ambient + Lo;

	// Tone mapping
	color = Uncharted2Tonemap(color * ubo.exposure);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
	// Gamma correction
	color = pow(color, vec3(1.0f / ubo.gamma));

	// Shadow
	vec3 lightVec = fragPosWorld - ubo.pointLights[0].position.xyz;
    float sampledDist = texture(shadowMap, lightVec).r;
    float dist = length(lightVec);

	// Check if fragment is in shadow
    float shadow = (dist <= sampledDist + EPSILON) ? 1.0 : SHADOW_OPACITY;
	//vec4 emission = vec4(SRGBtoLINEAR(subpassLoad(inputEmmisive)).rgb,1);
	outColor = vec4(color, 1.0);
	outColor *= shadow;
	}
}
