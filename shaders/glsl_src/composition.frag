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
layout(binding = 10) uniform sampler3D CloudLowFrequency;
layout(binding = 11) uniform sampler3D CloudHighFrequency;
layout(binding = 12) uniform sampler2D WeatherMap;
layout(binding = 13) uniform CloudLayer
{
    float Coverage;
    float Density;
    float WindSpeed;
} Clouds;

layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 outColor;

vec3 GetUV(vec3 p, float scale, float speed_mod)
{
    vec3 uv = p / Rct;
    vec3 anim = vec3(ubo.Time * Clouds.WindSpeed * speed_mod);
    return scale * uv + anim;
}

float GetHeightFraction(vec3 x0, float bottomBound, float topBound)
{
    return saturate((length(x0) - bottomBound) / (topBound - bottomBound));;
}

float SampleCloud(vec3 x0)
{
    vec3 uv = GetUV(x0, 50.0, 0.01);
    float height = 1.0 - GetHeightFraction(x0, Rcb + Rcdelta * 0.5 * (1.0 - max(Clouds.Coverage, 0.25)), Rct);

    float base = textureLod(CloudLowFrequency, uv, 3).r;

    float h1 = saturate(height + 0.2);
    float h2 = saturate(remap(1.0 - height, 0.0, 0.2, 0.0, 1.0));

    float weather = 1.0 - texture(WeatherMap, uv.xz / 5.0).r;
    base *= weather;

    weather = saturate(texture(WeatherMap, 1.0 - uv.zx / 40.0).r + 0.2);
    h1 *= weather;
    h2 *= weather;

    base = remap(base, 1.0 - Clouds.Coverage * h1 * h2, 1.0, Clouds.Coverage, 1.0);

    if (base == 0.0)
    {
        return base;
    }

    uv =  GetUV(x0, 400.0, 0.2);

    vec4 high_frequency_noise = texture(CloudHighFrequency, uv, 3);
    float high_frequency_fbm = high_frequency_noise.r * 0.625 + high_frequency_noise.g * 0.25 + high_frequency_noise.b * 0.125;
    float high_frequency_modifier = mix(high_frequency_fbm, 1.0 - high_frequency_fbm, saturate((1.0 - height) * 5.0));

    base = remap(base, high_frequency_modifier * mix(0.15, 0.45, Clouds.Coverage), 1.0, 0.0, 1.0);
    return saturate(height * Clouds.Density * base);
}

float SampleCloudShadow(vec3 x1)
{
    const int steps = 5;
    float bottomBound = Rcb + Rcdelta * 0.5 * (1.0 - max(Clouds.Coverage, 0.25));

    float bottomDistance = SphereMinDistance(x1, ubo.SunDirection.xyz, vec3(0.0), bottomBound);
    float topDistance = SphereMinDistance(x1, ubo.SunDirection.xyz, vec3(0.0), Rct);
    vec3 x0 = x1 + ubo.SunDirection.xyz * bottomDistance;
    float len = topDistance - bottomDistance;

    float Stepsize = len / float(steps);

    float Density = 0.0;
    for (int i = 0; i < steps; i++)
    {
        x0 += ubo.SunDirection.xyz * Stepsize;
        Density += SampleCloud(x0);
    }

    if (Density == 0.0)
        return 1.0;

    return saturate(0.01 + BeerLambert(Density, Stepsize));
}

void SampleAtmosphere(in vec3 Eye, in vec3 World, in vec3 View, in vec3 Sun, out SAtmosphere Atmosphere)
{
    int steps = 64;
    float len = distance(Eye, World);
    float Stepsize = len / float(steps);
    vec3 Ray = Eye;

    Atmosphere.S = vec3(0.0);
    Atmosphere.L = vec3(0.0);
    Atmosphere.E = vec3(0.0);
    Atmosphere.T = vec3(0.0);
    Atmosphere.Shadow = 0.0;
#if 0
    for (int i = 0; i < steps; i++)
    {
        vec3 Temp = Ray;
        Ray += View * Stepsize;

        SAtmosphere AtmosphereItter;
        AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, Temp, Ray, Sun, AtmosphereItter);

        float Shadow = AtmosphereItter.Shadow * mix(1.0, SampleCloudShadow(Ray), AtmosphereItter.Shadow * AtmosphereItter.Shadow);
        
        Atmosphere.S += AtmosphereItter.S * Shadow;
        Atmosphere.L += AtmosphereItter.L * Shadow;
        Atmosphere.E += AtmosphereItter.E * Shadow;
        Atmosphere.T = AtmosphereItter.T;
        Atmosphere.Shadow = Shadow;
    }
#else
    SAtmosphere AtmosphereStart, AtmosphereEnd;
    AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, Eye, Eye + View * Stepsize, Sun, AtmosphereStart);
    AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, Eye + View * Stepsize, World, Sun, AtmosphereEnd);

    Atmosphere.S = AtmosphereStart.S;
    Atmosphere.L = AtmosphereStart.L;
    Atmosphere.E = AtmosphereStart.E;

    vec3 L = vec3(0.0), S = vec3(0.0), E = vec3(0.0);
    for (int i = 0; i < steps; i++)
    {
        Ray += View * Stepsize;

        float w0 = float(i) / float(steps);
        float w1 = float(i + 1) / float(steps);

        Atmosphere.Shadow = mix(AtmosphereStart.Shadow, AtmosphereEnd.Shadow, w1);
        float Shadow = mix(1.0, SampleCloudShadow(Ray), Atmosphere.Shadow * Atmosphere.Shadow);

        S = mix(AtmosphereStart.S, AtmosphereEnd.S, w1) - mix(AtmosphereStart.S, AtmosphereEnd.S, w0);
        L = mix(AtmosphereStart.L, AtmosphereEnd.L, w1);
        E = mix(AtmosphereStart.E, AtmosphereEnd.E, w1);

        Atmosphere.S += S * Shadow;
        Atmosphere.L += L * Atmosphere.Shadow;
        Atmosphere.E += E * Atmosphere.Shadow;
        Atmosphere.Shadow = Shadow;
    }

    Atmosphere.T = AtmosphereEnd.T;
#endif
    Atmosphere.S *= GetScatteringFactor(Clouds.Coverage);
    Atmosphere.L /= float(steps);
    Atmosphere.E /= float(steps);
}

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

    vec3  F0 = mix(vec3(0.04), Material.Albedo.rgb, Material.Metallic);
    vec3  F  = Material.Specular * F_FresnelSchlick(HdotV, F0);
    float G  = G_GeometrySmith(NdotV, NdotL, Material.Roughness);
    float D  = D_DistributionGGX(NdotH, Material.Roughness);

    vec3 kD       = (vec3(1.0) - F) * vec3(1.0 - Material.Metallic);
    vec3 specular = (F * D * G) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 diffuse  = kD * DisneyDiffuse(Material.Albedo.rgb, NdotL, NdotV, LdotH, Material.Roughness);
    vec3 ambient  = vec3(mix(1e-3, 1e-4, Material.Metallic)) * Material.Albedo.rgb;

#if 0
    SAtmosphere Atmosphere;
    AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, Eye, World, Sun, Atmosphere);

    float Shadow = Atmosphere.Shadow * mix(1.0, SampleCloudShadow(World), Atmosphere.Shadow * Atmosphere.Shadow);
    vec3 scatter  = GetScatteringFactor(Clouds.Coverage) * Atmosphere.S;
    diffuse  = diffuse  * Shadow * (Atmosphere.L + Atmosphere.E);
    specular = specular * Shadow * Atmosphere.L;
    ambient  = ambient  * Atmosphere.T;
#else
    SAtmosphere Atmosphere;
    SampleAtmosphere(Eye, World, -V, Sun, Atmosphere);

    vec3 scatter  = Atmosphere.S;
    diffuse  = Atmosphere.Shadow * diffuse  * (Atmosphere.L + Atmosphere.E);
    specular = Atmosphere.Shadow * specular * Atmosphere.L;
    ambient  = ambient  * Atmosphere.T;
#endif
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