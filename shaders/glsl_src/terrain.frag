#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "brdf.glsl"
#include "hextiling.glsl"

layout(push_constant) uniform constants
{
    layout(offset = 80) vec4 ColorMask;
    layout(offset = 96) float RoughnessMultiplier;
    layout(offset = 100) float Metallic;
    layout(offset = 104) float HeightScale;
}
PushConstants;

layout(location = 0) in vec4 WorldPosition;
layout(location = 1) in vec4 NormalData;
layout(location = 2) in float Height;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outDeferred;

// float SampleWater(vec3 UVW)
// {
//     return max(textureLod(WaterMap, UVW, 0).r, textureLod(WaterMap, vec3(UVW.xy * 0.5 + 0.25, UVW.z + 1), 0).r);
// }

void main()
{
    vec2 WorldUV = vec2(0.0);
    if (abs(WorldPosition.y) > abs(WorldPosition.x) && abs(WorldPosition.y) > abs(WorldPosition.z))
        WorldUV = WorldPosition.xz;
    else if (abs(WorldPosition.z) > abs(WorldPosition.x) && abs(WorldPosition.z) > abs(WorldPosition.y))
        WorldUV = WorldPosition.xy;
    else
        WorldUV = WorldPosition.yz;

    outColor = vec4(WorldUV, Height, 100.5);
    outNormal = NormalData;
    outDeferred = vec4(0.0, 0.0, 0.0, 0.0);
}