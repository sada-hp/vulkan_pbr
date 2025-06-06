#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "brdf.glsl"

layout(binding = 1) uniform sampler2D HDRColor;
layout(binding = 2) uniform sampler2D HDRNormals;
layout(binding = 3) uniform sampler2D HDRDeferred;
layout(binding = 4) uniform sampler2D SceneDepth;
layout(binding = 5) uniform sampler2D TransmittanceLUT;
layout(binding = 6) uniform sampler2D IrradianceLUT;
layout(binding = 7) uniform sampler3D InscatteringLUT;
layout(binding = 8) uniform samplerCube DiffuseLUT;
layout(binding = 9) uniform samplerCube SpecularLUT;
layout(binding = 10) uniform sampler2D BRDFLUT;
layout(binding = 11) uniform sampler3D CloudLowFrequency;
layout(binding = 12) uniform sampler2D WeatherMap;
layout(binding = 13) uniform sampler2DArray WeatherMapCube;
layout(binding = 14) uniform CloudLayer
{
    float Coverage;
    float CoverageSq;
    float HeightFactor;
    float BottomSmoothnessFactor;
    float LightIntensity;
    float Ambient;
    float Density;
    float BottomBound;
    float TopBound;
    float BoundDelta;
} Clouds;

layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 outColor;

vec3 GetUV(vec3 p, float scale, float wind)
{
    vec3 uv = p / Clouds.TopBound;
    return scale * uv + wind * ubo.Time;
}

float GetHeightFraction(vec3 x0)
{
    return saturate((length(x0) - Clouds.BottomBound) / Clouds.BoundDelta);
}

float SampleCloud(vec3 x0, float height)
{
    const int lod = 1;

    vec3 uv = GetUV(x0, 50.0, ubo.Wind * 0.01);

    float base = textureLod(CloudLowFrequency, uv, lod).r;

    float h1 = pow(height, 1.35);
    float h2 = saturate(remap(1.0 - height, 0.0, Clouds.BottomSmoothnessFactor, 0.0, 1.0));

    vec3 sx = x0 / Clouds.TopBound;
    vec2 anim = vec2(ubo.Wind * 0.01 * ubo.Time) + vec2(sin(1e-6 * ubo.Time), cos(1e-6 * ubo.Time));
    vec3 temp = SampleArrayAsCube(WeatherMapCube, sx, 10.0, anim, lod).rgb;
    float weather1 = 1.0 - saturate(temp.x + 0.2) * saturate(0.5 + temp.y);
    float weather2 = saturate(SampleArrayAsCube(WeatherMapCube, sx, 1.0, anim, lod).r + 0.2);

    base *= weather1;
    float shape = 1.0 - Clouds.Coverage * h1 * h2 * weather2;
    base = height * saturate(remap(base, shape, 1.0, Clouds.HeightFactor, 1.0));
    return saturate(remap(base, 0.75 * mix(0.5, 0.2, Clouds.CoverageSq), 1.0, 0.0, 1.0));
}

float SampleCloudShadow(vec3 x1)
{
    const int steps = 5;
    float bottomDistance = SphereMinDistance(x1, ubo.SunDirection.xyz, vec3(0.0), Clouds.BottomBound);
    float topDistance = SphereMinDistance(x1, ubo.SunDirection.xyz, vec3(0.0), Clouds.TopBound);
    vec3 x0 = x1 + ubo.SunDirection.xyz * bottomDistance;
    float len = distance(x1 + ubo.SunDirection.xyz * bottomDistance, x1 + ubo.SunDirection.xyz * topDistance);

    float Stepsize = len / float(steps);

    float Density = 0.0;
    for (int i = 0; i < steps; i++)
    {
        x0 += ubo.SunDirection.xyz * Stepsize;
        float height = 1.0 - GetHeightFraction(x0);
        Density += SampleCloud(x0, height);
    }

    return Density > 0.0 ? saturate(0.05 + BeerLambert(0.1 * Clouds.Density * Density, Stepsize)) : 1.0;
}

void SampleAtmosphere(vec3 Eye, vec3 World, vec3 View, vec3 Sun, out SAtmosphere Atmosphere, inout float Shadow, inout float LightIntensity)
{
    float Re = length(Eye);
    float len = distance(Eye, World);
    LightIntensity = Clouds.LightIntensity * MaxLightIntensity;
    
    if (Clouds.Coverage == 0.0 || Re > Rt + Rdelta)
    {
        AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, Eye, World, Sun, Atmosphere);
        Shadow = Atmosphere.Shadow <= 0.1 ? mix(0.05, 1.0, Atmosphere.Shadow / 0.1) : 1.0;
        Atmosphere.S *= Shadow * LightIntensity;
        Atmosphere.E *= Shadow * Atmosphere.Shadow * LightIntensity;
        Atmosphere.L *= Shadow;
        return;
    }

    float a = Re < Rt ? 0.0 : saturate((Re - Rt) / Rdelta);
    
    int steps = int(floor(mix(32, 16, saturate(0.01 * len / 1e4))));
    float Stepsize = len / float(steps);
    vec3 Ray = Eye;

    SAtmosphere AtmosphereStart, AtmosphereEnd;
    AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, Eye, Eye + View * Stepsize, Sun, AtmosphereStart);
    AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, Eye, World, Sun, AtmosphereEnd);

    Atmosphere.T = AtmosphereEnd.T;
    Atmosphere.Shadow = AtmosphereStart.Shadow;

    float w0 = 0.0;
    float incr = 1.0 / float(steps);
    Atmosphere.S = LightIntensity * AtmosphereStart.S;

    for (int i = 1; i <= steps; i++)
    {
        Ray += View * Stepsize;

        float w1 = i * incr;
        Atmosphere.Shadow = mix(AtmosphereStart.Shadow, AtmosphereEnd.Shadow, w1);

        vec4 ScreenUV = GetScreenPosition(vec4(Ray, 1.0));
        if (a == 1.0)
        {
            Shadow = 1.0;
        }
        else
        {
            Shadow = mix(SampleCloudShadow(Ray), 1.0, a);
            if (Atmosphere.Shadow <= 0.1)
            {
                Shadow = mix(0.05, Shadow, Atmosphere.Shadow / 0.1);
            }
        }

        Atmosphere.S += Shadow * (LightIntensity * mix(AtmosphereStart.S, AtmosphereEnd.S, w1) - LightIntensity * mix(AtmosphereStart.S, AtmosphereEnd.S, w0));
        w0 = w1;
    }

    Atmosphere.E = AtmosphereEnd.E * Atmosphere.Shadow * Shadow * LightIntensity;
    Atmosphere.L = AtmosphereEnd.L * Shadow;
}

// get specular and ambient part from sunlight
vec3 DirectSunlight(in vec3 Eye, in vec3 World, in vec3 View, in vec3 Sun, inout float Shadow, in SMaterial Material)
{
    vec3 Lo = vec3(0.0);
    vec3 N = Material.Normal;
    vec3 S = Sun;
    vec3 V = View;
    vec3 H = normalize(V + S);

    float NdotV = abs(dot(N, V)) + 1e-5;
    float NdotL = saturate(dot(N, S));
    float NdotH = saturate(dot(N, H));
    float LdotH = saturate(dot(S, H));
    float HdotV = saturate(dot(H, V));

    SAtmosphere Atmosphere;
    float LightIntensity = MaxLightIntensity;
    SampleAtmosphere(Eye, World, -V, Sun, Atmosphere, Shadow, LightIntensity);

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

        Lo += F * G * D / (4.0 * NdotL * NdotV + 0.001);
        Lo += kD * DisneyDiffuse(Material.Albedo.rgb, NdotL, NdotV, LdotH, A);
    }

    vec3 R = 2.0f * N * dot(V, N) - V;
    R = mix(N, R, (1.0f - A2) * (sqrt(1.0f - A2) + A2));
    vec3 reflection = textureLod(SpecularLUT, R, A2 * float(textureQueryLevels(SpecularLUT) - 1)).rgb;
    vec2 brdf  = texture(BRDFLUT, vec2(NdotV, A2)).rg;
    vec3 irradiance = max(NdotL, 0.05) * (0.5 * Shadow + 0.25 * dFdx(Shadow) + 0.25 * dFdy(Shadow)) * texture(DiffuseLUT, N).rgb;

    vec3 diffuse = Material.Albedo.rgb * irradiance;
    vec3 specular = reflection * (F * brdf.x + brdf.y);
    vec3 ambient  = Atmosphere.Shadow * LightIntensity * Material.AO * (kD * diffuse + specular);
    vec3 scattering = Atmosphere.S;

    return scattering + ambient + Lo * (Atmosphere.L + Atmosphere.E) * ONE_OVER_PI * LightIntensity * NdotL;
}
 
vec3 GetSkyColor(vec3 Eye, vec3 View, vec3 Sun)
{
    vec3 SkyColor = vec3(0.0);
    float Hit = SphereMinDistance(Eye, View, vec3(0.0), Rct - Rcdelta * 0.35);

    if (Hit > 0.0)
    {
        vec3 HitPoint = Eye + View * Hit;

        SAtmosphere Atmosphere;
        AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, Eye, HitPoint, Sun, Atmosphere);
        float PhaseF = HGDPhase(0.75 * dot(Sun, View));
        
        vec3 n = normalize(HitPoint);
        float pole = abs(dot(n, vec3(0, 1, 0)));
        vec2 uv1 = 0.5 + vec2(atan(n.z, n.x) * ONE_OVER_2PI, asin(n.y) * ONE_OVER_2PI);
        vec2 uv2 = 0.5 + vec2(atan(n.x, n.y) * ONE_OVER_2PI, asin(n.z) * ONE_OVER_2PI);

        float left = pole > 0.9 ? 0.0 : (pole >= 0.8 ? 1.0 - ((pole - 0.8) / (0.9 - 0.8)) : 1.0);
        float right = 1.0 - left;
        float mask1 = left * texture(WeatherMap, 50.0 * uv1).b + right * texture(WeatherMap, 50.0 * uv2).b;
        float mask2 = left * texture(WeatherMap, 6.0 * uv1).r + right * texture(WeatherMap, 6.0 * uv2).r;
        float mask3 = left * texture(WeatherMap, vec2(85.0, 25.0) * uv1).r + right * texture(WeatherMap, vec2(85.0, 25.0) * uv2).r;

        float altocumulus = (left * texture(WeatherMap, 700.0 * uv1).g + right * texture(WeatherMap, 700.0 * uv2).g);
        float altocumulus_mask2 = dampen(mask2, 0.25);
        float altocumulus_mask1 = saturate(altocumulus_mask2 * remap(dampen(mask1, 0.2), altocumulus_mask2, 1.0, 0.0, 1.0));
        altocumulus = altocumulus_mask1 * altocumulus_mask2 * altocumulus;

        float cirrus = (left * texture(WeatherMap, vec2(125,15) * uv1).r + right * texture(WeatherMap, vec2(125,15) * uv2).r);
        float cirrus_mask = dampen(mask3, 0.35);
        cirrus = cirrus_mask * cirrus;

        float high_level = saturate(dampen(cirrus + altocumulus, 0.2) + cirrus_mask * dampen(1.0 - altocumulus_mask2, 0.5) * cirrus);
        SkyColor += PhaseF * Atmosphere.T * Atmosphere.L * dampen(high_level, 0.005);
    }

    return (SkyColor + SkyScattering(TransmittanceLUT, InscatteringLUT, Eye, View, Sun)) * MaxLightIntensity;
}

void main()
{
    float Shadow = 1.0;
    vec4 Color     = texelFetch(HDRColor, ivec2(gl_FragCoord.xy), 0);
    float Depth    = texelFetch(SceneDepth, ivec2(gl_FragCoord.xy), 0).r;
    vec4 Deferred  = texelFetch(HDRDeferred, ivec2(gl_FragCoord.xy), 0).rgba;
    vec3 Normal    = normalize(texelFetch(HDRNormals, ivec2(gl_FragCoord.xy), 0).rgb);

    vec3 Eye   = ubo.CameraPosition.xyz;
    vec3 Sun   = normalize(ubo.SunDirection.xyz);
    vec3 World = GetWorldPosition(UV, Depth);
    vec3 View = normalize(Eye - World);
 
    if (Color.a != 0.0)
    {
        SMaterial Material;
        Material.Roughness  = Deferred.g;
        Material.Metallic   = Deferred.b;
        Material.AO         = Deferred.r;
        Material.Albedo.rgb = pow(Color.rgb, vec3(2.2));
        Material.Albedo.a   = Color.a;
        Material.Specular   = Deferred.a;
        Material.Normal     = Normal;
        
        Color.rgb = DirectSunlight(Eye, World, View, Sun, Shadow, Material);
    }

    if (Color.a != 1.0)
    {
        Color.rgb += (1.0 - Color.a) * GetSkyColor(Eye, -View, Sun);
    }

    outColor = vec4(Color.rgb, 1.0);
}