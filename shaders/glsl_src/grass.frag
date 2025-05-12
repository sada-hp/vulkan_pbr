#version 460
#include "ubo.glsl"
#include "brdf.glsl"
#include "hextiling.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outDeferred;

layout(location = 0) in vec4 CenterPosition;
layout(location = 1) in vec4 WorldPosition;
layout(location = 2) in float AO;
layout(location = 3) in vec3 Normal;

layout(set = 2, binding = 0) uniform sampler2DArray AlbedoMap;
layout(set = 2, binding = 1) uniform sampler2DArray NormalHeightMap;
layout(set = 2, binding = 2) uniform sampler2DArray ARMMap;

void main()
{
    vec3 dX = dFdxFine(WorldPosition.xyz + ubo.CameraPosition.xyz);
    vec3 dY = dFdyFine(WorldPosition.xyz + ubo.CameraPosition.xyz);
    vec3 N = normalize(cross(dY.xyz, dX.xyz));

    // fake transcluency
    N = normalize(N + mix(0.85, 1.0, AO) * ubo.SunDirection.xyz);

    vec3 avgAlbedo = texture(AlbedoMap, vec3((ubo.CameraPosition + CenterPosition).xz * 5e-4, 0)).rgb;

    outColor = vec4(mix(avgAlbedo, vec3(0.0, 0.25, 0.0), 0.15), 1.0);
    outNormal = vec4(N, 1.0);
    outDeferred = vec4(AO, 1.0, 0.0, 0.1);
}