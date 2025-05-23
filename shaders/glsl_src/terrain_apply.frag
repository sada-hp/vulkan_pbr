#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "brdf.glsl"
#include "hextiling.glsl"

layout(push_constant) uniform constants
{
    layout(offset = 0) vec4 ColorMask;
    layout(offset = 16) float RoughnessMultiplier;
    layout(offset = 20) float Metallic;
    layout(offset = 24) float HeightScale;
}
PushConstants;

layout(set = 1, binding = 0) uniform sampler2DArray AlbedoMap;
layout(set = 1, binding = 1) uniform sampler2DArray NormalHeightMap;
layout(set = 1, binding = 2) uniform sampler2DArray ARMMap;

layout(set = 2, input_attachment_index = 0, binding = 0) uniform subpassInput HDRColor;
layout(set = 2, input_attachment_index = 1, binding = 1) uniform subpassInput HDRNormal;
layout(set = 2, input_attachment_index = 1, binding = 2) uniform subpassInput HDRDeferred;
layout(set = 2, input_attachment_index = 1, binding = 3) uniform subpassInput HDRDepth;

layout(location = 0) in vec2 UV;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outDeferred;

vec4 GetMaterialID(float Height, vec3 SlopeNormal, vec3 Up)
{
    float weights[4] = {
        1.0 - saturate(Height <= 2e-3 ? smoothstep(0.0, 1.0, 1.0 - Height / 2e-3) : 0.0),   // GrassMaterial
        saturate(10.0 * (1.0 - saturate(abs(dot(SlopeNormal, Up))))),                       // RockMaterial
        saturate((Height <= 0.02 ? smootherstep(0.0, 1.0, 1.0 - Height / 0.02) : 0.0)),     // SandMaterial
        saturate(8.0 * saturate(Height - 0.4) / 0.6)                                        // SnowMaterial
    };

    float sum = 0.0;
    for (int i = 0; i < 4; i++)
    {
        for (int j = i + 1; j < 4; j++)
        {
            weights[i] *= 1.0 - weights[j];
        }

        sum += weights[i];
    }

    int max_index = 0;
    float max_weight = weights[0];
    for (int i = 0; i < 4; i++)
    {
        weights[i] /= sum;

        if (max_weight < weights[i])
        {
            max_weight = weights[i];
            max_index = i;
        }
    }

    int second_index = 0;
    float second_weight = weights[0];
    for (int i = 0; i < 4; i++)
    {
        if (second_weight < weights[i] && i != max_index)
        {
            second_weight = weights[i];
            second_index = i;
        }
    }

    sum = max_weight + second_weight;
    max_weight = min(max_weight / sum, 0.999999);
    second_weight = min(second_weight / sum, 0.999999);

    max_weight = second_weight == 0.0 ? 1.0 : max_weight;
    second_weight = max_weight == 0.0 ? 1.0 : second_weight;

    return vec4(max_index, max_weight, second_index, second_weight);
}

void main()
{
    vec4 Descriptor1 = subpassLoad(HDRColor).rgba;
    vec4 Descriptor2 = subpassLoad(HDRDeferred).rgba;
    vec4 Descriptor3 = subpassLoad(HDRNormal).rgba;
    float Depth = subpassLoad(HDRDepth).r;

    if (round(Descriptor1.w) == 100.0)
    {
        vec3 WorldPosition = GetWorldPosition(UV, Depth);
        vec2 WorldUV = Descriptor1.xy;
        float Height = Descriptor1.z;

        vec2 dUV = vec2(Descriptor3.w / Descriptor3.z, 0.0);
        float f = 1.0 / (dUV.x * dUV.x);

        mat3 PlanetOrientation = mat3(ubo.PlanetMatrix);
        vec3 Normal = PlanetOrientation * normalize(vec3(Descriptor3.x, Descriptor3.w, Descriptor3.y));
        vec3 Tangent = PlanetOrientation * normalize(vec3(f * dUV.x * Descriptor3.w, f * dUV.x * (-Descriptor3.x), 0.0));
        vec3 Bitangent = PlanetOrientation * normalize(cross(Normal, Tangent));
        mat3 TBN = mat3(Tangent, Bitangent, Normal);

        HexParams Tiling;
        GetHexParams(1.0, WorldUV * 5e-4, Tiling);
        vec4 MaterialDescriptor = GetMaterialID(Height, Normal, normalize(WorldPosition));
        ivec2 MaterialID = ivec2(MaterialDescriptor.xz);
        vec2 MaterialWeight = MaterialDescriptor.yw;

        float w_water = saturate(Height <= 2e-3 ? smoothstep(0.0, 1.0, 1.0 - Height / 2e-3) : 0.0);
        const vec3 WaterColor = vec3(0.03, 0.03, 0.05);

        SMaterial Material, Material1, Material2;
        if (MaterialWeight.x != 0.0)
            hex2colTex(AlbedoMap, NormalHeightMap, ARMMap, MaterialID.x, Tiling, Material1);

        if (MaterialID.x != MaterialID.y)
        {
            if (MaterialWeight.y != 0.0)
                hex2colTex(AlbedoMap, NormalHeightMap, ARMMap, MaterialID.y, Tiling, Material2);
            
            Material = mixmat(Material1, Material2, MaterialWeight.x, MaterialWeight.y);
        }
        else
        {
            Material = Material1;
        }

        Material.Albedo.rgb = mix(Material.Albedo.rgb, WaterColor, w_water);
        Material.Roughness = max(mix(Material.Roughness, 0.0, w_water * w_water * w_water), 0.01);
        Material.AO = mix(Material.AO, 1.0, w_water);
        Material.Normal = normalize(mix(normalize(TBN * Material.Normal), TBN[2], w_water));
        Material.Specular *= mix(0.1, 1.0, w_water);

        outColor = vec4(PushConstants.ColorMask.rgb * Material.Albedo.rgb, 1.0);
        outNormal = vec4(Material.Normal, 1.0);
        outDeferred = vec4(vec3(Material.AO, PushConstants.RoughnessMultiplier * Material.Roughness, Material.Metallic), Material.Specular);
    }
    else if (round(Descriptor1.w) == 200.0)
    {
        vec3 WorldPosition = GetWorldPosition(UV, Depth);
        vec2 WorldUV = Descriptor1.xy;
        vec4 MaterialDescriptor = GetMaterialID(Descriptor3.w, normalize(Descriptor3.xyz), normalize(WorldPosition));
        ivec2 MaterialID = ivec2(MaterialDescriptor.xz);
        vec2 MaterialWeight = MaterialDescriptor.yw;

        vec3 avgGroundAlbedo = MaterialWeight.x * texture(AlbedoMap, vec3(WorldUV * 5e-4, MaterialID.x)).rgb
                + MaterialWeight.y * texture(AlbedoMap, vec3(WorldUV * 5e-4, MaterialID.y)).rgb;

        vec3 Albedo = 0.65 * mix(avgGroundAlbedo, vec3(0.35, 0.25, 0.0), 0.15);
    
        outColor = vec4(PushConstants.ColorMask.rgb * Albedo, 1.0);
        outNormal = vec4(Descriptor2.yzw, 1.0);
        outDeferred = vec4(Descriptor2.x, PushConstants.RoughnessMultiplier, 0.0, 0.1);
    }
    else
    {
        outColor = Descriptor1;
        outNormal = Descriptor3;
        outDeferred = Descriptor2;
    }
}