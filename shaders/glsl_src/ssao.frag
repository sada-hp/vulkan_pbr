#version 460
#include "ubo.glsl"

layout(binding = 1) uniform sampler2D HDRNormals;

layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(0.0);
}