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
layout(set = 1, binding = 4) uniform sampler2D NoiseMap;

layout(location = 0) in vec4 WorldPosition;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 UV;
layout(location = 3) in float Height;
layout(location = 4) in flat int Level;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outDeferred;

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
    hex2colTex(AlbedoMap, ARMMap, (ubo.CameraPosition.xz + WorldPosition.xz) * 1e-4, Material, W);
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
    Material.Albedo = Height == 0 ? vec4(0.0, 0.0, 1.0, 1.0) : vec4(0.0, 1.0, 0.0, 1.0);

    Material.AO = 1.0;
    Material.Roughness = 1.0;
    Material.Metallic = 0.0;
#endif
    Material.Albedo.rgb = PushConstants.ColorMask.rgb * Material.Albedo.rgb;

    outColor = vec4(Material.Albedo.rgb, 1.0);
    outNormal = vec4(N, 0.0);
    outDeferred = vec4(vec3(Material.AO, Material.Roughness, Material.Metallic), 1.0);
}