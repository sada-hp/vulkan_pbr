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

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 WorldPosition;
layout(location = 2) in mat3 TBN;

layout(set = 1, binding = 1) uniform sampler2D AlbedoMap;
layout(set = 1, binding = 2) uniform sampler2D NormalHeightMap;
layout(set = 1, binding = 3) uniform sampler2D ARMMap;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outDeferred;

vec2 Displace(vec2 inUV, vec3 V)
{
    const int steps = 64;
    const float stepsize = 1.0 / float(steps);

    vec2 UV = inUV;
    vec2 dUV = (V.xy * 0.01 * PushConstants.HeightScale) / (V.z * float(steps));

    float height = 1.0 - texture(NormalHeightMap, UV).a;
    float depth = 0.0;

    while (depth < height)
    {
        UV -= dUV;
        height = 1.0 - texture(NormalHeightMap, UV).a;
        depth += stepsize;
    }

    vec2 prevUV = UV + dUV;
	float nextDepth = height - depth;
	float prevDepth = 1.0 - texture(NormalHeightMap, prevUV).a - depth + stepsize;
    float weight = nextDepth / (nextDepth - prevDepth);
    
	return mix(UV, prevUV, weight);
}

void main()
{
    vec3 V = -normalize(WorldPosition.xyz);

    // parallax
    vec2 UV = Displace(inUV, normalize(transpose(TBN) * V));

    // reading the normal map
    vec3 NormalMap = normalize(texture(NormalHeightMap, UV).rgb * 2.0 - 1.0);
    vec3 N = normalize(TBN * NormalMap);

    vec4 ARM = texture(ARMMap, UV);

    // material descriptor
    SMaterial Material;
    Material.Roughness = max(PushConstants.RoughnessMultiplier * ARM.g, 0.01);
    Material.Metallic = (PushConstants.Metallic == 0.0 ? ARM.b : PushConstants.Metallic);
    Material.AO = ARM.r;
    Material.Albedo = texture(AlbedoMap, UV);
    Material.Albedo.rgb = pow(PushConstants.ColorMask.rgb * Material.Albedo.rgb, vec3(2.2));

    if (UV.x < 0.0 || UV.x > 1.0 || UV.y < 0.0 || UV.y > 1.0)
        discard;

    outColor = vec4(Material.Albedo.rgb, PushConstants.ColorMask.a * Material.Albedo.a);
    outNormal = vec4(N * 0.5 + 0.5, 0.0);
    outDeferred = vec4(vec3(Material.Roughness, Material.Metallic, Material.AO), 1.0);
}