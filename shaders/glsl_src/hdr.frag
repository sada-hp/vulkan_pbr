#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "brdf.glsl"

layout(input_attachment_index = 0, binding = 1) uniform subpassInput HDRColor;
layout(input_attachment_index = 1, binding = 2) uniform subpassInput HDRNormals;
layout(input_attachment_index = 2, binding = 3) uniform subpassInput HDRDeferred;
layout(input_attachment_index = 3, binding = 4) uniform subpassInput Depth;

layout(binding = 5) uniform sampler2D TransmittanceLUT;
layout(binding = 6) uniform sampler2D IrradianceLUT;
layout(binding = 7) uniform sampler3D InscatteringLUT;

layout(location = 0) in vec2 UV;

layout(location = 0) out vec4 outColor;

// get specular and ambient part from sunlight
vec3 DirectSunlight(vec3 V, vec3 L, vec3 N, in SMaterial Material)
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

    return ambient + ((specular + diffuse) / PI * NdotL) * NdotV;
}

// snatched from precomputed-atmospheric-scattering code
vec3 Tonemap(vec3 L) 
{
    const float exposure = 1.5;
    const float gamma = 2.2;

    L = L * exposure;
    L.r = L.r < 1.413 ? pow(L.r * 0.38317, 1.0 / gamma) : 1.0 - exp(-L.r);
    L.g = L.g < 1.413 ? pow(L.g * 0.38317, 1.0 / gamma) : 1.0 - exp(-L.g);
    L.b = L.b < 1.413 ? pow(L.b * 0.38317, 1.0 / gamma) : 1.0 - exp(-L.b);
    return L;
}

vec3 Tonemap2(vec3 L) 
{
    const float gamma = 2.2;
    L = L / (L + vec3(1.0));
    return pow(L, vec3(1.0 / gamma));
}

vec3 GetWorldPosition()
{
    vec4 ClipSpace = vec4(2.0 * UV - 1.0, subpassLoad(Depth).r, 1.0);
    vec4 WorldPosition = vec4(ubo.ViewMatrixInverse * (ubo.ProjectionMatrixInverse * ClipSpace));
    WorldPosition = WorldPosition / WorldPosition.w;

    return WorldPosition.xyz;
}
void main()
{
    vec4 Color = subpassLoad(HDRColor);
    vec3 Normal = normalize(2.0 * subpassLoad(HDRNormals).rgb - 1.0);
    vec3 Deferred = subpassLoad(HDRDeferred).rgb;
 
    if (Deferred != vec3(0.0))
    {
        vec3 WorldPosition = GetWorldPosition();
        vec3 L = normalize(ubo.SunDirection.xyz);
        //vec3 Eye = ubo.CameraPosition.xyz + vec3(0.0, Rg, 0.0);
        vec3 V = normalize(ubo.CameraPosition.xyz - WorldPosition.xyz);

        SMaterial Material;
        Material.Roughness = Deferred.r;
        Material.Metallic = Deferred.g;
        Material.AO = Deferred.b;
        Material.Albedo = Color;

        Color.rgb = DirectSunlight(V, L, Normal, Material);

        vec3 Point = WorldPosition.xyz;
        vec3 Eye = ubo.CameraPosition.xyz;

        SAtmosphere Atmosphere;
        PointRadiance(TransmittanceLUT, InscatteringLUT, L, Eye, Point, Atmosphere);
        Color.rgb = Color.rgb * Atmosphere.L + Atmosphere.S;
        //Color.rgb = Color.rgb * Atmosphere.L;
    }

    outColor = vec4(Tonemap(Color.rgb), 1.0);
}