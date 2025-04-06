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

layout(set = 1, binding = 1) uniform sampler2DArray AlbedoMap;
layout(set = 1, binding = 2) uniform sampler2DArray NormalHeightMap;
layout(set = 1, binding = 3) uniform sampler2DArray ARMMap;
layout(set = 2, binding = 0) uniform sampler2DArray NoiseMap;
layout(set = 2, binding = 1) uniform sampler2DArray WaterMap;

layout(location = 0) in vec4 WorldPosition;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 UV;
layout(location = 3) in float Height;
layout(location = 4) in flat int Level;
layout(location = 5) in mat3 TBN;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outDeferred;

float SampleWater(vec3 UVW, float Factor)
{
    if (UVW.z >= 4)
        return 0.0;

    float r = max(textureLod(WaterMap, UVW, 0).r, textureLod(WaterMap, vec3(UVW.xy * 0.5 + 0.25, UVW.z + 1), 0).r);

    r = saturate(r / Factor);
    r = saturate(r - 0.1) / 0.9;

    return r;
}

void main()
{
    SMaterial Material;
    SMaterial GrassMaterial, RockMaterial, SandMaterial, SnowMaterial;
    hex2colTex(AlbedoMap, NormalHeightMap, ARMMap, 0, (ubo.CameraPosition.xz + WorldPosition.xz) * 5e-4, GrassMaterial);
    hex2colTex(AlbedoMap, NormalHeightMap, ARMMap, 1, (ubo.CameraPosition.xz + WorldPosition.xz) * 5e-4, RockMaterial);
    hex2colTex(AlbedoMap, NormalHeightMap, ARMMap, 2, (ubo.CameraPosition.xz + WorldPosition.xz) * 5e-4, SandMaterial);
    hex2colTex(AlbedoMap, NormalHeightMap, ARMMap, 3, (ubo.CameraPosition.xz + WorldPosition.xz) * 5e-4, SnowMaterial);

    vec3 Water = vec3(0.03, 0.03, 0.05);

#if 1
    vec3 N = normalize(Normal);
    vec3 U = normalize(WorldPosition.xyz + ubo.CameraPosition.xyz);
#else
    vec3 dX = dFdxFine(WorldPosition.xyz);
    vec3 dY = dFdyFine(WorldPosition.xyz);
    vec3 N = normalize(cross(dY, dX));
#endif

    // material descriptor
#if 1
    float w = saturate(10.0 * (1.0 - saturate(abs(dot(N, U)))));

    // Grass-to-rock
    Material = mixmat(GrassMaterial, RockMaterial, w);

    // -to-sand
    w = Height <= 0.02 ? smoothstep(0.0, 1.0, 1.0 - Height / 0.02) : 0.0;
    w = saturate(w + SampleWater(vec3(UV, Level), 500.0));
    Material = mixmat(Material, SandMaterial, w);

    // -to-snow
    w = saturate(2.0 * saturate(Height - 0.4) / 0.6);
    Material = mixmat(Material, SnowMaterial, w);

    // -water
    w = Height <= 2e-3 ? smoothstep(0.0, 1.0, 1.0 - Height / 2e-3) : 0.0;
    w = saturate(w + SampleWater(vec3(UV, Level), 500.0));
    Material.Albedo.rgb = mix(Material.Albedo.rgb, Water, w);
    Material.Roughness = mix(Material.Roughness, 0.0, w * w * w);
    Material.AO = mix(Material.AO, 1.0, w);
    Material.Normal = normalize(mix(normalize(TBN * Material.Normal), N, w));
#else
    Material = GrassMaterial;
    Material.Normal = normalize(TBN * Material.Normal);
#endif

    Material.Roughness = max(PushConstants.RoughnessMultiplier * Material.Roughness, 0.01);

    outColor = vec4(Material.Albedo.rgb, 1.0);
    outNormal = vec4(Material.Normal, 0.0);
    outDeferred = vec4(vec3(Material.AO, Material.Roughness, Material.Metallic), 0.0);
}