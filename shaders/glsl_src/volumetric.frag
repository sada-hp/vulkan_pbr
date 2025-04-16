#version 460

#define USE_CLOUD_MIE

#include "ubo.glsl"
#include "lighting.glsl"
#include "noise.glsl"

float topBound;
float bottomBound;

struct RayMarch
{
    vec3 Direction;
    vec3 Position;
    vec3 Start;
    vec3 End;
    vec3 FirstHit;
    vec3 LastHit;
    float Stepsize;
    float Height;
    float T;
    float A;
};

void UpdateRay(inout RayMarch Ray)
{
    Ray.Position += Ray.Direction * Ray.Stepsize;
    Ray.Height = saturate((length(Ray.Position) - bottomBound) / (topBound - bottomBound));
}

void UpdateRay(inout RayMarch Ray, float Stepsize)
{
    Ray.Position += Ray.Direction * Stepsize;
    Ray.Height = saturate((length(Ray.Position) - bottomBound) / (topBound - bottomBound));
}

void UpdateRay(inout RayMarch Ray, float Stepsize, float f)
{
    Ray.Stepsize = min(smoothstep(0.0, 2.0, f) * 4.0 * Stepsize, Stepsize);
    Ray.Position += Ray.Direction * Ray.Stepsize;
    Ray.Height = saturate((length(Ray.Position) - bottomBound) / (topBound - bottomBound));
}

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

struct
{
    float Camera;
    float TopCloud;
    float BottomCloud;
    float Ground;
} Hits;

layout(location = 0) in vec2 ScreenUV;
layout(location=0) out vec4 outScattering;

layout(set = 1, binding = 0) uniform CloudLayer
{
    float Coverage;
    float Density;
    float WindSpeed;
} Clouds;

layout(set = 1, binding = 1) uniform sampler3D CloudLowFrequency;
layout(set = 1, binding = 2) uniform sampler3D CloudHighFrequency;
layout(set = 1, binding = 3) uniform sampler2D WeatherMap;
layout(set = 1, binding = 4) uniform sampler2D TransmittanceLUT;
layout(set = 1, binding = 5) uniform sampler2D IrradianceLUT;
layout(set = 1, binding = 6) uniform sampler3D InscatteringLUT;

layout(set = 2, binding = 0) uniform sampler2D PreviousFrame;
layout(set = 2, binding = 1) uniform UnfiormBufferOld
{
    dmat4 ViewProjectionMatrix;
    dmat4 ViewMatrix;
    dmat4 ViewMatrixInverse;
    dvec4 CameraPositionFP64;
    mat4 ProjectionMatrix;
    mat4 ProjectionMatrixInverse;
    vec4 CameraPosition;
    vec4 SunDirection;
    vec4 WorldUp;
    vec4 CameraUp;
    vec4 CameraRight;
    vec4 CameraForward;
    vec2 Resolution;
    double CameraRadius;
    float Time;
} ubo_old;

vec3 GetUV(vec3 p, float scale, float speed_mod)
{
    vec3 uv = p / topBound;
    vec3 anim = vec3(ubo.Time * speed_mod);
    return scale * uv + anim;
}

void GetUV(in RayMarch Ray, out vec3 UVA1, out vec3 UVA2)
{
    vec3 n = normalize(Ray.Position);
    float pole = abs(dot(n, vec3(0, 1, 0)));
    UVA1.xy = 0.5 + vec2(atan(n.z, n.x) * ONE_OVER_2PI, asin(n.y) * ONE_OVER_2PI);
    UVA2.xy = 0.5 + vec2(atan(n.x, n.y) * ONE_OVER_2PI, asin(n.z) * ONE_OVER_2PI);

    UVA1.z = pole > 0.9 ? 0.0 : (pole >= 0.8 ? 1.0 - ((pole - 0.8) / (0.9 - 0.8)) : 1.0);
    UVA2.z = 1.0 - UVA1.z;
}

// Cheaper version of Sample-Density
float SampleCloudShape(in RayMarch Ray, int lod)
{
    vec3 uv = GetUV(Ray.Position, 50.0, Clouds.WindSpeed * 0.01);
    float height = 1.0 - Ray.Height;

    float base = textureLod(CloudLowFrequency, uv, lod).r;

    float h1 = pow(height, 0.65);
    float h2 = saturate(remap(1.0 - height, 0.0, mix(0.05, 0.2, Clouds.Coverage), 0.0, 1.0));

    vec3 temp = texture(WeatherMap, uv.xz / 5.0).rgb;
    float weather1 = 1.0 - saturate(temp.x + 0.2) * saturate(0.5 + temp.y);
    float weather2 = saturate(texture(WeatherMap, uv.zx / 40.0).r + 0.2);
    
    base *= weather1;
    float shape = 1.0 - Clouds.Coverage * h1 * h2 * weather2;
    base = height * saturate(remap(base, shape, 1.0, pow(Clouds.Coverage, 1.5), 1.0));

    return base;
}

float SampleCloudDetail(in RayMarch Ray, float base, int lod)
{
    float height = 1.0 - Ray.Height;
    vec3 uv =  GetUV(Ray.Position, 200.0, Clouds.WindSpeed * -0.01);

    vec4 high_frequency_noise = texture(CloudHighFrequency, uv, lod);
    float high_frequency_fbm = (high_frequency_noise.r * 1.0 + high_frequency_noise.g * 0.5 + high_frequency_noise.b * 0.25) / 1.75;
    float high_frequency_modifier = mix(high_frequency_fbm, 1.0 - high_frequency_fbm, saturate(height * 25.0));

    base = mix(5.0 * Clouds.Density, Clouds.Density / 5.0, outScattering.a) * saturate(remap(base, high_frequency_modifier * mix(0.05, 0.15, outScattering.a), 1.0, 0.0, 1.0));
    return saturate(base);
}

void Blend(in RayMarch Ray, vec4 World)
{
    vec3 SkyColor = vec3(0.0), CloudsColor = vec3(0.0);

    vec3 Eye = ubo.CameraPosition.xyz;
    vec3 SunDir = ubo.SunDirection.xyz;

    if (outScattering.a != 1.0)
    {
        SAtmosphere Atmosphere;
        AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, Ray.T, Ray.A, Eye, Ray.FirstHit, SunDir, Atmosphere);

        vec3 Radiance      = Atmosphere.L;
        vec3 Scattering    = Atmosphere.S  * MaxLightIntensity;
        outScattering.rgb = Scattering + Radiance * outScattering.rgb;

        vec4 NDC = vec4(ubo_old.ViewProjectionMatrix * World);
        NDC = (NDC / NDC.w) * 0.5 + 0.5;
        if (NDC.x >= 0.0 && NDC.y >= 0.0 && NDC.y <= 1.0 && NDC.x <= 1.0)
        {
            outScattering = mix(outScattering, texture(PreviousFrame, NDC.xy), 0.25);
        }
    }

    if (outScattering.a != 0.0)
    {
        SkyColor += SkyScattering(TransmittanceLUT, InscatteringLUT, Eye, Ray.Direction, SunDir);

        if (Hits.TopCloud > 0.0)
        {
            vec3 HitPoint = Eye + Ray.Direction * Hits.TopCloud;

            SAtmosphere Atmosphere;
            AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, 1.0, 1.0, Eye, HitPoint, SunDir, Atmosphere);
            float PhaseF = HGDPhaseCloud(dot(ubo.SunDirection.xyz, Ray.Direction));
            
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
            float altocumulus_mask2 = saturate(mask2 - 0.25) / 0.75;
            float altocumulus_mask1 = saturate(altocumulus_mask2 * remap(saturate(mask1 - 0.2) / 0.8, altocumulus_mask2, 1.0, 0.0, 1.0));
            altocumulus = altocumulus_mask1 * altocumulus_mask2 * altocumulus;

            float cirrus = (left * texture(WeatherMap, vec2(125,15) * uv1).r + right * texture(WeatherMap, vec2(125,15) * uv2).r);
            float cirrus_mask = saturate(mask3 - 0.35) / 0.65;
            cirrus = cirrus_mask * cirrus;

            float high_level = ((saturate((cirrus + altocumulus) - 0.2) / 0.8) + (cirrus_mask * saturate((1.0 - altocumulus_mask2) - 0.5) / 0.5) * cirrus);
            SkyColor += PhaseF * Atmosphere.T * Atmosphere.L * high_level;
        }

        SkyColor *= MaxLightIntensity;
    }
    
    outScattering.rgb = mix(outScattering.rgb, SkyColor, outScattering.a * (1.0 - saturate(Hits.Ground)));
}

// volumetric-cloudscapes-of-horizon-zero-dawn
float SampleCone(RayMarch Ray, int mip)
{
    float cone_density = 0.0;
    Ray.Stepsize /= 6.f;
    for (int i = 0; i <= 6; i++)
    {
        Ray.Position = Ray.Position + light_kernel[i] * float(i) * Ray.Stepsize;

        if (cone_density < 0.3)
        {
            cone_density += Clouds.Density * SampleCloudShape(Ray, mip + 1);
        }
    }

    return cone_density;
}

float MultiScatter(float phi, float e, float ds)
{
    float luminance = 0.0;
    float a = 1.0, b = 1.0, c = 1.0;
    for (int i = 0; i < 5; i++)
    {
        luminance += b * HGDPhaseCloud(phi, c) * BeerLambert(e * a, ds);
        a *= 0.5;
        b *= 0.45;
        c *= 0.75;
    }

    return luminance;
}

float MarchToLight(vec3 pos, vec3 rd, float phi, int steps, int mip)
{
    RayMarch Ray;
    float len = SphereMinDistance(pos, rd, vec3(0.0), topBound);
    Ray.Stepsize = min(len, Rcdelta) / float(steps);
    Ray.Direction = ubo.SunDirection.xyz;
    Ray.Position = pos;

    float cone_density = 0.0;
    for (int i = 0; i < steps; i++)
    {
        UpdateRay(Ray);

        cone_density += SampleCone(Ray, mip);
        // cone_density += SampleCloudDetail(Ray, SampleCloudShape(Ray, mip + 1), mip + 1);
    }

    return MultiScatter(phi, cone_density, Ray.Stepsize);
}

// Unoptimized and expensive
void MarchToCloud(inout RayMarch Ray)
{
    Ray.Position = Ray.End;

    float phi = dot(ubo.SunDirection.xyz, Ray.Direction);
    float EdotV = dot(normalize(ubo.CameraPosition.xyz), Ray.Direction);
    float ray_length = distance(Ray.Start, Ray.End);
    float step_mod = max(float(ray_length / (topBound - bottomBound)), 1.0);

    float mean_depth = 0.0;
    float sample_density = 0.0;
    float mean_transmittance = 0.0;

    // skip empty space until cloud is found
    int steps = int(min(32 * step_mod, 96)), i = 0;
    Ray.Stepsize = ray_length / float(steps);

    bool found = false;
    bool isreverse = false;
    for (i = steps; i > 0; i--)
    {
        UpdateRay(Ray);

        if (!isreverse && ((sample_density = SampleCloudShape(Ray, 1)) > 0))
        {
            if ((sample_density = SampleCloudDetail(Ray, sample_density, 1)) > 0)
            {
                Ray.End = Ray.Position;

                steps /= 2;
                i = steps - 1;
                Ray.Stepsize = Ray.Stepsize / float(steps);
                
                Ray.Direction = -Ray.Direction;
                isreverse = true;
                found = true;
            }
        }
        else if (isreverse && ((sample_density = SampleCloudShape(Ray, 0)) > 0))
        {
            if ((sample_density = SampleCloudDetail(Ray, sample_density, 0)) > 0)
            {
                Ray.End = Ray.Position;
            }
        }
        else if (isreverse)
        {
            break;
        }
    }

    if (!found)
    {
        return;
    }

    // go again for clouds
    Ray.FirstHit = Ray.End;
    Ray.LastHit = Ray.Start;
    Ray.Position = Ray.End;
    Ray.Direction = isreverse ? -Ray.Direction : Ray.Direction;
    steps = max(int(6 * step_mod), 16);
    float Stepsize = distance(Ray.Start, Ray.End) / float(steps);
    Ray.Stepsize = Stepsize;
    float ToCamera = distance(ubo.CameraPosition.xyz, Ray.FirstHit);
    float ToAEP = distance(ubo.CameraPosition.xyz, Ray.Start);

    float Dist = 0.0;
    float incr = 1.0 / float(steps);
    for (i = steps; i >= 0; i--)
    {
        UpdateRay(Ray, Stepsize, float(steps - i) * incr);
        
        int mip = int(floor(mix(5.0, 0.0, saturate(20.0 * outScattering.a))));
        if ((sample_density = SampleCloudShape(Ray, mip)) > 0)
            sample_density = SampleCloudDetail(Ray, sample_density, mip);

        Dist += Ray.Stepsize;
        if (sample_density > 0.0)
        {
            float extinction = sample_density + 1e-7;
            float transmittance = BeerLambert(extinction, Ray.Stepsize);
            vec3 E = extinction * MaxLightIntensity * vec3(MarchToLight(Ray.Position, ubo.SunDirection.xyz, phi, 6, mip));
            E = vec3(E - transmittance * E) / extinction;
            outScattering.rgb += E * outScattering.a;
            outScattering.a *= transmittance;

            float DistAEP = (Dist + ToCamera) / ToAEP;
            mean_depth += mix(Ray.Height, 1.0, DistAEP) * outScattering.a;
            mean_transmittance += outScattering.a * mix(1.0 - Ray.Height, 1.0, DistAEP);
            Ray.LastHit = Ray.Position;
        }
    }

    if (outScattering.a != 1.0)
    {
        Ray.T = saturate(mean_depth / mean_transmittance);
        Ray.A = smoothstep(0.0, 1.0, outScattering.a);

        vec4 clip1 = vec4(ubo.ViewProjectionMatrix * vec4(Ray.FirstHit, 1.0));
        vec4 clip2 = vec4(ubo.ViewProjectionMatrix * vec4(Ray.LastHit, 1.0));
        gl_FragDepth = max(clip1.z / clip1.w, clip2.z / clip2.w);
    }
}

void main()
{
    gl_FragDepth = 0.0;

    vec4 ScreenNDC = vec4(2.0 * ScreenUV - 1.0, 0.0, 1.0);
    vec4 ScreenView = ubo.ProjectionMatrixInverse * ScreenNDC;
    vec4 ScreenWorld = vec4(ubo.ViewMatrixInverse * ScreenView);
    vec3 RayOrigin = vec3(ubo.CameraPositionFP64.xyz);
    vec3 SphereCenter = vec3(0.0, 0.0, 0.0);
    outScattering = vec4(0.0, 0.0, 0.0, 1.0);

    RayMarch Ray;
    Ray.Direction = normalize(ScreenWorld.xyz / ScreenWorld.w);

    topBound = Rct;
    bottomBound = Rcb + Rcdelta * (0.5 - min(Clouds.Coverage, 0.5));

    Hits.Ground = SphereMinDistance(RayOrigin, Ray.Direction, SphereCenter, Rg);
    Hits.TopCloud = SphereMinDistance(RayOrigin, Ray.Direction, SphereCenter, topBound);
    Hits.BottomCloud = SphereMinDistance(RayOrigin, Ray.Direction, SphereCenter, bottomBound);

    Hits.Camera = length(RayOrigin);
    float muHoriz = sqrt(1.0 - pow(Rg / Hits.Camera, 2.0));
    float horizon = dot(RayOrigin, Ray.Direction) / Hits.Camera + muHoriz;

    if (Clouds.Coverage > 0 && (horizon > 0 || Hits.Camera > bottomBound))
    {
        if (Hits.Camera < bottomBound)
        {
            Ray.End = RayOrigin + Ray.Direction * Hits.BottomCloud;
            Ray.Start = RayOrigin + Ray.Direction * Hits.TopCloud;
        }
        else if (Hits.Camera > topBound)
        {
            if (horizon >= 0)
            {
                Ray.End = RayOrigin + Ray.Direction * Hits.TopCloud;
                Ray.Start = RayOrigin + Ray.Direction * SphereMaxDistance(RayOrigin, Ray.Direction, SphereCenter, topBound);
            }
            else
            {
                Ray.End = RayOrigin + Ray.Direction * Hits.TopCloud;
                Ray.Start = RayOrigin + Ray.Direction * Hits.BottomCloud;
            }
        }
        else
        {
            if (horizon >= 0)
            {
                Ray.End = RayOrigin;
                Ray.Start = RayOrigin + Ray.Direction * Hits.TopCloud;
            }
            else
            {
                Ray.End = RayOrigin;
                Ray.Start = RayOrigin + Ray.Direction * Hits.BottomCloud;
            }
        }

        MarchToCloud(Ray);
    }

    Blend(Ray, ScreenWorld);
}