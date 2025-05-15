#version 460
#include "ubo.glsl"
#include "brdf.glsl"
#include "hextiling.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outDeferred;

layout(location = 0) in vec4 ObjectCenter;
layout(location = 1) in vec4 FragNormal;
layout(location = 2) in vec3 SlopeNormal;

void main()
{
    vec2 WorldUV = vec2(0.0);
    if (abs(ObjectCenter.y) > abs(ObjectCenter.x) && abs(ObjectCenter.y) > abs(ObjectCenter.z))
        WorldUV = ObjectCenter.xz;
    else if (abs(ObjectCenter.z) > abs(ObjectCenter.x) && abs(ObjectCenter.z) > abs(ObjectCenter.y))
        WorldUV = ObjectCenter.xy;
    else
        WorldUV = ObjectCenter.yz;

    // fake transcluency
    float AO = smootherstep(0.0, 1.0, FragNormal.w);
    vec3 N = normalize(normalize(FragNormal.xyz) + mix(0.5, 1.0, AO) * ubo.SunDirection.xyz);

    outColor = vec4(WorldUV, 0.0, 200.5);
    outNormal = vec4(SlopeNormal, ObjectCenter.w);
    outDeferred = vec4(AO, N);
}