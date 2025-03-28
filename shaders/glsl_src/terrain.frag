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

layout(set = 1, binding = 1) uniform sampler2D AlbedoMap;
layout(set = 1, binding = 2) uniform sampler2D NormalHeightMap;
layout(set = 1, binding = 3) uniform sampler2D ARMMap;
layout(set = 2, binding = 0) uniform sampler2DArray NoiseMap;
layout(set = 2, binding = 1) uniform sampler2DArray WaterMap;

layout(location = 0) in vec4 WorldPosition;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 UV;
layout(location = 3) in float Height;
layout(location = 4) in flat int Level;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outDeferred;

float SampleWater(vec3 UVW)
{
    float r = textureLod(WaterMap, UVW, 0).r;
    r += textureLod(WaterMap, vec3(UV, Level), 1).r * 0.5;
    r += textureLod(WaterMap, vec3(UV, Level), 2).r * 0.25;
    r += textureLod(WaterMap, vec3(UV, Level), 3).r * 0.1;

    return saturate(pow(1.0 - Height, 2.0) * saturate(max(r - 1500.0, 0.0) / 2850.0 - 0.1));
}

void main()
{
#if 1
    vec3 N = normalize(Normal);
#else
    vec3 dX = dFdxFine(WorldPosition.xyz);
    vec3 dY = dFdyFine(WorldPosition.xyz);
    vec3 N = normalize(cross(dY, dX));
#endif

    // material descriptor
    SMaterial Material;
#if 1
    vec3 W;
    hex2colTex(AlbedoMap, ARMMap, (ubo.CameraPosition.xz + WorldPosition.xz) * 3e-4, Material, W);
    // Material.Albedo.rgb = W;
#else
    // Material.Albedo = texture(NoiseMap, UV);
    switch (Level)
    {
        case 0:
        Material.Albedo = vec4(1.0);
        break;
        case 1:
        Material.Albedo = vec4(1.0, 0.0, 0.0, 1.0);
        break;
        case 2:
        Material.Albedo = vec4(0.0, 1.0, 0.0, 1.0);
        break;
        case 3:
        Material.Albedo = vec4(0.0, 0.0, 1.0, 1.0);
        break;
        case 4:
        Material.Albedo = vec4(0.0, 1.0, 1.0, 1.0);
        break;
        case 5:
        Material.Albedo = vec4(1.0, 0.0, 1.0, 1.0);
        break;
        case 6:
        Material.Albedo = vec4(1.0, 1.0, 0.0, 1.0);
        break;
    }

    float w = Height <= 2e-3 ? smoothstep(0.0, 1.0, 1.0 - Height / 2e-3) : 0.0;
    float r = SampleWater(vec3(UV, Level));
    w = saturate(w + mix(5.0, 1.0, r) * r);
    Material.Albedo = mix(vec4(0.0, 1.0, 0.0, 1.0), vec4(0.0, 0.0, 1.0, 1.0), w * 0.5);

    Material.AO = 1.0;
    Material.Roughness = 1.0;
    Material.Metallic = 0.0;
#endif
    Material.Albedo.rgb = PushConstants.ColorMask.rgb * Material.Albedo.rgb;
    Material.Roughness = max(PushConstants.RoughnessMultiplier * Material.Roughness, 0.01);

    outColor = vec4(Material.Albedo.rgb, 1.0);
    outNormal = vec4(N, 0.0);
    outDeferred = vec4(vec3(Material.AO, Material.Roughness, Material.Metallic), 0.0);
}