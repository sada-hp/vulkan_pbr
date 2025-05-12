#include "common.glsl"

struct SMaterial
{
    vec4 Albedo;
    vec3 Normal;
    float Roughness;
    float Metallic;
    float AO;
    float Specular;
};

SMaterial mixmat(in SMaterial A, in SMaterial B, float w)
{
    if (w >= 1.0)
        return B;
    else if (w <= 0.0)
        return A;
    else
    {
        SMaterial C;
        C.Albedo = mix(A.Albedo, B.Albedo, w);
        C.Normal = normalize(mix(A.Normal, B.Normal, w));
        C.Roughness = mix(A.Roughness, B.Roughness, w);
        C.Specular = mix(A.Specular, B.Specular, w);
        C.Metallic = mix(A.Metallic, B.Metallic, w);
        C.AO = mix(A.AO, B.AO, w);

        return C;
    }
}

SMaterial mixmat(in SMaterial A, in SMaterial B, float wA, float wB)
{
    SMaterial C;
    C.Albedo = wA * A.Albedo + wB * B.Albedo;
    C.Normal = normalize(wA * A.Normal + wB * B.Normal);
    C.Roughness = saturate(wA * A.Roughness + B.Roughness);
    C.Specular = saturate(wA * A.Specular + wB * B.Specular);
    C.Metallic = saturate(wA * A.Metallic + wB * B.Metallic);
    C.AO = saturate(wA * A.AO + wB * B.AO);

    return C;
}

vec3 F_FresnelSchlick(float Mu, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(max(1.0 - Mu, 0.0), 5.0);
}

vec3 F_FresnelSchlick(float Mu, vec3 f0, vec3 f90)
{
    return f0 + (f90 - f0) * pow(max(1.0 - Mu, 0.0), 5.0);
}

float D_DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float det = NdotH * NdotH * (a2 - 1.0) + 1.0;
	
    return a2 / max((det * det), 0.001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float G_GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float G_GeometrySmithCorrelated(float NdotV, float NdotL, float roughness)
{
    float a2 = roughness * roughness;

    float Lambda_GGXV = NdotL * sqrt((-NdotV * a2 + NdotV) * NdotV + a2);
    float Lambda_GGXL = NdotV * sqrt((-NdotL * a2 + NdotL) * NdotL + a2);

    return 0.5 / (Lambda_GGXV + Lambda_GGXL);
}

// course-notes-moving-frostbite-to-pbr-v2
vec3 DisneyDiffuse(vec3 Albedo, float NdotL, float NdotV, float LdotH, float Roughness)
{
    float EnergyBias = mix(0.0, 0.5, Roughness);
    float EnergyFactor = mix(1.0, 1.0 / 1.51, Roughness);
    vec3 F90 = vec3(EnergyBias + 2.0 * LdotH * LdotH * Roughness);
    vec3 F0 = vec3(1.0);

    float LightScatter = F_FresnelSchlick(NdotL, F0, F90).r;
    float ViewScatter = F_FresnelSchlick(NdotV, F0, F90).r;

    return Albedo * vec3(LightScatter * ViewScatter * EnergyFactor);
}

vec3 ImportanceSampleGGX(mat3 TBN, vec2 Xi, float a)
{
    float phi = 6.28318530718 * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
    // from spherical coordinates to cartesian coordinates
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    return normalize(TBN * H);
}  

float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}