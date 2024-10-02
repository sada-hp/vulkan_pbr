#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "brdf.glsl"

layout(push_constant) uniform constants
{
    layout(offset = 128) vec4 ColorMask;
    layout(offset = 144) float RoughnessMultiplier;
    layout(offset = 148) float Metallic;
    layout(offset = 152) float HeightScale;
} 
PushConstants;

layout(set = 1, binding = 1) uniform sampler2D AlbedoMap;
layout(set = 1, binding = 2) uniform sampler2D NormalHeightMap;
layout(set = 1, binding = 3) uniform sampler2D ARMMap;

layout(location = 0) in vec4 WorldPosition;
layout(location = 1) in vec3 Normal;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outDeferred;

void main()
{
    // reading the normal map
    vec3 N = normalize(Normal);

    //vec4 ARM = texture(ARMMap, UV);
    vec4 ARM = vec4(1.0, 1.0, 0.0, 0.0);

    // material descriptor
    SMaterial Material;
    Material.Roughness = max(PushConstants.RoughnessMultiplier * ARM.g, 0.01);
    Material.Metallic = (PushConstants.Metallic == 0.0 ? ARM.b : PushConstants.Metallic);
    Material.AO = ARM.r;
    //Material.Albedo = texture(AlbedoMap, UV);
    //Material.Albedo.rgb = pow(PushConstants.ColorMask.rgb * Material.Albedo.rgb, vec3(2.2));
    Material.Albedo = PushConstants.ColorMask.rgba;
    Material.Albedo.rgb = pow(Material.Albedo.rgb, vec3(2.2));

    outColor = vec4(Material.Albedo.rgb, PushConstants.ColorMask.a * Material.Albedo.a);
    outNormal = vec4(N * 0.5 + 0.5, 0.0);
    outDeferred = vec4(vec3(Material.Roughness, Material.Metallic, Material.AO), 0.0);
}