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
    float Density;
    float WindSpeed;
} Clouds;

layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 outColor;

// get specular and ambient part from sunlight
vec3 DirectSunlight(in vec3 Eye, in vec3 World, in vec3 Sun, in SMaterial Material)
{
    vec3 N = Material.Normal;
    vec3 V = normalize(Eye - World);
    vec3 H = normalize(V + Sun);

    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, Sun));
    float NdotH = saturate(dot(N, H));
    float LdotH = saturate(dot(Sun, H));
    float HdotV = saturate(dot(H, V));

    SAtmosphere Atmosphere;
    AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, Eye, World, Sun, Atmosphere);

    vec3  F0 = mix(vec3(0.04), Material.Albedo.rgb, Material.Metallic);
    vec3  F  = Material.Specular * F_FresnelSchlick(HdotV, F0);
    float G  = G_GeometrySmith(NdotV, NdotL, Material.Roughness);
    float D  = D_DistributionGGX(NdotH, Material.Roughness);

    vec3 kD       = (vec3(1.0) - F) * vec3(1.0 - Material.Metallic);
    vec3 specular = (F * D * G) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 diffuse  = kD * DisneyDiffuse(Material.Albedo.rgb, NdotL, NdotV, LdotH, Material.Roughness);
    vec3 ambient  = vec3(mix(1e-3, 1e-4, Material.Metallic)) * Material.Albedo.rgb;
    vec3 scatter  = Atmosphere.S;

    diffuse  = diffuse  * Atmosphere.Shadow * (Atmosphere.L + Atmosphere.E);
    specular = specular * Atmosphere.Shadow * Atmosphere.L;
    ambient  = ambient  * Atmosphere.T;

    return vec3(ambient + MaxLightIntensity * (scatter + (Material.AO * (specular + diffuse) / PI) * NdotL));
}

void main()
{
    vec4 SkyColor  = texture(SceneBackground, UV);
    float SkyDepth = texture(BackgroundDepth, UV).r;
    vec4 Color     = texelFetch(HDRColor, ivec2(gl_FragCoord.xy), 0);
    float Depth    = texelFetch(SceneDepth, ivec2(gl_FragCoord.xy), 0).r;
    vec4 Deferred  = texelFetch(HDRDeferred, ivec2(gl_FragCoord.xy), 0).rgba;
    vec3 Normal    = normalize(texelFetch(HDRNormals, ivec2(gl_FragCoord.xy), 0).rgb);
 
    if (Color.a != 0.0)
    {
        vec3 Eye   = ubo.CameraPosition.xyz;
        vec3 Sun   = normalize(ubo.SunDirection.xyz);
        vec3 World = GetWorldPosition(UV, Depth);
        
        SMaterial Material;
        Material.Roughness  = Deferred.g;
        Material.Metallic   = Deferred.b;
        Material.AO         = Deferred.r;
        Material.Albedo.rgb = pow(Color.rgb, vec3(2.2));
        Material.Albedo.a   = Color.a;
        Material.Specular   = Deferred.a;
        Material.Normal     = Normal;
        
        Color.rgb = DirectSunlight(Eye, World, Sun, Material);

        if (Depth < SkyDepth)
        {
            vec3 CloudPosition = GetWorldPosition(UV, SkyDepth);
            float d = distance(CloudPosition, World);

            float a = 1.0 - SkyColor.a;
            float b = 1.0 - saturate(exp(-d * Clouds.Density * 0.01));
            Color.rgb = mix(SkyColor.rgb, Color.rgb, 1.0 - a * b);
        }
    }
    else
    {
        Color.rgb = SkyColor.rgb;
    }

    outColor = vec4(Color.rgb, 1.0);
}