#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "noise.glsl"

vec3 sphereStart;
vec3 sphereEnd;

layout(location=0) out vec4 outColor;

layout(binding = 2) uniform CloudLayer
{
    float Coverage;
    float VerticalSpan;
    float Absorption;
    float PhaseCoefficient;
    float WindSpeed;
} Clouds;

layout(binding = 3) uniform sampler3D CloudLowFrequency;
layout(binding = 4) uniform sampler3D CloudHighFrequency;
layout(binding = 5) uniform sampler2D TransmittanceLUT;
layout(binding = 6) uniform sampler2D IrradianceLUT;
layout(binding = 7) uniform sampler3D InscatteringLUT;

float GetHeightFraction(vec3 p)
{
    //return saturate((p.y - sphereStart.y) / (sphereEnd.y - sphereStart.y));
    return 1.0 - saturate(dot(p, p) / dot(sphereEnd, sphereEnd));
}

vec3 GetUV(vec3 p, float scale)
{
    return scale * p / ATMOSPHERE_START + vec2(ubo.Time * Clouds.WindSpeed * 0.01, 0.0).xyx;
}

float SampleCloudShape(vec3 x0, int lod)
{
    float height = GetHeightFraction(x0);
    vec3 uv =  GetUV(x0, 4.0);

    vec4 low_frequency_noise = textureLod(CloudLowFrequency, uv, lod);
    float base = low_frequency_noise.r;
    base = remap(base, 1.0 - Clouds.Coverage, 1.0, 0.0, 1.0);

    base *= saturate(remap(height, 0.1, 0.2, 0.0, 1.0));
    base *= saturate(remap(height, mix(0.8, 0.5, Clouds.VerticalSpan), mix(0.9, 0.6, Clouds.VerticalSpan), 0.0, 1.0));
    base *= Clouds.Coverage;

    return base;
}

//GPU Pro 7
float SampleDensity(vec3 x0, int lod)
{
    float height = GetHeightFraction(x0);
    vec3 uv =  GetUV(x0, 60.0);

    float base = SampleCloudShape(x0, lod);

    //return base;

    vec4 high_frequency_noise = texture(CloudHighFrequency, uv);
    float high_frequency_fbm = high_frequency_noise.r * 0.625 + high_frequency_noise.g * 0.25 + high_frequency_noise.b * 0.125;
    float high_frequency_modifier = mix(high_frequency_fbm, 1.0 - high_frequency_fbm, saturate(height * 45.0));

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

        float sampled_density = SampleDensity(pos, 2);

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
    const int steps = 128;
    const float len = distance(rs, re);
    const float stepsize = len / float(steps);

    vec4 scattering = vec4(0.0, 0.0, 0.0, 1.0);
    vec3 pos = rs;
    float density = 0.0;
    vec3 rl = ubo.SunDirection;
    //float phase = DualLobeFunction(dot(rl, rd), Clouds.PhaseCoefficient, -0.25, 0.55);
    float phase = HGDPhase(dot(rl, rd), 0.75, -0.25, 0.5, Clouds.PhaseCoefficient);

    int i = 0;
    while (density == 0.0 && i < steps)
    {
        density = SampleDensity(pos, 2);
        pos += stepsize * rd * 10.0;
        i += 10;
    }

    //super hacky, but somehow it works, gonna be changed
    //vec3 L, T, S, E;
    //PointRadiance(TransmittanceLUT, IrradianceLUT, InscatteringLUT, rl, rd, vec3(0, Rg, 0) + View.CameraPosition.xyz, vec3(0, Rg, 0) + View.CameraPosition.xyz, L, T, E, S);

    for (; i < steps; i++)
    {
        density = SampleDensity(pos, 0);

        if (density > 0.0) 
        {
            float extinction = max(density * Clouds.Absorption, 1e-8);
            float transmittance = BeerLambert(stepsize * extinction);

            vec3 energy = vec3(density * phase * (MarchToLight(pos, rl, stepsize)) * Powder(stepsize, density, Clouds.Absorption));
            vec3 intScat = (energy - energy * transmittance) / density;

            scattering.rgb += intScat * scattering.a;
            scattering.a *= transmittance;
        }

        pos += stepsize * rd;
    }

    return scattering;
}

void main()
{
    vec2 ScreenUV = gl_FragCoord.xy / View.Resolution;
    vec4 ScreenNDC = vec4(2.0 * ScreenUV - 1.0, 1.0, 1.0);
    vec4 ScreenView = inverse(ubo.ProjectionMatrix) * ScreenNDC;
    vec4 ScreenWorld = inverse(ubo.ViewMatrix) * vec4(ScreenView.xy, -1.0, 0.0);
    vec3 RayDirection = normalize(ScreenWorld.xyz);

    if (dot(RayDirection, vec3(0.0, 1.0, 0.0)) < 0.0) 
    {
        outColor = vec4(0.0);
        discard;
    }

    vec3 RayOrigin = View.CameraPosition.xyz;
    vec3 SphereCenter = vec3(View.CameraPosition.x, -EARTH_RADIUS, View.CameraPosition.z);
    if (RaySphereintersection(RayOrigin, RayDirection, SphereCenter, ATMOSPHERE_START, sphereStart)
    && RaySphereintersection(RayOrigin, RayDirection, SphereCenter, ATMOSPHERE_END, sphereEnd)) 
    {
        outColor = MarchToCloud(sphereStart, sphereEnd, RayDirection);
    }
    else 
    {
        discard;
    }

    gl_FragDepth = 1.0;
}