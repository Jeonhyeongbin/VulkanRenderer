#version 450


layout (location = 0) in vec2 fraguv;

#define EPSILON 0.15
#define SHADOW_OPACITY 0.1

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

vec3 SpecularAndDiffuseContribution(vec3 L, vec3 V, vec3 N, vec3 F0, float metallic, float roughness, vec4 LColor, vec4 albedo)
{
	// Precalculate vectors and dot products
	vec3 H = normalize (V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = clamp(abs(dot(N, V)), 0.001, 1.0);
	float dotNL = clamp(dot(N, L), 0.001, 1.0);

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
		vec3 spec = D * F * G / (4.0 * dotNL * dotNV);
		vec3 diffuse = ((albedo.rgb) * (vec3(1.0)-F0) * (1.0 - metallic))/PI;
		diffuse*=(1.0 - F);
		color += (diffuse + spec) * radiance* dotNL;
	}

	return color;
}

vec3 getIBLContribution(vec3 V, vec3 N, vec3 R,float roughness, float metallic, vec3 baseColor)
{
	vec3 f0 = vec3(0.04);

	vec3 diffuseColor = baseColor.rgb * (vec3(1.0) - f0);
	vec3 specularColor = mix(f0, baseColor.rgb, metallic); 

	// hard coding mip levels. need to dynamic data using ubo.
	float lod = roughness * 7;

	vec2 brdf = texture(samplerBRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;

	vec3 specularLight = textureLod(prefilteredMap, R, lod).rgb; // same as reflection
	vec3 diffuseLight = texture(samplerIrradiance, N).rgb; // same as irradiance

	// Specular reflectance
	vec3 specular = specularLight * (specularColor * brdf.x + brdf.y);
	vec3 diffuse = diffuseLight * diffuseColor;

	return specular + diffuse;
}

void main() {
	vec3 cameraPosWorld = ubo.invView[3].xyz;
	vec3 occlusionMaterialRoughness = subpassLoad(inputMaterial).rgb;
	vec3 fragPosWorld = subpassLoad(inputPosition).xyz;
	float metallic = occlusionMaterialRoughness.b;
	float roughness = occlusionMaterialRoughness.g;
	float occulsion = subpassLoad(inputMaterial).r;

	vec3 N = normalize(subpassLoad(inputNormal).xyz*2-1);
	vec3 V = normalize(cameraPosWorld - fragPosWorld);
	vec3 R = reflect(-V, N);

	vec3 F0 = vec3(0.04);

	vec4 albedo = subpassLoad(inputAlbedo);

	F0 = mix(F0, albedo.rgb, metallic);


	vec3 Lo = vec3(0.0);

	// per light
	vec3 L = normalize(ubo.pointLights[0].position.xyz - fragPosWorld);
	Lo += SpecularAndDiffuseContribution(L, V, N, F0, metallic, roughness, ubo.pointLights[0].color, albedo);

	vec3 iblColor = getIBLContribution(V, N, R, roughness, metallic, albedo.rgb);

	vec3 color = iblColor+ Lo;

	// Tone mapping
	color = Uncharted2Tonemap(color * ubo.exposure);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
	// Gamma correction
	color = pow(color, vec3(1.0f / ubo.gamma));

	// Shadow
	vec3 lightVec = fragPosWorld - ubo.pointLights[0].position.xyz;
	float dist = length(lightVec);
	lightVec=normalize(lightVec);
    float sampledDist = texture(shadowMap, lightVec).r; 
    
	// Check if fragment is in shadow
    float shadow = (dist - EPSILON <= sampledDist) ? 1.0 : SHADOW_OPACITY;

	const float u_OcclusionStrength = 1.0f;
	color = mix( color, color * occulsion, u_OcclusionStrength);

	vec3 emission = SRGBtoLINEAR(subpassLoad(inputEmmisive)).rgb;
	color+=emission;

	outColor = vec4(color, 1.0);
	outColor *= shadow;
}
