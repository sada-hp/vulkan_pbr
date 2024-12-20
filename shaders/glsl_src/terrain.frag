#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "brdf.glsl"

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

layout(location = 0) in vec4 WorldPosition;
layout(location = 1) in vec3 Normal;
layout(location = 2) in float Height;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outDeferred;

void main()
{
    vec3 dX = dFdxFine(WorldPosition.xyz);
    vec3 dY = dFdyFine(WorldPosition.xyz);

    // reading the normal map
    // vec3 N = normalize(cross(dY.xyz, dX.xyz));
    vec3 N = normalize(Normal);
    // vec3 N = vec3(0.0, 1.0, 0.0);

    //vec4 ARM = texture(ARMMap, UV);
    vec4 ARM = vec4(1.0, 1.0, 0.0, 0.0);

    // material descriptor
    SMaterial Material;
    Material.Roughness = max(PushConstants.RoughnessMultiplier * ARM.g, 0.01);
    Material.Metallic = (PushConstants.Metallic == 0.0 ? ARM.b : PushConstants.Metallic);
    Material.AO = ARM.r;
    //Material.Albedo = texture(AlbedoMap, UV);
    //Material.Albedo.rgb = pow(PushConstants.ColorMask.rgb * Material.Albedo.rgb, vec3(2.2));

#if 1

    float Slope = abs(dot(N, normalize(WorldPosition.xyz)));

    vec4 c1 = vec4(0.0, 1.0, 0.0, 1.0);
    vec4 c2 = vec4(1.0, 0.0, 0.0, 1.0);
    vec4 c3 = vec4(0.0, 0.0, 1.0, 1.0);

    if (Slope > 0.95)
    {
        Material.Albedo = c1;
    }
    //else if (Slope > 0.95)
    //{
    //    Material.Albedo = c3;
    //}
    else
    {
        Material.Albedo = mix(c1, c2, saturate(10.0 * (1.0 - saturate(Slope / 0.95))));
    }

#else
    Material.Albedo = vec4(0.25, 1.0, 0.5, 1.0);
#endif

    //Material.Albedo = PushConstants.ColorMask.rgba;
    Material.Albedo.rgb = pow(Material.Albedo.rgb, vec3(2.2));

    outColor = Material.Albedo;
    outNormal = vec4(N * 0.5 + 0.5, 0.0);
    outDeferred = vec4(vec3(Material.Roughness, Material.Metallic, Material.AO), 1.0);
}