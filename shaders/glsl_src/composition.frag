#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "brdf.glsl"

layout(input_attachment_index = 0, binding = 1) uniform subpassInput HDRColor;
layout(input_attachment_index = 1, binding = 2) uniform subpassInput HDRNormals;
layout(input_attachment_index = 2, binding = 3) uniform subpassInput HDRDeferred;
layout(input_attachment_index = 3, binding = 4) uniform subpassInput SceneDepth;

layout(binding = 5) uniform sampler2D SceneBackground;
layout(binding = 6) uniform sampler2D BackgroundDepth;

layout(binding = 7) uniform sampler2D TransmittanceLUT;
layout(binding = 8) uniform sampler2D IrradianceLUT;
layout(binding = 9) uniform sampler3D InscatteringLUT;
layout(binding = 10) uniform samplerCube DiffuseLUT;
layout(binding = 11) uniform samplerCube SpecularLUT;
layout(binding = 12) uniform sampler2D BRDFLUT;
layout(binding = 13) uniform sampler3D CloudLowFrequency;
layout(binding = 14) uniform sampler3D CloudHighFrequency;
layout(binding = 15) uniform sampler2D WeatherMap;
layout(binding = 16) uniform CloudLayer
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
    vec3 anim = vec3(ubo.Time * speed_mod);
    return scale * uv + anim;
}

float GetHeightFraction(vec3 x0, float bottomBound, float topBound)
{
    return saturate((length(x0) - bottomBound) / (topBound - bottomBound));;
}

float SampleCloud(vec3 x0)
{
    vec3 uv = GetUV(x0, 50.0, Clouds.WindSpeed * 0.01);
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

    uv =  GetUV(x0, 300.0, -0.05);

    vec4 high_frequency_noise = texture(CloudHighFrequency, uv, 3);
    float high_frequency_fbm = high_frequency_noise.r * 0.625 + high_frequency_noise.g * 0.25 + high_frequency_noise.b * 0.125;
    float high_frequency_modifier = mix(high_frequency_fbm, 1.0 - high_frequency_fbm, saturate(height * 25.0));

    base = remap(base, high_frequency_modifier * mix(0.35, 0.55, Clouds.Coverage), 1.0, 0.0, 1.0);
    return saturate(height * Clouds.Density * base);
}

float SampleCloudShadow(vec3 x1)
{
    const int steps = 3;
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

    return saturate(BeerLambert(Density, Stepsize));
}

void SampleAtmosphere(in vec3 Eye, in vec3 World, in vec3 View, in vec3 Sun, out SAtmosphere Atmosphere, inout float Shadow)
{
    if (Clouds.Coverage == 0.0)
    {
        AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, 1.0, 1.0, Eye, World, Sun, Atmosphere);
        Atmosphere.S *= MaxLightIntensity;
        Atmosphere.E *= MaxLightIntensity;
        return;
    }

    float len = distance(Eye, World);
    int steps = int(floor(mix(32, 16, saturate(0.01 * len / 1e4))));
    float Stepsize = len / float(steps);
    vec3 Ray = Eye;

    SAtmosphere AtmosphereStart, AtmosphereEnd;
    AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, 1.0, 1.0, Eye, Eye + View * Stepsize, Sun, AtmosphereStart);
    AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, 1.0, 1.0, Eye, World, Sun, AtmosphereEnd);

    Atmosphere.T = AtmosphereEnd.T;
    Atmosphere.Shadow = AtmosphereEnd.Shadow;
    
    float w0 = 0.0;
    float incr = 1.0 / float(steps);
    Atmosphere.S = MaxLightIntensity * AtmosphereStart.S;

    for (int i = 1; i <= steps; i++)
    {
        Ray += View * Stepsize;

        float w1 = 2.0 * smootherstep(0.0, 2.0, i * incr);

        Shadow = mix(1.0, SampleCloudShadow(Ray), mix(AtmosphereStart.Shadow, AtmosphereEnd.Shadow, w1));
        Atmosphere.S += Shadow * (MaxLightIntensity * mix(AtmosphereStart.S, AtmosphereEnd.S, w1) - MaxLightIntensity * mix(AtmosphereStart.S, AtmosphereEnd.S, w0));
        w0 = w1;
    }

    Atmosphere.E = AtmosphereEnd.E * Shadow * MaxLightIntensity;
    Atmosphere.L = AtmosphereEnd.L * Shadow;
}

// get specular and ambient part from sunlight
vec3 DirectSunlight(in vec3 Eye, in vec3 World, in vec3 Sun, in SMaterial Material)
{
    vec3 Lo = vec3(0.0);
    vec3 N = Material.Normal;
    vec3 S = Sun;
    vec3 V = normalize(Eye - World);
    vec3 H = normalize(V + S);

    float NdotV = abs(dot(N, V)) + 1e-5;
    float NdotL = saturate(dot(N, S));
    float NdotH = saturate(dot(N, H));
    float LdotH = saturate(dot(S, H));
    float HdotV = saturate(dot(H, V));

    float Shadow = 1.0;
    SAtmosphere Atmosphere;
    SampleAtmosphere(Eye, World, -V, Sun, Atmosphere, Shadow);

    vec3 Illumination = MaxLightIntensity * Atmosphere.L * NdotL;

    float A = Material.Roughness;
    float A2 = A * A;

    vec3  F0 = Material.Specular * mix(vec3(0.04), Material.Albedo.rgb, Material.Metallic);
    vec3 F90 = max(vec3(F0), 1.0 - A);
    vec3  F  = F_FresnelSchlick(LdotH, F0, vec3(1.0));
    vec3 kD  = vec3(1.0 - F) * vec3(1.0 - Material.Metallic);

    if (NdotL > 0.0)
    {
        float G  = G_GeometrySmith(NdotV, NdotL, A);
        float D  = D_DistributionGGX(NdotH, A);

        Lo += ONE_OVER_PI * F * G * D / (4.0 * NdotL * NdotV + 0.001);
        Lo += ONE_OVER_PI * kD * DisneyDiffuse(Material.Albedo.rgb, NdotL, NdotV, LdotH, A);
    }

    vec3 R = 2.0f * N * dot(V, N) - V;
    R = mix(N, R, (1.0f - A2) * (sqrt(1.0f - A2) + A2));
    vec3 reflection = textureLod(SpecularLUT, R, A2 * float(textureQueryLevels(SpecularLUT) - 1)).rgb;
    vec2 brdf  = texture(BRDFLUT, vec2(NdotV, A2)).rg;
    vec3 irradiance = Atmosphere.E + texture(DiffuseLUT, N).rgb;

    vec3 diffuse = Material.Albedo.rgb * irradiance;
    vec3 specular = reflection * (F * brdf.x + brdf.y);
    vec3 ambient  = Atmosphere.L * Material.AO * (kD * diffuse + specular);
    vec3 scattering = clamp(1.0 - Clouds.Coverage, 0.25, 1.0) * Atmosphere.S;

    return scattering + ambient + Illumination * Lo;
    // return ambient + Illumination * Lo;
}

void main()
{
    vec4 SkyColor  = texture(SceneBackground, UV);
    float SkyDepth = texture(BackgroundDepth, UV).r;
    vec4 Color     = subpassLoad(HDRColor);
    float Depth    = subpassLoad(SceneDepth).r;
    vec4 Deferred  = subpassLoad(HDRDeferred).rgba;
    vec3 Normal    = normalize(subpassLoad(HDRNormals).rgb);
 
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