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
layout(binding = 14) uniform sampler2D WeatherMap;
layout(binding = 15) uniform CloudLayer
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

float SampleCloud(vec3 x0, float transmittance, float height)
{
    vec3 uv = GetUV(x0, 50.0, Clouds.WindSpeed * 0.01);

    float base = textureLod(CloudLowFrequency, uv, 0).r;

    float h1 = pow(height, 0.65);
    float h2 = saturate(remap(1.0 - height, 0.0, mix(0.05, 0.2, Clouds.Coverage), 0.0, 1.0));

    vec3 temp = texture(WeatherMap, uv.xz / 5.0).rgb;
    float weather1 = 1.0 - saturate(temp.x + 0.2) * saturate(0.5 + temp.y);
    float weather2 = saturate(texture(WeatherMap, uv.zx / 40.0).r + 0.2);

    base *= weather1;
    float shape = 1.0 - Clouds.Coverage * h1 * h2 * weather2;
    base = height * saturate(remap(base, shape, 1.0, pow(Clouds.Coverage, 1.5), 1.0));

    return saturate(mix(2.0 * Clouds.Density, Clouds.Density / 2.0, transmittance) * base);
}

float SampleCloudShadow(vec3 x1)
{
    const int steps = 5;
    float topBound = Rct;
    float bottomBound = Rcb + Rcdelta * (0.5 - min(Clouds.Coverage, 0.5));
    float bottomDistance = SphereMinDistance(x1, ubo.SunDirection.xyz, vec3(0.0), bottomBound);
    float topDistance = SphereMinDistance(x1, ubo.SunDirection.xyz, vec3(0.0), topBound);
    vec3 x0 = x1 + ubo.SunDirection.xyz * bottomDistance;
    float len = topDistance - bottomDistance;

    float Stepsize = len / float(steps);

    float Density = 0.0;
    float transmittance = 1.0; 
    for (int i = 0; i < steps; i++)
    {
        x0 += ubo.SunDirection.xyz * Stepsize;
        float height = 1.0 - GetHeightFraction(x0, bottomBound, topBound);
        Density = SampleCloud(x0, transmittance, height);
        transmittance *= BeerLambert(Density, Stepsize);
    }

    return saturate(0.05 + transmittance);
}

void SampleAtmosphere(vec3 Eye, vec3 World, vec3 View, vec3 Sun, out SAtmosphere Atmosphere, inout float Shadow, inout float LightIntensity)
{
    float Re = length(Eye);
    float len = distance(Eye, World);
    float T = saturate(len / 1e5);
    LightIntensity = mix(1.0, 0.25, smoothstep(0.0, 1.0, Clouds.Coverage)) * MaxLightIntensity;
    
    if (Clouds.Coverage == 0.0 || Re > Rt + Rcdelta)
    {
        AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, T, 1.0, Eye, World, Sun, Atmosphere);
        Shadow = Atmosphere.Shadow <= 0.1 ? mix(0.05, 1.0, Atmosphere.Shadow / 0.1) : 1.0;
        Atmosphere.S *= Shadow * LightIntensity;
        Atmosphere.E *= Shadow * LightIntensity;
        Atmosphere.L *= Shadow;
        return;
    }

    float a = Re < Rt ? 0.0 : (Re - Rt) / Rcdelta;
    
    int steps = int(floor(mix(32, 16, saturate(0.01 * len / 1e4))));
    float Stepsize = len / float(steps);
    vec3 Ray = Eye;

    SAtmosphere AtmosphereStart, AtmosphereEnd;
    AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, T, 1.0, Eye, Eye + View * Stepsize, Sun, AtmosphereStart);
    AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, T, 1.0, Eye, World, Sun, AtmosphereEnd);

    Atmosphere.T = AtmosphereEnd.T;
    Atmosphere.Shadow = AtmosphereStart.Shadow;

    float w0 = 0.0;
    float incr = 1.0 / float(steps);
    Atmosphere.S = LightIntensity * AtmosphereStart.S;

    for (int i = 1; i <= steps; i++)
    {
        Ray += View * Stepsize;

        float w1 = 2.0 * smootherstep(0.0, 2.0, i * incr);
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

    Atmosphere.E = AtmosphereEnd.E * Shadow * LightIntensity;
    Atmosphere.L = AtmosphereEnd.L * Shadow;
}

// get specular and ambient part from sunlight
vec3 DirectSunlight(in vec3 Eye, in vec3 World, in vec3 View, in vec3 Sun, in SMaterial Material)
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

    float Shadow = 1.0;
    SAtmosphere Atmosphere;
    float LightIntensity = MaxLightIntensity;
    SampleAtmosphere(Eye, World, -V, Sun, Atmosphere, Shadow, LightIntensity);

    vec3 Illumination = LightIntensity * Atmosphere.L * NdotL;

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
    vec3 scattering = Atmosphere.S;

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
        
        Color.rgb = DirectSunlight(Eye, World, View, Sun, Material);

        if (Depth < SkyDepth)
        {
            vec3 CloudPosition = GetWorldPosition(UV, SkyDepth);
            float d = distance(CloudPosition, World);

            float a = 1.0 - SkyColor.a;
            float b = 1.0 - saturate(exp(-d * Clouds.Density * 0.01));
            Color.rgb = mix(SkyColor.rgb, Color.rgb, 1.0 - a * b);
            Color.a = max(a, Color.a);
        }
    }
    else
    {
        Color.rgb = SkyColor.rgb;
        Color.a = 1.0 - SkyColor.a;
    }

    Color.rgb += (1.0 - Color.a) * MaxLightIntensity * SkyScattering(TransmittanceLUT, InscatteringLUT, Eye, -View, Sun);

    outColor = vec4(Color.rgb, 1.0);
}