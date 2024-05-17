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

layout (set = 1, binding = 0) uniform PickingUbo{
	int objId;
} pickingUbo;

layout (location = 0) out ivec3 outColor;


void main() {
	outColor = ivec3(pickingUbo.objId, 0, 0);
}
