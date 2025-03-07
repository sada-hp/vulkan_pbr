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
vec3 DirectSunlight(vec3 Eye, vec3 X, vec3 L, in SMaterial Material)
{
    vec3 N = Material.Normal;
    vec3 V = normalize(Eye - X);
    vec3 H = normalize(V + L);

    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float NdotH = saturate(dot(N, H));
    float LdotH = saturate(dot(L, H));
    float HdotV = saturate(dot(H, V));

    vec3 F0 = mix(vec3(0.04), Material.Albedo.rgb, Material.Metallic);
    vec3 F = Material.Specular * F_FresnelSchlick(HdotV, F0);
    float G = G_GeometrySmith(NdotV, NdotL, Material.Roughness);
    float D = D_DistributionGGX(NdotH, Material.Roughness);

    vec3 kD = (vec3(1.0) - F) * vec3(1.0 - Material.Metallic);
    vec3 specular = (F * D * G) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 diffuse = kD * DisneyDiffuse(Material.Albedo.rgb, NdotL, NdotV, LdotH, Material.Roughness);
    vec3 ambient = vec3(mix(0.001, 0.01, Material.Metallic)) * Material.Albedo.rgb;

    if (Eye != X)
    {
        float r0 = length(X);
        float PdotL = dot(X, L) / r0;

        float Shadow = smoothstep(0.0, 1.0, max(PdotL, 0.0));
        vec3 Li = GetTransmittanceWithShadow(TransmittanceLUT, r0, PdotL);
        vec3 E =  GetIrradiance(IrradianceLUT, r0, PdotL);
        // vec3 S = PointScattering(TransmittanceLUT, InscatteringLUT, Eye, X, L);

        diffuse =  MaxLightIntensity * (Li + E) * diffuse;
        specular = MaxLightIntensity * (Li + E) * specular;
    }

    return ambient + (Material.AO * (specular + diffuse)) * NdotL / PI;
}

vec3 GetWorldPosition(float Depth)
{
    vec4 ClipSpace = vec4(2.0 * UV - 1.0, Depth, gl_FragCoord.w);
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
    vec3 Normal = normalize(texelFetch(HDRNormals, ivec2(gl_FragCoord.xy), 0).rgb);
 
    if (Color.a != 0.0)
    {
        vec3 Eye = ubo.CameraPosition.xyz;
        vec3 L = normalize(ubo.SunDirection.xyz);
        vec3 WorldPosition = GetWorldPosition(Depth);
        
#if 1
        SMaterial Material;
        Material.Roughness = Deferred.g;
        Material.Metallic = Deferred.b;
        Material.AO = Deferred.r;
        Material.Albedo.rgb = pow(Color.rgb, vec3(2.2));
        Material.Albedo.a = Color.a;
        Material.Specular = Deferred.a;
        Material.Normal = Normal;
        Color.rgb = DirectSunlight(Eye, WorldPosition, L, Material);

        if (Depth < SkyDepth)
        {
            Color.rgb = mix(SkyColor.rgb, Color.rgb, SkyColor.a);
        }
    #endif
    }
    else
    {
        Color.rgb = SkyColor.rgb;
    }

    outColor = vec4(Color.rgb, 1.0);
}