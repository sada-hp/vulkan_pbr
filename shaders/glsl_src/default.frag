#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "brdf.glsl"

struct SMaterial
{
    vec4 Albedo;
    float Roughness;
    float Metallic;
    float AO;
};

layout(push_constant) uniform constants
{
    layout(offset = 64) vec4 ColorMask;
    layout(offset = 80) float RoughnessMultiplier;
    layout(offset = 84) float Metallic;
    layout(offset = 88) float HeightScale;
} 
PushConstants;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 WorldPosition;
layout(location = 2) in mat3 TBN;

layout(set = 1, binding = 1) uniform sampler2D TransmittanceLUT;
layout(set = 1, binding = 2) uniform sampler2D IrradianceLUT;
layout(set = 1, binding = 3) uniform sampler3D InscatteringLUT;
layout(set = 1, binding = 4) uniform sampler2D AlbedoMap;
layout(set = 1, binding = 5) uniform sampler2D NormalHeightMap;
layout(set = 1, binding = 6) uniform sampler2D ARMMap;

layout(location = 0) out vec4 outColor;

// very simplified version which skips aerial perspective and scattering
vec3 PointRadiance(vec3 Sun, vec3 Eye, vec3 Point)
{
    vec3 View = normalize(Point - Eye);
    const float Rp = length(Point);
    const float DotPL = dot(Point, Sun) / Rp;

    return GetTransmittanceWithShadow(TransmittanceLUT, Rp, DotPL) * MaxLightIntensity;
}

// course-notes-moving-frostbite-to-pbr-v2
vec3 GetDiffuseTerm(vec3 Albedo, float NdotL, float NdotV, float LdotH, float Roughness)
{
    float EnergyBias = mix(0.0, 0.5, Roughness);
    float EnergyFactor = mix(1.0, 1.0 / 1.51, Roughness);
    vec3 F90 = vec3(EnergyBias + 2.0 * LdotH * LdotH * Roughness);
    vec3 F0 = vec3(1.0);

    float LightScatter = FresnelSchlick(NdotL, F0, F90).r;
    float ViewScatter = FresnelSchlick(NdotV, F0, F90).r;

    return Albedo * vec3(LightScatter * ViewScatter * EnergyFactor);
}

// get specular and ambient part from sunlight
vec3 DirectSunlight(vec3 Eye, vec3 Point, vec3 V, vec3 L, vec3 N, in SMaterial Material)
{
    vec3 H = normalize(V + L);

    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float NdotH = saturate(dot(N, H));
    float LdotH = saturate(dot(L, H));
    float HdotV = saturate(dot(H, V));

    vec3 F0 = mix(vec3(0.04), Material.Albedo.rgb, Material.Metallic);
    vec3 F = FresnelSchlick(HdotV, F0);
    float G = GeometrySmith(NdotV, NdotL, Material.Roughness);
    float D = DistributionGGX(NdotH, Material.Roughness);

    vec3 kD = vec3(1.0 - Material.Metallic);
    vec3 specular = vec3(max((D * G * F) / (4.0 * NdotV * NdotL), 0.001));
    vec3 diffuse = kD * GetDiffuseTerm(Material.Albedo.rgb, NdotL, NdotV, LdotH, Material.Roughness);
    //vec3 diffuse = kD * Material.Albedo.rgb;
    vec3 ambient = Material.AO * vec3(0.01) * Material.Albedo.rgb;
    vec3 radiance = PointRadiance(L, Eye, Point);

    return ambient * radiance + ((specular + diffuse) / PI * NdotL) * radiance * NdotV;
}

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
    // these are used to sample atmosphere, so we offset them to ground level
    vec3 Point = WorldPosition.xyz + vec3(0.0, Rg, 0.0);
    vec3 Eye = ubo.CameraPosition.xyz + vec3(0.0, Rg, 0.0);
    
    // getting default lighting vectors
    vec3 V = normalize(ubo.CameraPosition.xyz - WorldPosition.xyz);
    vec3 L = normalize(ubo.SunDirection.xyz);

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

    // getting the color
    vec3 Lo = DirectSunlight(Eye, Point, V, L, N, Material);
    outColor = vec4(Lo, PushConstants.ColorMask.a * Material.Albedo.a);
}