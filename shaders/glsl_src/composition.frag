#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "brdf.glsl"

layout(binding = 1) uniform sampler2D HDRColor;
layout(binding = 2) uniform sampler2D HDRNormals;
layout(binding = 3) uniform sampler2D HDRDeferred;
layout(binding = 4) uniform sampler2D SceneDepth;
layout(binding = 5) uniform sampler2D SceneBackground;
layout(binding = 6) uniform sampler2D BackgroundDepth;

layout(binding = 7) uniform sampler2D TransmittanceLUT;
layout(binding = 8) uniform sampler2D IrradianceLUT;
layout(binding = 9) uniform sampler3D InscatteringLUT;

layout(binding = 10) uniform CloudLayer
{
    float Coverage;
    float Absorption;
    float WindSpeed;
} Clouds;

layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 outColor;

// get specular and ambient part from sunlight
vec3 DirectSunlight(vec3 V, vec3 L, vec3 N, in SMaterial Material, in SAtmosphere Atmosphere)
{
    vec3 H = normalize(V + L);

    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float NdotH = saturate(dot(N, H));
    float LdotH = saturate(dot(L, H));
    float HdotV = saturate(dot(H, V));

    vec3 F0 = mix(vec3(0.04), Material.Albedo.rgb, Material.Metallic);
    vec3  F = Material.Specular * F_FresnelSchlick(HdotV, F0);
    float G = G_GeometrySmith(NdotV, NdotL, Material.Roughness);
    float D = D_DistributionGGX(NdotH, Material.Roughness);

    vec3 Sunl = Atmosphere.L * MaxLightIntensity;

    vec3 kD = (vec3(1.0) - F) * vec3(1.0 - Material.Metallic);
    vec3 specular = (F * D * G) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 diffuse = kD * DisneyDiffuse(Material.Albedo.rgb, NdotL, NdotV, LdotH, Material.Roughness);
    vec3 ambient = Material.AO * vec3(0.01) * Material.Albedo.rgb;

    return Sunl * (ambient + ((specular + diffuse)) * NdotL); // + Atmosphere.S
}

vec3 GetWorldPosition(float Depth)
{
    vec4 ClipSpace = vec4(2.0 * UV - 1.0, Depth, 1.0);
    vec4 WorldPosition = vec4(ubo.ViewMatrixInverse * (ubo.ProjectionMatrixInverse * ClipSpace));
    WorldPosition = WorldPosition / WorldPosition.w;

    return WorldPosition.xyz;
}

void main()
{
    vec4 SkyColor = texture(SceneBackground, UV);
    float SkyDepth = texture(BackgroundDepth, UV).r;
    vec4 Color = texelFetch(HDRColor, ivec2(gl_FragCoord.xy), 0);
    float Depth = texelFetch(SceneDepth, ivec2(gl_FragCoord.xy), 0).r;
    vec4 Deferred = texelFetch(HDRDeferred, ivec2(gl_FragCoord.xy), 0).rgba;
    vec3 Normal = normalize(2.0 * texelFetch(HDRNormals, ivec2(gl_FragCoord.xy), 0).rgb - 1.0);
 
    if (Color.a != 0.0 && Depth > SkyDepth)
    {
        vec3 Eye = ubo.CameraPosition.xyz;
        vec3 L = normalize(ubo.SunDirection.xyz);
        vec3 WorldPosition = GetWorldPosition(Depth);
        vec3 V = normalize(Eye - WorldPosition.xyz);

        float t = distance(Eye, WorldPosition);
        SAtmosphere Atmosphere;
        AtmosphereAtPoint(TransmittanceLUT, InscatteringLUT, WorldPosition.xyz, t, V, L, Atmosphere);

        SMaterial Material;
        Material.Roughness = Deferred.g;
        Material.Metallic = Deferred.b;
        Material.AO = Deferred.r;
        Material.Albedo.rgb = pow(Color.rgb, vec3(2.2));
        Material.Albedo.a = Color.a;
        Material.Specular = Deferred.a;
        Color.rgb = DirectSunlight(V, L, Normal, Material, Atmosphere);
    }
    else
    {
        Color.rgb = SkyColor.rgb;
    }

    outColor = vec4(Color.rgb, 1.0);
}