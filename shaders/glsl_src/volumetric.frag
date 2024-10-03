#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "noise.glsl"

vec3 sphereStart;
vec3 sphereEnd;
float outSphereRSq;
float inSphereRSq;

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
layout(location=0) out vec4 outColor;

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
layout(set = 1, binding = 6) uniform sampler2D IrradianceLUT;
layout(set = 1, binding = 7) uniform sampler3D InscatteringLUT;

float GetHeightFraction(vec3 p)
{
    p = p - sphereStart;
    return 1.0 - saturate(sqrt(dot(p, p) / inSphereRSq));
    //return 1.0 - saturate(dot(p, p) / dot(sphereEnd, sphereEnd));
}

vec3 GetUV(vec3 p, float scale, float speed_mod)
{
    return scale * p / Rct + vec2(ubo.Time * Clouds.WindSpeed * speed_mod, 0.0).xyx;
}

float SampleCloudShape(vec3 x0, int lod)
{
    float weather = remap(texture(WeatherMap, GetUV(x0, 50.0, 0.05).xz).r, 0.0, 1.0, 0.0, Clouds.Coverage);
    float height = GetHeightFraction(x0);
    vec3 uv =  GetUV(x0, 45.0, 0.01);

    vec4 low_frequency_noise = textureLod(CloudLowFrequency, uv, lod);
    float base = low_frequency_noise.r;

    base = remap(base, 1.0 - Clouds.Coverage, 1.0, 0.0, 1.0);

    base *= saturate(remap(height, mix(0.95, 0.5, Clouds.VerticalSpan), mix(1.0, 0.6, weather * Clouds.VerticalSpan), 0.0, 1.0));
    base *= Clouds.Coverage;

    return base;
}

// GPU-Pro-7
float SampleDensity(vec3 x0, int lod)
{
    float height = GetHeightFraction(x0);
    vec3 uv =  GetUV(x0, 750.0, 0.3);

    float base = SampleCloudShape(x0, lod);

    //return base;

    vec4 high_frequency_noise = texture(CloudHighFrequency, uv);
    float high_frequency_fbm = high_frequency_noise.r * 0.625 + high_frequency_noise.g * 0.25 + high_frequency_noise.b * 0.125;
    float high_frequency_modifier = mix(high_frequency_fbm, 1.0 - high_frequency_fbm, saturate(height * 45.0));

    return remap(base, high_frequency_modifier * 0.1, 1.0, 0.0, 1.0) * height;
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
            float transmittance_light = BeerLambert(stepsize * sampled_density * Clouds.Absorption);
            transmittance *= transmittance_light;
        }
        
        density += sampled_density;
        rs += stepsize * rd;
        radius += 1.f / 6.f;
    }

    return transmittance;
}

// I doubt it's physically correct but looks nice
// Unoptimized and expensive
vec4 MarchToCloud(vec3 rs, vec3 re, vec3 rd)
{
    const int steps = int(mix(256, 64, abs(dot(rd, vec3(0.0, 1.0, 0.0)))));

    const float len = distance(rs, re);
    const float stepsize = len / float(steps);

    vec4 scattering = vec4(0.0, 0.0, 0.0, 1.0);
    vec3 pos = rs;
    float sample_density = 0.0;

    mat3 R;
    R[1] = normalize(rs);
    R[0] = vec3(1.0, 0.0, 0.0);
    R[2] = normalize(cross(R[0], R[1]));
    vec3 rl = normalize(transpose(R) * ubo.SunDirection.xyz);

    float phase = HGDPhase(dot(rl, rd), 0.75, -0.15, 0.5, MieG);
    //float phase = DualLobeFunction(dot(rl, rd), MieG, -0.25, 0.65);

    //return vec4(vec3(10.0 * vec3(smoothstep(0.25, 1.0, phase))), 0.0);

    // skip empty space until cloud is found
    int i = 0;
    for (; i < steps && sample_density == 0.0; i += 10)
    {
        sample_density = SampleDensity(pos, 4);
        pos = rs + stepsize * rd * i;
    }

    float meanD = 0.0;
    float meanT = 0.0;

    rs = pos;
    for (; i < steps; ++i)
    {
        sample_density = SampleDensity(pos, 0);

        if (sample_density > 0.0) 
        {
            float extinction = sample_density * Clouds.Absorption;
            float transmittance = BeerLambert(stepsize * extinction);

            float Vis = MarchToLight(pos, rl, stepsize); // * Powder(stepsize, sample_density, Clouds.Absorption)

            //vec3 E = sample_density * (Atmosphere.L * phase * Vis + Atmosphere.S);
            vec3 E = vec3(sample_density * phase * Vis);
            vec3 inScat = (E - E * transmittance) / sample_density;

            scattering.rgb += inScat * scattering.a;
            scattering.a *= transmittance;

            meanD += (length(pos) / length(re)) * transmittance;
            meanT += transmittance;
        }

        pos += stepsize * rd;
    }

    // aerial perspective
    if (scattering.a != 1.0)
    {
        SAtmosphere Atmosphere;
        AtmosphereAtPoint(TransmittanceLUT, InscatteringLUT, rs, distance(rs, re), normalize(re - rs), ubo.SunDirection.xyz, Atmosphere);
        float CloudTr = (meanD / meanT);
        vec3 E = CloudTr * Atmosphere.L + Atmosphere.S;
        scattering.rgb = scattering.rgb * E;
    
        AtmosphereAtPoint(TransmittanceLUT, InscatteringLUT, ubo.CameraPosition.xyz, distance(ubo.CameraPosition.xyz, rs), normalize(rs - ubo.CameraPosition.xyz), ubo.SunDirection.xyz, Atmosphere);
        scattering.rgb = scattering.rgb + Atmosphere.S * (1.0 - scattering.a);
    }

    return scattering;
}

void main()
{
    vec4 ScreenNDC = vec4(2.0 * ScreenUV - 1.0, 1.0, 1.0);
    vec4 ScreenView = ubo.ProjectionMatrixInverse * ScreenNDC;
    vec4 ScreenWorld = vec4(ubo.ViewMatrixInverse * vec4(ScreenView.xy, -1.0, 0.0));
    vec3 RayDirection = normalize(ScreenWorld.xyz);
    vec3 RayOrigin = vec3(ubo.CameraPositionFP64.xyz);
    vec3 SphereCenter = vec3(0.0, 0.0, 0.0);

    if (RaySphereintersection(RayOrigin, RayDirection, SphereCenter, Rg, sphereStart))
    {
        discard;
    }

    if (RaySphereintersection(RayOrigin, RayDirection, SphereCenter, Rcb, sphereStart)
    && RaySphereintersection(RayOrigin, RayDirection, SphereCenter, Rct, sphereEnd)
    )
    {
        outSphereRSq = dot(sphereEnd, sphereEnd);
        inSphereRSq = dot(sphereEnd - sphereStart, sphereEnd - sphereStart);
        outColor = MarchToCloud(sphereStart, sphereEnd, RayDirection);
    }
    else 
    {
        discard;
    }

    gl_FragDepth = 0.0;
}