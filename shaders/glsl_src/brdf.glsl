struct SMaterial
{
    vec4 Albedo;
    float Roughness;
    float Metallic;
    float AO;
};

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
    float a2 = pow(roughness, 4.0);
    float det = NdotH * NdotH * (a2 - 1.0) + 1.0;
	
    return a2 / max((det * det) * PI, 0.001);
}

float G_GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float SchlickV = NdotV / (NdotV * (1.0 - k) + k);
    float SchlickL = NdotL / (NdotL * (1.0 - k) + k);

    return SchlickV * SchlickL;
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

    return Albedo * vec3(LightScatter * ViewScatter * EnergyFactor) / PI;
}