#version 460
#include "fxaa.glsl"

layout(binding = 0) uniform sampler2D HDRColor;
layout(binding = 1) uniform sampler2D Blurred;

layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 outColor;

vec3 Tonemap(vec3 L) 
{
    const float exposure = 0.8;
    const float gamma = 2.2;

    L = exposure * L;
    L = L / (0.15 * L + vec3(1.0));

    L.r = L.r < 1.413 ? pow(L.r * 0.38317, 1.0 / gamma) : 1.0 - exp(-L.r);
    L.g = L.g < 1.413 ? pow(L.g * 0.38317, 1.0 / gamma) : 1.0 - exp(-L.g);
    L.b = L.b < 1.413 ? pow(L.b * 0.38317, 1.0 / gamma) : 1.0 - exp(-L.b);

    return L;
}

void main()
{
    vec3 Color = fxaa_apply(HDRColor, UV, textureSize(HDRColor, 0).xy);
    Color = Tonemap(Color + texture(Blurred, UV).rgb);
    outColor = vec4(Color, 1.0);
}