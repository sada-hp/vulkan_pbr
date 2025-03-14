#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "noise.glsl"

vec3 marchHit;
vec3 marchEnd;
vec3 marchStart;
vec3 fromCamera;

float topBound;
float bottomBound;

// used for shadow sampling
vec3 light_kernel[] =
{
    vec3(0.57735, 0.57735, 0.57735),
    vec3(-0.677686, 0.690636, 0.252514),
    vec3(0.289651, 0.1549, 0.944515),
    vec3(0.871223, 0.400051, 0.284481),
    vec3(0.0914922, 0.494058, 0.864602),
    vec3(0.488603, 0.531976, 0.691569),
    vec3(0.260733, 0.921748, 0.287052)
};

layout(location = 0) in vec2 ScreenUV;
layout(location=0) out vec4 outScattering;

layout(set = 1, binding = 1) uniform CloudLayer
{
    float Coverage;
    float Density;
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
    float t1 = length(p) - bottomBound;
    float t2 = topBound - bottomBound;
    return 1.0 - saturate(t1 / t2);
}

vec3 GetUV(vec3 p, float scale, float speed_mod)
{
    vec3 uv = p / topBound;
    vec3 anim = vec3(ubo.Time * Clouds.WindSpeed * speed_mod);
    
    return scale * uv + anim;
}

// Cheaper version of Sample-Density
float SampleCloudShape(vec3 x0, int lod)
{
    vec3 uv =  GetUV(x0, 50.0, 0.01);
    float height = GetHeightFraction(x0);

    float base = textureLod(CloudLowFrequency, uv, lod).r;

    float h1 = saturate(height + 0.2);
    float h2 = saturate(remap(1.0 - height, 0.0, 0.2, 0.0, 1.0));

    float weather = 1.0 - texture(WeatherMap, uv.xz / 5.0).r;
    base *= weather;

    weather = saturate(texture(WeatherMap, 1.0 - uv.zx / 40.0).r + 0.2);
    h1 *= weather;
    h2 *= weather;

    base = remap(base, 1.0 - Clouds.Coverage * h1 * h2, 1.0, Clouds.Coverage, 1.0);

    return saturate(base);
}

// GPU-Pro-7
float SampleDensity(vec3 x0, int lod)
{
    vec3 uv =  GetUV(x0, 50.0, 0.01);
    float height = GetHeightFraction(x0);

    float base = textureLod(CloudLowFrequency, uv, lod).r;

    float h1 = saturate(height + 0.2);
    float h2 = saturate(remap(1.0 - height, 0.0, 0.2, 0.0, 1.0));

    float weather = 1.0 - texture(WeatherMap, uv.xz / 5.0).r;
    base *= weather;

    weather = saturate(texture(WeatherMap, 1.0 - uv.zx / 40.0).r + 0.2);
    h1 *= weather;
    h2 *= weather;

    base = remap(base, 1.0 - Clouds.Coverage * h1 * h2, 1.0, Clouds.Coverage, 1.0);

    uv =  GetUV(x0, 400.0, 0.2);

    vec4 high_frequency_noise = texture(CloudHighFrequency, uv, lod);
    float high_frequency_fbm = high_frequency_noise.r * 0.625 + high_frequency_noise.g * 0.25 + high_frequency_noise.b * 0.125;
    float high_frequency_modifier = mix(high_frequency_fbm, 1.0 - high_frequency_fbm, saturate((1.0 - height) * 5.0));

    base = remap(base, high_frequency_modifier * mix(0.15, 0.45, Clouds.Coverage), 1.0, 0.0, 1.0);
    return saturate((1.0 - height) * Clouds.Density * base);
}

float SampleDensity(vec3 x0, float base, int lod)
{
    float height = GetHeightFraction(x0);
    vec3 uv =  GetUV(x0, 400.0, 0.2);

    vec4 high_frequency_noise = texture(CloudHighFrequency, uv, lod);
    float high_frequency_fbm = high_frequency_noise.r * 0.625 + high_frequency_noise.g * 0.25 + high_frequency_noise.b * 0.125;
    float high_frequency_modifier = mix(high_frequency_fbm, 1.0 - high_frequency_fbm, saturate((1.0 - height) * 5.0));

    base = remap(base, high_frequency_modifier * mix(0.15, 0.45, Clouds.Coverage), 1.0, 0.0, 1.0);
    return saturate(height * Clouds.Density * base);
}

void Blend(float T, float A)
{
    vec3 Eye = ubo.CameraPosition.xyz;
    vec3 ViewDir = fromCamera;
    vec3 SunDir = ubo.SunDirection.xyz;
    vec3 SkyColor = vec3(0.0), CloudsColor = vec3(0.0);

    if (outScattering.a != 0.0)
    {
        SkyColor += SkyScattering(TransmittanceLUT, InscatteringLUT, Eye, ViewDir, SunDir);
        SkyColor *= MaxLightIntensity;
    }

    if (outScattering.a != 1.0)
    {
        SAtmosphere Atmosphere, Atmosphere1, Atmosphere2;
        AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, Eye, marchHit, SunDir, Atmosphere1);
        AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, Eye, marchEnd, SunDir, Atmosphere2);

        Atmosphere.L = mix(Atmosphere1.L, Atmosphere2.L, A);
        Atmosphere.T = mix(Atmosphere1.T, Atmosphere2.T, A);
        Atmosphere.S = mix(Atmosphere1.S, Atmosphere2.S, A);
        Atmosphere.Shadow = mix(Atmosphere1.Shadow, Atmosphere2.Shadow, A);

        vec3 Radiance      = T * Atmosphere.T * Atmosphere.L;
        vec3 Scattering    = mix(T, 1.0, Atmosphere.Shadow) * Atmosphere.S  * MaxLightIntensity;

        CloudsColor = Scattering + Radiance * outScattering.rgb;
    }
    
    outScattering.rgb = mix(CloudsColor, SkyColor, outScattering.a);
    // outScattering.rgb = vec3(T);
    // outScattering.rgb = vec3(A);
}

// volumetric-cloudscapes-of-horizon-zero-dawn
float SampleCone(vec3 pos, vec3 rd, float stepsize, int mip)
{
    float cone_density = 0.0;
    stepsize /= 6.f;
    for (int i = 0; i <= 6; i++)
    {
        pos += rd * stepsize;

        pos = pos + light_kernel[i] * float(i) * stepsize;

        if (cone_density < 0.3)
            cone_density += SampleDensity(pos, mip + 1);
        else
            cone_density += Clouds.Density * SampleCloudShape(pos, mip + 1);
    }

    return cone_density;
}

float MarchToLight(vec3 pos, vec3 rd, int steps, int mip)
{
    const float shadowclr = 0.2;

    if (outScattering.a < shadowclr)
        return shadowclr;

    float len = SphereMinDistance(pos, rd, vec3(0.0), topBound);
    float stepsize = min(len, topBound - bottomBound) / float(steps);

    if (stepsize == 0.0)
        return 1.0;

    float transmittance = 1.0;
    for (int i = 0; i < steps; i++)
    {
        float cone_density = SampleCone(pos, rd, stepsize, mip);

        if (cone_density > 0.0)
        {
            transmittance *= Powder(cone_density, stepsize);
        }

        stepsize *= 1.5;
    }

    return mix(shadowclr, 1.0, transmittance);
}

// Unoptimized and expensive
void MarchToCloud()
{
    vec3 pos = marchStart;
    vec3 dir = normalize(marchEnd - marchStart);
    float PhaseF = DualLobeFunction(dot(ubo.SunDirection.xyz, fromCamera), -0.25, 0.6, 0.35);
    float w = saturate(100.0 * pow(abs(dot(normalize(ubo.CameraPosition.xyz), fromCamera)), 5.0));

    float mean_depth = 0.0;
    float sample_density = 0.0;
    float mean_transmittance = 0.0;

    // take larger steps and
    // skip empty space until cloud is found
    int steps = int(ceil(mix(64, 32, w))), i = steps;
    float len = distance(marchStart, marchEnd);
    float stepsize = len / float(steps);

    for (i; i >= 0 && sample_density == 0.0;)
    {
        pos = marchStart + stepsize * dir * i;
        sample_density = SampleCloudShape(pos, 3);
        i--;
    }

    if (sample_density == 0.0)
    {
        Blend(0, 0);
        return;
    }
    else
    {
        int revert = min(i + 2, steps);
        marchEnd = marchStart + stepsize * dir * revert;
        len -= stepsize * (steps - revert);
    }

    // go more precise for clouds
    steps = int(mix(128, 64, w));
    int light_steps = int(mix(32, 4, w));
    stepsize = len / float(steps);

    const float I = MaxLightIntensity * PhaseF;

    for (i = steps; i >= 0; i--)
    {
        pos = marchStart + i * dir * stepsize;

        int mip = 4 - int(min(ceil(outScattering.a * 100.0), 4.0));
        if ((sample_density = SampleCloudShape(pos, mip)) > 0)
            sample_density = SampleDensity(pos, sample_density, mip);

        if (sample_density > 0.0) 
        {
            if (outScattering.a == 1.0)
                marchHit = pos;

            float ray_depth = saturate((length(pos) - bottomBound) / (topBound - bottomBound));
            
            float extinction = max(sample_density, 1e-6);
            float transmittance = BeerLambert(extinction, stepsize);
            float luminance = MarchToLight(pos, ubo.SunDirection.xyz, light_steps, mip) * I;

            vec3 E = extinction * vec3(luminance);
            E = vec3(E - transmittance * E) / extinction;
            outScattering.rgb += E * outScattering.a;
            outScattering.a *= transmittance;
            marchEnd = pos;

            mean_depth += ray_depth * outScattering.a;
            mean_transmittance += outScattering.a * (1.0 - ray_depth);
        }
    }
    
    float T = 0.0, A = 0.0;
    if (outScattering.a != 1.0)
    {
        T = saturate(mean_depth / mean_transmittance);
        A = smoothstep(0.0, 1.0, outScattering.a);

        vec4 clip1 = vec4(ubo.ViewProjectionMatrix * vec4(marchHit, 1.0));
        vec4 clip2 = vec4(ubo.ViewProjectionMatrix * vec4(marchEnd, 1.0));
        gl_FragDepth = max(clip1.z / clip1.w, clip2.z / clip2.w);
    }

    Blend(T, A);
}

void main()
{
    gl_FragDepth = 0.0;

    vec4 ScreenNDC = vec4(2.0 * ScreenUV - 1.0, 1.0, 1.0);
    vec4 ScreenView = ubo.ProjectionMatrixInverse * ScreenNDC;
    vec4 ScreenWorld = vec4(ubo.ViewMatrixInverse * vec4(ScreenView.xyz, 0.0));
    vec3 RayOrigin = vec3(ubo.CameraPositionFP64.xyz);
    vec3 SphereCenter = vec3(0.0, 0.0, 0.0);
    fromCamera = normalize(ScreenWorld.xyz);
    outScattering = vec4(0.0, 0.0, 0.0, 1.0);

    topBound = Rct;
    bottomBound = Rcb + Rcdelta * 0.5 * (1.0 - max(Clouds.Coverage, 0.25));

    float ground = SphereMinDistance(RayOrigin, fromCamera, SphereCenter, Rg);
    float top = SphereMinDistance(RayOrigin, fromCamera, SphereCenter, topBound);
    float bottom = SphereMinDistance(RayOrigin, fromCamera, SphereCenter, bottomBound);

    float distanceCamera = length(RayOrigin);
    float muHoriz = -sqrt(1.0 - pow(Rg / distanceCamera, 2.0));
    float horizon = dot(RayOrigin, fromCamera) / distanceCamera - muHoriz;
    
    if (Clouds.Coverage > 0 && (horizon > 0 || distanceCamera > bottomBound))
    {
        if (distanceCamera < bottomBound)
        {
            marchEnd = RayOrigin + fromCamera * bottom;
            marchStart = RayOrigin + fromCamera * top;
        }
        else if (distanceCamera > topBound)
        {
            if (ground == 0)
            {
                marchEnd = RayOrigin + fromCamera * top;
                marchStart = RayOrigin + fromCamera * SphereMaxDistance(RayOrigin, fromCamera, SphereCenter, topBound);
            }
            else
            {
                marchEnd = RayOrigin + fromCamera * top;
                marchStart = RayOrigin + fromCamera * bottom;
            }
        }
        else
        {
            if (ground == 0)
            {
                marchEnd = RayOrigin;
                marchStart = RayOrigin + fromCamera * top;
            }
            else
            {
                marchEnd = RayOrigin;
                marchStart = RayOrigin + fromCamera * bottom;
            }
        }

        MarchToCloud();
    }
    else if (ground == 0)
    {
        Blend(0, 0);
    }
}