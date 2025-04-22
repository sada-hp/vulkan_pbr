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

layout(set = 1, binding = 0) uniform sampler2DArray AlbedoMap;
layout(set = 1, binding = 1) uniform sampler2DArray NormalHeightMap;
layout(set = 1, binding = 2) uniform sampler2DArray ARMMap;

layout(set = 2, binding = 0) uniform sampler2DArray NoiseMap;
layout(set = 2, binding = 1) uniform sampler2DArray WaterMap;

layout(location = 0) in vec4 WorldPosition;
layout(location = 1) in vec2 VertPosition;
layout(location = 2) in flat float Diameter;
layout(location = 3) in flat float Diameter2;
layout(location = 4) in float Height;
layout(location = 5) in flat int Level;
layout(location = 6) in mat3 TBN;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outDeferred;

float SampleWater(vec3 UVW)
{
    float r = textureLod(WaterMap, UVW, 0).r;
    for (int i = int(UVW.z + 1); i < textureSize(WaterMap, 0).z; i++)
    {
        UVW = vec3(UVW.xy * 0.5 + 0.25, UVW.z + 1.0);
        r = max(r, textureLod(WaterMap, UVW, 0).r);
    }

    return r;
}

float DampenValue(float Value, float Factor)
{
    Value = saturate(Value / Factor);
    Value = saturate(Value - 0.1) / 0.9;

    return Value;
}

void main()
{
    HexParams Tiling;

    vec2 WorldUV = vec2(0.0);
    if (abs(WorldPosition.y) > abs(WorldPosition.x) && abs(WorldPosition.y) > abs(WorldPosition.z))
    {
        WorldUV = WorldPosition.xz;
    }
    else if (abs(WorldPosition.z) > abs(WorldPosition.x) && abs(WorldPosition.z) > abs(WorldPosition.y))
    {
        WorldUV = WorldPosition.xy;
    }
    else
    {
        WorldUV = WorldPosition.yz;
    }

    GetHexParams(1.0, WorldUV * 5e-4, Tiling);

    vec3 W;
    SMaterial Material;
    SMaterial GrassMaterial, RockMaterial, SandMaterial, SnowMaterial;
    hex2colTex(AlbedoMap, NormalHeightMap, ARMMap, 0, Tiling, GrassMaterial, W);
    hex2colTex(AlbedoMap, NormalHeightMap, ARMMap, 1, Tiling, RockMaterial, W);
    hex2colTex(AlbedoMap, NormalHeightMap, ARMMap, 2, Tiling, SandMaterial, W);
    hex2colTex(AlbedoMap, NormalHeightMap, ARMMap, 3, Tiling, SnowMaterial, W);

    vec3 WaterUV = vec3(0.5 + VertPosition / Diameter, Level);
    float WaterAmount = SampleWater(WaterUV);
    vec3 WaterColor = vec3(0.03, 0.03, 0.05);

    vec3 U = normalize(WorldPosition.xyz);
    // material descriptor
#if 1
    float w = saturate(10.0 * (1.0 - saturate(abs(dot(TBN[2], U)))));

    // Grass-to-rock
    Material = mixmat(GrassMaterial, RockMaterial, w);

    // -to-sand
    w = Height <= 0.02 ? smootherstep(0.0, 1.0, 1.0 - Height / 0.02) : 0.0;
    w = saturate(w + DampenValue(WaterAmount, 85.0));
    Material = mixmat(Material, SandMaterial, w);

    // -to-snow
    w = saturate(2.0 * saturate(Height - 0.4) / 0.6);
    Material = mixmat(Material, SnowMaterial, w);

    // -water
    w = Height <= 2e-3 ? smoothstep(0.0, 1.0, 1.0 - Height / 2e-3) : 0.0;
    w = saturate(w + DampenValue(WaterAmount, 175.0));
    Material.Albedo.rgb = mix(Material.Albedo.rgb, WaterColor, w);
    Material.Roughness = mix(Material.Roughness, 0.0, w * w * w);
    Material.AO = mix(Material.AO, 1.0, w);
    Material.Normal = normalize(mix(normalize(TBN * Material.Normal), TBN[2], w));
    Material.Specular = mix(0.1, 1.0, w);
#else
    if (Level == 0)
        Material.Albedo = vec4(1.0, 0.0, 0.0, 1.0);
    else if (Level == 1)
        Material.Albedo = vec4(0.0, 1.0, 0.0, 1.0);
    else if (Level == 2)
        Material.Albedo = vec4(0.0, 0.0, 1.0, 1.0);
    else if (Level == 3)
        Material.Albedo = vec4(1.0, 0.0, 1.0, 1.0);
    else if (Level == 4)
        Material.Albedo = vec4(0.0, 1.0, 1.0, 1.0);
    else if (Level == 5)
        Material.Albedo = vec4(1.0, 1.0, 0.0, 1.0);
    else if (Level == 6)
        Material.Albedo = vec4(1.0, 1.0, 1.0, 1.0);
    else if (Level == 7)
        Material.Albedo = vec4(0.863, 0.214, 0.6, 1.0);
    else if (Level == 8)
        Material.Albedo = vec4(0.654, 0.98, 0.213, 1.0);
    else if (Level == 9)
        Material.Albedo = vec4(0.323, 0.75, 0.75, 1.0);
    else if (Level == 10)
        Material.Albedo = vec4(0.12, 0.876, 0.1, 1.0);
    else
        Material.Albedo = vec4(0.5215, 0.876, 0.56, 1.0);

    Material.Normal = vec3(0.0, 1.0, 0.0);
    Material.AO = 1.0;
    Material.Roughness = 1.0;
    Material.Metallic = 0.0;
    Material.Specular = 0.1;
#endif

    // Material.Albedo.rgb = W;

    Material.Roughness = max(PushConstants.RoughnessMultiplier * Material.Roughness, 0.01);

    outColor = vec4(Material.Albedo.rgb, 1.0);
    outNormal = vec4(Material.Normal, 0.0);
    outDeferred = vec4(vec3(Material.AO, Material.Roughness, Material.Metallic), Material.Specular);
}