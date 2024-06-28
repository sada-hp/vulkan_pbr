#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "noise.glsl"

vec3 sphereStart;
vec3 sphereEnd;
float sphereRSq;

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
layout(set = 1, binding = 4) uniform sampler2D TransmittanceLUT;
layout(set = 1, binding = 5) uniform sampler2D IrradianceLUT;
layout(set = 1, binding = 6) uniform sampler3D InscatteringLUT;

// precomputed-atmospheric-scattering
void AtmosphereAtPoint(vec3 x, float t, vec3 v, vec3 s, out SAtmosphere Atmosphere) 
{
    vec3 result;
    float r = length(x);
    float mu = dot(x, v) / r;
    float d = -r * mu - sqrt(r * r * (mu * mu - 1.0) + Rt * Rt);
    
    if (d > 0.0) 
    {
        x += d * v;
        t -= d;
        mu = (r * mu + d) / Rt;
        r = Rt;
    }

    if (r <= Rt)
    {
        float nu = dot(v, s);
        float muS = dot(x, s) / r;
        float phaseR = RayleighPhase(nu);
        float phaseM = HenyeyGreensteinPhase(nu, MieG);
        vec4 inscatter = max(GetInscattering(InscatteringLUT, r, mu, muS, nu), 0.0);

        if (t > 0.0) 
        {
            vec3 x0 = x + t * v;
            float r0 = length(x0);
            float rMu0 = dot(x0, v);
            float mu0 = rMu0 / r0;
            float muS0 = dot(x0, s) / r0;
            Atmosphere.T = GetTransmittance(TransmittanceLUT, r, mu, v, x0);
            Atmosphere.L = GetTransmittanceWithShadow(TransmittanceLUT, r, muS) * MaxLightIntensity;

            if (r0 > Rg + 0.01) 
            {
                inscatter = max(inscatter - Atmosphere.T.rgbr * GetInscattering(InscatteringLUT, r0, mu0, muS0, nu), 0.0);
                
                const float EPS = 0.004;
                float muHoriz = -sqrt(1.0 - (Rg / r) * (Rg / r));
                if (abs(mu - muHoriz) < EPS) 
                {
                    float a = ((mu - muHoriz) + EPS) / (2.0 * EPS);

                    mu = muHoriz - EPS;
                    r0 = sqrt(r * r + t * t + 2.0 * r * t * mu);
                    mu0 = (r * mu + t) / r0;
                    vec4 inScatter0 = GetInscattering(InscatteringLUT, r, mu, muS, nu);
                    vec4 inScatter1 = GetInscattering(InscatteringLUT, r0, mu0, muS0, nu);
                    vec4 inScatterA = max(inScatter0 - Atmosphere.T.rgbr * inScatter1, 0.0);

                    mu = muHoriz + EPS;
                    r0 = sqrt(r * r + t * t + 2.0 * r * t * mu);
                    mu0 = (r * mu + t) / r0;
                    inScatter0 = GetInscattering(InscatteringLUT, r, mu, muS, nu);
                    inScatter1 = GetInscattering(InscatteringLUT, r0, mu0, muS0, nu);
                    vec4 inScatterB = max(inScatter0 - Atmosphere.T.rgbr * inScatter1, 0.0);

                    inscatter = mix(inScatterA, inScatterB, a);
                }
            }
        }
        inscatter.w *= smoothstep(0.00, 0.02, muS);

        result = max(inscatter.rgb * phaseR + GetMie(inscatter) * phaseM, 0.0);
    } 
    else 
    {
        Atmosphere.S = vec3(0.0);
    }

    Atmosphere.S = result * MaxLightIntensity;
}

float GetHeightFraction(vec3 p)
{
    return 1.0 - saturate(dot(p, p) / sphereRSq);
}

vec3 GetUV(vec3 p, float scale, float speed_mod)
{
    return scale * p / Rg + vec2(ubo.Time * Clouds.WindSpeed * speed_mod, 0.0).xyx;
}

float SampleCloudShape(vec3 x0, int lod)
{
    float height = GetHeightFraction(x0);
    vec3 uv =  GetUV(x0, 45.0, 0.01);

    vec4 low_frequency_noise = textureLod(CloudLowFrequency, uv, lod);
    float base = low_frequency_noise.r;
    base = remap(base, 1.0 - Clouds.Coverage, 1.0, 0.0, 1.0);

    base *= saturate(remap(height, mix(0.8, 0.25, Clouds.VerticalSpan), mix(0.95, 0.4, Clouds.VerticalSpan), 0.0, 1.0));
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

// I doubt it's physically correct but looks nice
// Unoptimized and expensive
vec4 MarchToCloud(vec3 rs, vec3 re, vec3 rd)
{
    // need more precision near horizon, worst case is very expensive
    const int steps = int(mix(256, 64, abs(dot(rd, vec3(0.0, 1.0, 0.0)))));

    const float len = distance(rs, re);
    const float stepsize = len / float(steps);

    vec4 scattering = vec4(0.0, 0.0, 0.0, 1.0);
    vec3 pos = rs;
    float sample_density = 0.0;
    vec3 rl = normalize(ubo.SunDirection.xyz);
    float phase = HGDPhase(dot(rl, rd), 0.75, -0.15, 0.5, MieG);
    //float phase = DualLobeFunction(dot(rl, rd), MieG, -0.25, 0.65);

    // skip empty space until cloud is found
    int i = 0;
    for (; i < steps && sample_density == 0.0; i += 10)
    {
        sample_density = SampleDensity(pos, 4);
        pos = rs + stepsize * rd * i;
    }

    rs = pos;
    SAtmosphere Atmosphere;
    for (; i < steps; ++i)
    {
        sample_density = SampleDensity(pos, 0);

        if (sample_density > 0.0) 
        {
            float extinction = sample_density * Clouds.Absorption;
            float transmittance = BeerLambert(stepsize * extinction);

            // get scattering along the step ray for this sample, expensive
            AtmosphereAtPoint(vec3(0, Rg, 0) + pos, stepsize, rd, rl, Atmosphere);
            float Vis = MarchToLight(pos, rl, stepsize) * Powder(stepsize, sample_density, Clouds.Absorption);

            vec3 E = sample_density * (Atmosphere.L * phase * Vis + Atmosphere.S);
            vec3 inScat = (E - E * transmittance) / sample_density;

            scattering.rgb += inScat * scattering.a;
            scattering.a *= transmittance;
        }

        pos += stepsize * rd;
    }

    if (scattering.a != 1.0)
    {
        AtmosphereAtPoint(vec3(0, Rg, 0) + ubo.CameraPosition.xyz, distance(ubo.CameraPosition.xyz, rs), rd, rl, Atmosphere);
        // aerial perspective
        scattering.rgb += (Atmosphere.S + phase * Atmosphere.L * scattering.a) * (1.0 - scattering.a);
    }

    return scattering;
}

void main()
{
    vec2 ScreenUV = gl_FragCoord.xy / ubo.Resolution;
    vec4 ScreenNDC = vec4(2.0 * ScreenUV - 1.0, 1.0, 1.0);
    vec4 ScreenView = inverse(ubo.ProjectionMatrix) * ScreenNDC;
    vec4 ScreenWorld = inverse(ubo.ViewMatrix) * vec4(ScreenView.xy, -1.0, 0.0);
    vec3 RayDirection = normalize(ScreenWorld.xyz);

    if (dot(RayDirection, vec3(0.0, 1.0, 0.0)) < 0.0) 
    {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        discard;
    }

    vec3 RayOrigin = ubo.CameraPosition.xyz;
    vec3 SphereCenter = vec3(0.0, -Rg, 0.0);
    if (RaySphereintersection(RayOrigin, RayDirection, SphereCenter, Rcb, sphereStart)
    && RaySphereintersection(RayOrigin, RayDirection, SphereCenter, Rct, sphereEnd)) 
    {
        sphereRSq = dot(sphereEnd, sphereEnd);
        outColor = MarchToCloud(sphereStart, sphereEnd, RayDirection);
    }
    else 
    {
        discard;
    }

    gl_FragDepth = 1.0;
}