#version 450
#extension GL_GOOGLE_include_directive : require
#include "materialAlpha.glsl"

layout(location = 0) flat in uint objectId;
layout(location = 1) in vec2 inUV;
layout(location = 0) out uint outId;

void main()
{
    applyAlphaMask(inUV);
    outId = objectId;
}
