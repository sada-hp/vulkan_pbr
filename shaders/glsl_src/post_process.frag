#version 460
#include "ubo.glsl"
#include "fxaa.glsl"

layout(binding = 1) uniform sampler2D SceneColor;

layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(fxaa_apply(SceneColor, UV, ubo.Resolution), 1.0);
}