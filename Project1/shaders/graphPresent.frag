#version 450
layout(set=2, binding=0) uniform sampler2D sourceImage;
layout(location=0) out vec4 color;
void main() {
    color = texelFetch(sourceImage, ivec2(gl_FragCoord.xy), 0);
}
