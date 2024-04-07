#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "noise.glsl"
#include "common.glsl"

vec3 sphereStart;
vec3 sphereEnd;

layout(location=0) out vec4 outColor;

layout(binding = 2) uniform CloudLayer
{
    float Coverage;
    float Absorption;
    float PhaseCoefficient;
    float WindSpeed;
} Clouds;

layout(binding = 3) uniform sampler3D CloudLowFrequency;
layout(binding = 4) uniform sampler3D CloudHighFrequency;
layout(binding = 5) uniform sampler2D GradientTexture;

float GetHeightFraction(vec3 p)
{
    //return saturate((p.y - sphereStart.y) / (sphereEnd.y - sphereStart.y));
    return 1.0 - saturate(dot(p, p) / dot(sphereEnd, sphereEnd));
}

vec3 GetUV(vec3 p, float scale)
{
    return scale * p / ATMOSPHERE_START + vec2(ubo.Time * Clouds.WindSpeed * 0.01, 0.0).xyx;
}

float SampleCloudShape(vec3 x0)
{
    float height = GetHeightFraction(x0);
    vec3 uv =  GetUV(x0, 4.0);

    vec4 low_frequency_noise = texture(CloudLowFrequency, uv);
    float low_frequency_fbm = low_frequency_noise.g * 0.625 + low_frequency_noise.b * 0.25 + low_frequency_noise.a * 0.125;
    float base = remap(low_frequency_noise.r, -(1.0 - low_frequency_fbm), 1.0, 0.0, 1.0);

    //base *= remap(height, 0.0, 0.1, 0.0, 1.0) * remap(height, 0.2, 0.3, 1.0, 0.0);
   
    base = remap(base, 1.0 - Clouds.Coverage, 1.0, 0.0, 1.0);
    base *= texture(GradientTexture, vec2(0.0, height)).r;
    base *= Clouds.Coverage;

    return base;
}

//GPU Pro 7
float SampleDensity(vec3 x0)
{
    float height = GetHeightFraction(x0);
    vec3 uv =  GetUV(x0, 60.0);

    float base = SampleCloudShape(x0);

    //return base;

    vec4 high_frequency_noise = texture(CloudHighFrequency, uv);
    float high_frequency_fbm = high_frequency_noise.r * 0.625 + high_frequency_noise.g * 0.25 + high_frequency_noise.b * 0.125;
    float high_frequency_modifier = mix(high_frequency_fbm, 1.0 - high_frequency_fbm, saturate(height * 25.0));

    return remap(base, high_frequency_modifier * 0.1, 1.0, 0.0, 1.0);
}

vec3 light_kernel[] =
{
    vec3(0.5, 0.5, 0.5),
    vec3(-0.628, 0.64, 0.234),
    vec3(0.23, -0.123, 0.75),
    vec3(0.98, 0.45, -0.32),
    vec3(-0.1, -0.54, 0.945),
    vec3(0.0, 0.6, -0.78),
};

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

        float sampled_density = SampleDensity(pos);

        if (sampled_density > 0.0)
        {
            float transmittance_light = BeerLambert(stepsize * sampled_density * Clouds.Absorption);
            transmittance *= transmittance_light;
        }
        
        density += sampled_density;
        rs += stepsize * rd;
        radius += 1.f / 6.f;
    }

    return transmittance;
}

vec4 MarchToCloud(vec3 rs, vec3 re, vec3 rd)
{
    const int steps = 512;
    const float len = distance(rs, re);
    const float stepsize = len / float(steps);

    vec4 scattering = vec4(0.0, 0.0, 0.0, 1.0);
    vec3 pos = rs;
    float density = 0.0;
    vec3 rl = normalize(ubo.SunDirection);
    float phase = DualLobeFunction(dot(rl, rd), Clouds.PhaseCoefficient, -0.05, 0.25);
    //float phase = HGDPhase(dot(rl, rd), mix(5.0, 50.0, Clouds.PhaseCoefficient));

    for (int i = 0; i < steps; i++) 
    {
        density = SampleDensity(pos);

        float extinction = max(density * Clouds.Absorption, 1e-8);

        if (density > 0.0) 
        {
            float transmittance = BeerLambert(stepsize * extinction);

            vec3 L = vec3(0.99, 0.9, 1.0) * density * 5.0;
            vec3 energy = L * phase * Powder(stepsize, density, Clouds.Absorption) * MarchToLight(pos, rl, stepsize); //phase * Powder(stepsize * extinction) * MarchToLight(pos, rl, stepsize)
            vec3 intScat = (energy - energy * transmittance) / density;

            scattering.rgb += intScat * scattering.a;
            scattering.a *= transmittance;
        }

        pos += stepsize * rd;
    }

    //scattering.rgb = pow(scattering.rgb, vec3(1.0 / 2.2));

    return scattering;
}

void main()
{
    vec2 ScreenUV = gl_FragCoord.xy / View.Resolution;
    vec4 ScreenNDC = vec4(2.0 * ScreenUV - 1.0, 1.0, 1.0);
    vec4 ScreenView = inverse(ubo.ProjectionMatrix) * ScreenNDC;
    vec4 ScreenWorld = inverse(ubo.ViewMatrix) * vec4(ScreenView.xy, -1.0, 0.0);
    vec3 RayDirection = normalize(ScreenWorld.xyz);

    if (dot(RayDirection, vec3(0.0, 1.0, 0.0)) < 0.0) {
        outColor = vec4(0.0);
        return;
    }

    if (raySphereintersection(View.CameraPosition.xyz, RayDirection, ATMOSPHERE_START, sphereStart)
    && raySphereintersection(View.CameraPosition.xyz, RayDirection, ATMOSPHERE_END, sphereEnd)) {
        outColor = MarchToCloud(sphereStart, sphereEnd, RayDirection);
    }
    else {
        outColor = vec4(0.0);
    }

    gl_FragDepth = 1.0;
}