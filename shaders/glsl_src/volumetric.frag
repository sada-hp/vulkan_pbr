#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "noise.glsl"

vec3 marchEnd;
vec3 marchStart;
vec3 marchDirection;

float distanceEnd;
float distanceStart;
float distanceCamera;
float distancePosition;
float topBound;

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
    float VerticalSpan;
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
    float t1 = length(p) - Rcb;
    float t2 = topBound - Rcb;
    return saturate(t1 / t2);
}

vec3 GetUV(vec3 p, float scale, float speed_mod)
{
    return scale * p / Rct + vec2(ubo.Time * Clouds.WindSpeed * speed_mod, 0.0).xxx;
}

vec3 GetSunColor(vec3 Eye, vec3 V, vec3 S)
{
    vec3 p;
    return RaySphereintersection(Eye, V, vec3(0.0), Rg, p) ? vec3(0.0) : 3.0 * vec3(smoothstep(0.9998, 1.0, max(dot(V, S), 0.0)));
}

// Cheaper version of Sample-Density
float SampleCloudShape(vec3 x0, int lod)
{
    vec3 uv =  GetUV(x0, 65.0, 0.01);
    float height = 1.0 - GetHeightFraction(x0);
    float base = textureLod(CloudLowFrequency, uv, lod).r * height;

    base = remap(base, 1.0 - Clouds.Coverage, 1.0, 0.0, 1.0);
    base *= Clouds.Coverage;
    
    return saturate(base);
}

// GPU-Pro-7
float SampleDensity(vec3 x0, int lod)
{
    vec3 uv =  GetUV(x0, 65.0, 0.01);
    float height = 1.0 - GetHeightFraction(x0);
    float base = textureLod(CloudLowFrequency, uv, lod).r * height;

    base = remap(base, 1.0 - Clouds.Coverage, 1.0, 0.0, 1.0);
    base *= Clouds.Coverage;

    uv =  GetUV(x0, 800.0, 0.2);
    height = 1.0 - GetHeightFraction(x0);

    vec4 high_frequency_noise = texture(CloudHighFrequency, uv);
    float high_frequency_fbm = high_frequency_noise.r * 0.625 + high_frequency_noise.g * 0.25 + high_frequency_noise.b * 0.125;
    float high_frequency_modifier = mix(high_frequency_fbm, 1.0 - high_frequency_fbm, saturate(height * 65.0));

    base = remap(base, high_frequency_modifier * 0.5, 1.0, 0.0, 1.0);
    return saturate(base);
}

// volumetric-cloudscapes-of-horizon-zero-dawn
float MarchToLight(vec3 rs, vec3 rd, float stepsize)
{
    vec3 pos = rs;
    float density = 0.0;
    float radius = 1.0;
    float transmittance = 1.0;

    stepsize *= 6.f;
    for (int i = 0; i <= 6; i++)
    {
        pos = rs + float(i) * radius * light_kernel[i];

        float sampled_density = 0.0;

        if (density < 0.3)
        {
            sampled_density = SampleDensity(pos, 1);
        }
        else
        {
            sampled_density = SampleCloudShape(pos, 1);
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

void FillSky()
{
    SAtmosphere Atmosphere;
    float t = SphereDistance(ubo.CameraPosition.xyz, marchDirection, vec3(0.0), Rt, false);
    AtmosphereAtPoint_2(TransmittanceLUT, InscatteringLUT, ubo.CameraPosition.xyz, t, marchDirection, ubo.SunDirection.xyz, Atmosphere, Atmosphere);
    outScattering.rgb = outScattering.rgb + outScattering.a * (Atmosphere.S + GetSunColor(ubo.CameraPosition.xyz, marchDirection, ubo.SunDirection.xyz) * Atmosphere.L);

    gl_FragDepth = 0.0;
}

// I doubt it's physically correct but looks nice
// Unoptimized and expensive
void MarchToCloud()
{
    const int steps = 512;
    const float len = distance(marchStart, marchEnd);
    const float stepsize = len / float(steps);

    mat3 R;
    R[1] = normalize(marchStart);
    R[0] = vec3(1.0, 0.0, 0.0);
    R[2] = normalize(cross(R[0], R[1]));
    vec3 rl = normalize(transpose(R) * ubo.SunDirection.xyz);
    
    vec3 pos = marchStart;
    float phase = HGDPhase(dot(rl, marchDirection), 0.75, -0.15, 0.5, MieG);
    //float phase = DualLobeFunction(dot(rl, marchDirection), MieG, -0.25, 0.65);

    // skip empty space until cloud is found
    int i = 1;
    float sample_density = 0.0;
    vec3 cloudStart = vec3(0.0);

    for (; i < steps && sample_density == 0.0; i += 1)
    {
        sample_density = SampleCloudShape(pos, 4);
        pos = marchStart + stepsize * marchDirection * i;
        cloudStart = pos;
    }
    i = max(i - 1, 1);

    float mean_depth = 0.0;
    float mean_transmittance = 0.0;
    float aep_distance = distance(ubo.CameraPosition.xyz, marchEnd);
    float march_to_camera = distance(ubo.CameraPosition.xyz, marchStart);

    for (; i < steps; ++i)
    {
        pos = marchStart + i * stepsize * marchDirection;
        sample_density = SampleDensity(pos, 0);

        if (sample_density > 0.0) 
        {
            float extinction = Clouds.Absorption * sample_density;
            float transmittance = BeerLambert(extinction, stepsize);
            float luminance = phase * MarchToLight(pos, rl, stepsize);
            
            vec3 E = extinction * vec3(luminance);
            E = vec3(E - transmittance * E) / max(extinction, 1e-6);

            outScattering.rgb += E * outScattering.a;
            outScattering.a *= transmittance;

            mean_depth += ((float(i) * stepsize + march_to_camera) / aep_distance) * outScattering.a;
            mean_transmittance += outScattering.a;
        }
    }

    // aerial perspective
    if (outScattering.a != 1.0)
    {
        SAtmosphere Atmosphere_Sky;
        SAtmosphere Atmosphere_Cloud;

        float T = (mean_depth / mean_transmittance);

        AtmosphereAtPoint(TransmittanceLUT, InscatteringLUT, marchStart, len, marchDirection, ubo.SunDirection.xyz, Atmosphere_Cloud);
        vec3 L = T * (Atmosphere_Cloud.L - Atmosphere_Cloud.L * outScattering.a);
        outScattering.rgb = outScattering.rgb * L;

        float t = SphereDistance(ubo.CameraPosition.xyz, marchDirection, vec3(0.0), Rt, false);
        AtmosphereAtPoint_2(TransmittanceLUT, InscatteringLUT, ubo.CameraPosition.xyz, t, marchDirection, ubo.SunDirection.xyz, Atmosphere_Cloud, Atmosphere_Sky);
        vec3 S = T * (Atmosphere_Cloud.S - Atmosphere_Cloud.S * outScattering.a);
        outScattering.rgb = outScattering.rgb + S;

        S = (Atmosphere_Sky.S + GetSunColor(ubo.CameraPosition.xyz, marchDirection, ubo.SunDirection.xyz) * Atmosphere_Sky.L) * outScattering.a;
        outScattering.rgb = outScattering.rgb + S;

        vec4 ndc = vec4(ubo.ViewProjectionMatrix * vec4(cloudStart, 1.0));
        ndc.xyz /= ndc.w;
        gl_FragDepth = ndc.z;
    }
    else
    {
        FillSky();
    }
}

void main()
{
    vec4 ScreenNDC = vec4(2.0 * ScreenUV - 1.0, 1.0, 1.0);
    vec4 ScreenView = ubo.ProjectionMatrixInverse * ScreenNDC;
    vec4 ScreenWorld = vec4(ubo.ViewMatrixInverse * vec4(ScreenView.xy, -1.0, 0.0));
    vec3 RayOrigin = vec3(ubo.CameraPositionFP64.xyz);
    vec3 SphereCenter = vec3(0.0, 0.0, 0.0);

    outScattering = vec4(0.0, 0.0, 0.0, 1.0);
    marchDirection = normalize(ScreenWorld.xyz);

    topBound = Rcb + Rcdelta * max(Clouds.VerticalSpan, 0.0075);
    float ground = SphereDistance(RayOrigin, marchDirection, SphereCenter, Rg, false);
    distanceCamera = length(RayOrigin);

    if (distanceCamera < Rcb && ground == 0.0)
    {
        distanceStart = SphereDistance(RayOrigin, marchDirection, SphereCenter, Rcb, false);
        distanceEnd = SphereDistance(RayOrigin, marchDirection, SphereCenter, topBound, false);

        marchStart = RayOrigin + marchDirection * distanceStart;
        marchEnd = RayOrigin + marchDirection * distanceEnd;
    }
    else if (distanceCamera > Rcb && distanceCamera < topBound)
    {
        float t1 = SphereDistance(RayOrigin, marchDirection, SphereCenter, Rcb, false);;
        float t2 = SphereDistance(RayOrigin, marchDirection, SphereCenter, topBound, false);

        marchStart = RayOrigin;

        if (t1 == 0.0)
        {
            marchEnd = RayOrigin + marchDirection * t2;
        }
        else if (t2 == 0.0)
        {
            marchEnd = RayOrigin + marchDirection * t1;
        }
        else
        {
            marchEnd = RayOrigin + marchDirection * min(t1, t2);
        }
    }
    else if (distanceCamera > topBound)
    {
        distanceStart = SphereDistance(RayOrigin, marchDirection, SphereCenter, topBound, false);
        distanceEnd = SphereDistance(RayOrigin, marchDirection, SphereCenter, Rcb, false);
        if (distanceEnd == 0.0)
        {
            distanceEnd = SphereDistance(RayOrigin, marchDirection, SphereCenter, topBound, true);
        }

        marchStart = RayOrigin + marchDirection * distanceStart;
        marchEnd = RayOrigin + marchDirection * distanceEnd;  
    }
    else
    {
        FillSky();

        return;
    }

    distanceStart = length(marchStart);
    distanceEnd = length(marchEnd);

    MarchToCloud();
}