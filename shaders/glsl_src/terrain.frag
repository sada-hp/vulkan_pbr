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
    float r = texture(WaterMap, UVW).r;
    r = saturate(r / 400.0);
    r = saturate(r - 0.1) / 0.9;
    return r;
}

void main()
{
#if 1
    vec3 N = normalize(Normal);
    vec3 U = normalize(WorldPosition.xyz + ubo.CameraPosition.xyz);
#else
    vec3 dX = dFdxFine(WorldPosition.xyz);
    vec3 dY = dFdyFine(WorldPosition.xyz);
    vec3 N = normalize(cross(dY, dX));
#endif

    // material descriptor
    SMaterial Material;

    float w = Height <= 2e-3 ? smoothstep(0.0, 1.0, 1.0 - Height / 2e-3) : 0.0;
    float r = SampleWater(vec3(UV, Level));
    w = saturate(w + r);

    vec3 W;
    hex2colTex(AlbedoMap, ARMMap, (ubo.CameraPosition.xz + WorldPosition.xz) * 3e-4, Material, W);
    // Material.Albedo.rgb = W;

    Material.Albedo.rgb = mix(PushConstants.ColorMask.rgb * Material.Albedo.rgb, vec3(0.03, 0.03, 0.05), w);
    Material.Roughness = mix(Material.Roughness, 0.0, w * w * w);
    Material.AO = mix(Material.AO, 1.0, w);
    Material.Normal = N;

    w = saturate(10.0 * (1.0 - saturate(abs(dot(N, U)))));
    Material.Albedo.rgb = mix(Material.Albedo.rgb, vec3(0.05), w);
    Material.Roughness = mix(Material.Roughness, 1.0, w);
    Material.AO = mix(Material.AO, 1.0, w);

    w = saturate(2.0 * saturate(Height - 0.4) / 0.6);
    Material.Albedo.rgb = mix(Material.Albedo.rgb, vec3(1.0), w);
    Material.Roughness = mix(Material.Roughness, 0.15, w);

    Material.Roughness = max(PushConstants.RoughnessMultiplier * Material.Roughness, 0.01);

    outColor = vec4(Material.Albedo.rgb, 1.0);
    outNormal = vec4(Material.Normal, 0.0);
    outDeferred = vec4(vec3(Material.AO, Material.Roughness, Material.Metallic), 0.0);
}