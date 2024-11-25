#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "noise.glsl"

vec3 marchEnd;
vec3 marchStart;
vec3 marchDirection;
vec3 fromCamera;

float distanceEnd;
float distanceStart;
float distanceCamera;
float distancePosition;

float topBound;
float bottomBound;

// used for shadow sampling
vec3 light_kernel[] =
{
    vec3(0.5, 0.5, 0.5),
    vec3(-0.628, 0.64, 0.234),
    vec3(0.23, -0.123, 0.75),
    vec3(0.98, 0.45, -0.32),
    vec3(-0.1, -0.54, 0.945),
    vec3(0.0, 0.6, -0.78),
};

layout(location = 0) in vec2 ScreenUV;
layout(location=0) out vec4 outScattering;

layout(set = 1, binding = 1) uniform CloudLayer
{
    float Coverage;
    float Absorption;
    float WindSpeed;
} Clouds;

layout(set = 1, binding = 2) uniform sampler3D CloudLowFrequency;
layout(set = 1, binding = 3) uniform sampler3D CloudHighFrequency;
layout(set = 1, binding = 4) uniform sampler2D WeatherMap;
layout(set = 1, binding = 5) uniform sampler2D TransmittanceLUT;
layout(set = 1, binding = 6) uniform sampler3D InscatteringLUT;

float GetHeightFraction(vec3 p)
{
    float t1 = length(p) - bottomBound;
    float t2 = topBound - bottomBound;
    return saturate(t1 / t2);
}

vec3 GetUV(vec3 p, float scale, float speed_mod)
{
    return scale * p / topBound + vec3(ubo.Time * Clouds.WindSpeed * speed_mod);
}

vec3 GetSunColor(vec3 Eye, vec3 V, vec3 S)
{
    return SphereIntersect(Eye, V, vec3(0.0), Rg) ? vec3(0.0) : 3.0 * vec3(smoothstep(0.9998, 1.0, max(dot(V, S), 0.0)));
}

// Cheaper version of Sample-Density
float SampleCloudShape(vec3 x0, int lod)
{
    vec3 uv =  GetUV(x0, 45.0, 0.01);
    float height = 1.0 - GetHeightFraction(x0);
    float base = textureLod(CloudLowFrequency, uv, lod).r;

    base = remap(base, 1.0 - Clouds.Coverage, 1.0, 0.0, 1.0);
    base *= saturate(remap(1.0 - height, 0.0, 0.1, 0.0, 1.0));
    base *= Clouds.Coverage;
    base *= height;
    
    return saturate(base);
}

// GPU-Pro-7
float SampleDensity(vec3 x0, int lod)
{
    vec3 uv =  GetUV(x0, 45.0, 0.01);
    float height = 1.0 - GetHeightFraction(x0);
    float base = textureLod(CloudLowFrequency, uv, lod).r;
    
    base = remap(base, 1.0 - Clouds.Coverage, 1.0, 0.0, 1.0);
    base *= saturate(remap(1.0 - height, 0.0, 0.1, 0.0, 1.0));
    base *= Clouds.Coverage;
    base *= height;

    uv =  GetUV(x0, 400.0, 0.2);

    vec4 high_frequency_noise = texture(CloudHighFrequency, uv);
    float high_frequency_fbm = high_frequency_noise.r * 0.625 + high_frequency_noise.g * 0.25 + high_frequency_noise.b * 0.125;
    float high_frequency_modifier = mix(high_frequency_fbm, 1.0 - high_frequency_fbm, saturate((1.0 - height) * 5.0));

    base = remap(base, high_frequency_modifier * mix(0.25, 0.45, Clouds.Coverage), 1.0, 0.0, 1.0);
    return saturate(base);
}

// volumetric-cloudscapes-of-horizon-zero-dawn
float MarchToLight(vec3 rs, vec3 rd, float stepsize)
{
    vec3 pos = rs;
    float density = 0.0;
    float radius = 1.0;
    float transmittance = 1.0;

    stepsize *= 3.f;
    for (int i = 0; i <= 6; i++)
    {
        pos = rs + float(i) * radius * light_kernel[i];

        float sampled_density = 0.0;

        if (density < 0.3)
        {
            sampled_density = SampleDensity(pos, 2);
        }
        else
        {
            sampled_density = SampleCloudShape(pos, 2);
        }

        if (sampled_density > 0.0)
        {
            float transmittance_light = BeerLambert(sampled_density * Clouds.Absorption, stepsize);
            transmittance *= transmittance_light;
        }
        
        density += sampled_density;
        rs += stepsize * rd;
        radius += 1.f / 6.f;
    }

    return transmittance;
}

void FillScattering(float T)
{
    SAtmosphere Atmosphere;

    // Clouds scattering
    float t = SphereMinDistance(ubo.CameraPosition.xyz, fromCamera, vec3(0.0), topBound);
    AtmosphereAtPoint(TransmittanceLUT, InscatteringLUT, ubo.CameraPosition.xyz, t, fromCamera, ubo.SunDirection.xyz, Atmosphere);
    vec3 S = T * Atmosphere.S;
    S = S - S * outScattering.a;

    // Sky scattering
    t = SphereMinDistance(ubo.CameraPosition.xyz, fromCamera, vec3(0.0), Rt);
    AtmosphereAtPoint_2(TransmittanceLUT, InscatteringLUT, ubo.CameraPosition.xyz, t, fromCamera, ubo.SunDirection.xyz, Atmosphere);
    S += (Atmosphere.S + GetSunColor(ubo.CameraPosition.xyz, fromCamera, ubo.SunDirection.xyz) * Atmosphere.L * MaxLightIntensity) * outScattering.a;
    outScattering.rgb = Atmosphere.L * outScattering.rgb + S;
}

// I doubt it's physically correct but looks nice
// Unoptimized and expensive
void MarchToCloud()
{
    vec3 rl = normalize(ubo.SunDirection.xyz);

    marchDirection = normalize(marchEnd - marchStart);
    
    int i = 0;
    vec3 pos = marchStart;
    float sample_density = 0.0;
    float phase = HGDPhase(dot(rl, fromCamera), 0.75, -0.15, 0.5, 0.35);
    //float phase = DualLobeFunction(dot(rl, marchDirection), MieG, -0.25, 0.65);

    float mean_depth = 0.0;
    float mean_transmittance = 0.0;

    // take larger steps and
    // skip empty space until cloud is found

    int steps = 24;
    float len = distance(marchStart, marchEnd);
    float stepsize = len / float(steps);
    for (i; i < steps && sample_density == 0.0; i++)
    {
        pos = marchStart + stepsize * marchDirection * i;
        sample_density = SampleCloudShape(pos, 2);
    }

    if (sample_density == 0.0)
    {
        FillScattering(0.0);
        return;
    }

    // go more precise for clouds
    float DotEH = dot(normalize(ubo.CameraPosition.xyz), fromCamera);

    steps = int(mix(256, 32, abs(DotEH)) * (1.0 - float(max(i - 1, 0)) / float(steps)));
    stepsize = len / float(steps);

    float march_to_camera = distance(ubo.CameraPosition.xyz, marchStart);
    float aep_distance = SphereMinDistance(ubo.CameraPosition.xyz, fromCamera, vec3(0.0), topBound);

    for (i = 0; i < steps; i++)
    {
        pos = marchStart + i * marchDirection * stepsize;
        sample_density = SampleDensity(pos, 0);

        if (sample_density > 0.0) 
        {
            float extinction = Clouds.Absorption * sample_density;

            float transmittance = BeerLambert(extinction, stepsize);
            float luminance = MaxLightIntensity * phase * MarchToLight(pos, rl, stepsize);

            vec3 E = extinction * vec3(luminance);
            E = vec3(E - transmittance * E) / max(extinction, 1e-6);

            outScattering.rgb += E * outScattering.a;
            outScattering.a *= transmittance;

            //mean_depth += (float(i + 1) / float(steps)) * outScattering.a;
            mean_depth += ((float(i + 1) * stepsize + march_to_camera) / aep_distance) * outScattering.a;
            mean_transmittance += outScattering.a;
        }
    }

    // apply aerial perspective and sky scattering
    
    float T = 0.0; // zero for no cloud
    if (outScattering.a != 1.0)
    {
        T = (mean_depth / mean_transmittance);

        // SAtmosphere Atmosphere;
        // AtmosphereAtPoint(TransmittanceLUT, InscatteringLUT, marchStart, distance(ubo.CameraPosition.xyz, marchStart), -fromCamera, ubo.SunDirection.xyz, Atmosphere);
        // outScattering.rgb = outScattering.rgb * Atmosphere.L;
    }

    FillScattering(T);
}

void main()
{
    vec4 ScreenNDC = vec4(2.0 * ScreenUV - 1.0, 1.0, 1.0);
    vec4 ScreenView = ubo.ProjectionMatrixInverse * ScreenNDC;
    vec4 ScreenWorld = vec4(ubo.ViewMatrixInverse * vec4(ScreenView.xy, -1.0, 0.0));
    vec3 RayOrigin = vec3(ubo.CameraPositionFP64.xyz);
    vec3 SphereCenter = vec3(0.0, 0.0, 0.0);
    fromCamera = normalize(ScreenWorld.xyz);
    outScattering = vec4(0.0, 0.0, 0.0, 1.0);

    bottomBound = Rcb + Rcdelta * 0.5 * (1.0 - max(Clouds.Coverage, 0.0075));
    topBound = Rcb + Rcdelta * 0.5 * (1.0 + max(Clouds.Coverage, 0.0075));

    float ground = SphereMinDistance(RayOrigin, fromCamera, SphereCenter, Rg);
    distanceCamera = length(RayOrigin);

    if (distanceCamera > Rt)
    {
        discard;
    }

    if (distanceCamera < bottomBound && ground == 0.0)
    {
        distanceStart = SphereMinDistance(RayOrigin, fromCamera, SphereCenter, bottomBound);
        distanceEnd = SphereMinDistance(RayOrigin, fromCamera, SphereCenter, topBound);

        marchStart = RayOrigin + fromCamera * distanceStart;
        marchEnd = RayOrigin + fromCamera * distanceEnd;

        MarchToCloud();
    }
#if 0
    else if (distanceCamera > bottomBound && distanceCamera < topBound)
    {
        float t1 = SphereMinDistance(RayOrigin, fromCamera, SphereCenter, bottomBound);
        float t2 = SphereMinDistance(RayOrigin, fromCamera, SphereCenter, topBound);
    
        marchStart = RayOrigin;
    
        if (t1 == 0.0)
        {
            marchEnd = RayOrigin + fromCamera * t2;
        }
        else if (t2 == 0.0)
        {
            marchEnd = RayOrigin + fromCamera * t1;
        }
        else
        {
            marchEnd = RayOrigin + fromCamera * min(t1, t2);
        }

        MarchToCloud();
    }
    else if (distanceCamera > topBound)
    {
        distanceStart = SphereMinDistance(RayOrigin, fromCamera, SphereCenter, topBound);
        distanceEnd = SphereMinDistance(RayOrigin, fromCamera, SphereCenter, bottomBound);
        if (distanceEnd == 0.0)
        {
            FillScattering(0.0);
            return;
        }
    
        marchStart = RayOrigin + fromCamera * distanceStart;
        marchEnd = RayOrigin + fromCamera * distanceEnd;

        MarchToCloud();
    }
#endif
    else
    {
        FillScattering(0.0);
        return;
    }
}