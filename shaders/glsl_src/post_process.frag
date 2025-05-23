#version 460
#include "ubo.glsl"

layout(set = 1, binding = 0) uniform sampler2D HDRColor;
layout(set = 1, binding = 1) uniform sampler2D Blurred;

layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 outColor;

vec3 Tonemap(vec3 L) 
{
    L = ubo.Exposure * L;
    L = L / (0.15 * L + vec3(1.0));

    L.r = L.r < 1.413 ? pow(L.r * 0.38317, 1.0 / ubo.Gamma) : 1.0 - exp(-L.r);
    L.g = L.g < 1.413 ? pow(L.g * 0.38317, 1.0 / ubo.Gamma) : 1.0 - exp(-L.g);
    L.b = L.b < 1.413 ? pow(L.b * 0.38317, 1.0 / ubo.Gamma) : 1.0 - exp(-L.b);

    return L;
}

vec3 fxaa()
{
    float FXAA_SPAN_MAX = 16.0;
    float FXAA_REDUCE_MUL = 0.125f;
    float FXAA_REDUCE_MIN = 1.0 / 256.0;

    vec3 rgbNW = Tonemap(texelFetch(HDRColor, ivec2(gl_FragCoord.xy + vec2(-1.0,-1.0)), 0).xyz);
    vec3 rgbNE = Tonemap(texelFetch(HDRColor, ivec2(gl_FragCoord.xy + vec2(1.0,-1.0)), 0).xyz);
    vec3 rgbSW = Tonemap(texelFetch(HDRColor, ivec2(gl_FragCoord.xy + vec2(-1.0,1.0)), 0).xyz);
    vec3 rgbSE = Tonemap(texelFetch(HDRColor, ivec2(gl_FragCoord.xy + vec2(1.0,1.0)), 0).xyz);
    vec3 rgbM  = Tonemap(texelFetch(HDRColor, ivec2(gl_FragCoord.xy), 0).xyz);

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max
    (
        (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
        FXAA_REDUCE_MIN
    );

    float rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2( FXAA_SPAN_MAX,  FXAA_SPAN_MAX),
          max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
          dir * rcpDirMin)) / textureSize(HDRColor, 0).xy;

    vec3 rgbA = (1.0/2.0) * (
        texture(HDRColor, UV.xy + dir * (1.0/3.0 - 0.5)).xyz +
        texture(HDRColor, UV.xy + dir * (2.0/3.0 - 0.5)).xyz);
    vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
        texture(HDRColor, UV.xy + dir * (0.0/3.0 - 0.5)).xyz +
        texture(HDRColor, UV.xy + dir * (3.0/3.0 - 0.5)).xyz);
    float lumaB = dot(Tonemap(rgbB), luma);

    if((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        return rgbA;
    }
    else
    {
        return rgbB;
    }
}

void main()
{
    outColor = vec4(Tonemap(fxaa() + texture(Blurred, UV).rgb), 1.0);
}