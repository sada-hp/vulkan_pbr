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

void UpdateRay(inout RayMarch Ray, float Stepsize, float f)
{
    Ray.Stepsize = smootherstep(0.0, 2.0, f) * 4.0 * Stepsize;
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

float SampleCloudDetail(in RayMarch Ray, float base, int lod)
{
    float height = 1.0 - Ray.Height;
    vec3 uv =  GetUV(Ray.Position, 400.0, -0.05);

    vec4 high_frequency_noise = texture(CloudHighFrequency, uv, lod);
    float high_frequency_fbm = high_frequency_noise.r * 0.625 + high_frequency_noise.g * 0.25 + high_frequency_noise.b * 0.125;
    float high_frequency_modifier = mix(high_frequency_fbm, 1.0 - high_frequency_fbm, saturate(height * 25.0));

    base = remap(base, high_frequency_modifier * mix(0.15, 0.45, Clouds.Coverage), 1.0, 0.0, 1.0);
    return saturate(height * Clouds.Density * base);
}

void Blend(in RayMarch Ray)
{
    vec3 Eye = ubo.CameraPosition.xyz;
    vec3 SunDir = ubo.SunDirection.xyz;
    vec3 SkyColor = vec3(0.0), CloudsColor = vec3(0.0);

    if (outScattering.a != 0.0)
    {
        SAtmosphere Atmosphere;
        AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, 1.0, 1.0, Eye, Eye + Ray.Direction * SphereMinDistance(Eye, Ray.Direction, vec3(0.0), Rct), SunDir, Atmosphere);

        SkyColor += SkyScattering(TransmittanceLUT, InscatteringLUT, Eye, Ray.Direction, SunDir);

#if 1
        float PhaseF = HGDPhaseCloud(dot(ubo.SunDirection.xyz, Ray.Direction));
        float Hit = SphereMinDistance(Eye, Ray.Direction, vec3(0.0), Rct);
        if (Hit > 0.0)
        {
            vec3 n = normalize(Eye + Ray.Direction * Hit);
            float pole = abs(dot(n, vec3(0, 1, 0)));
            vec2 uv1 = 0.5 + vec2(atan(n.z, n.x) * ONE_OVER_2PI, asin(n.y) * ONE_OVER_2PI);
            vec2 uv2 = 0.5 + vec2(atan(n.x, n.y) * ONE_OVER_2PI, asin(n.z) * ONE_OVER_2PI);

            float left = pole > 0.9 ? 0.0 : (pole >= 0.8 ? 1.0 - ((pole - 0.8) / (0.9 - 0.8)) : 1.0);
            float right = 1.0 - left;
            float mask = left * texture(WeatherMap, 5.0 * uv1).b + right * texture(WeatherMap, 5.0 * uv2).b;
            mask = saturate(mask - 0.6) / 0.4;

            float www = left * texture(WeatherMap, 700.0 * uv1).g + right * texture(WeatherMap, 700.0 * uv2).g;
            float y = left * texture(WeatherMap, vec2(250.0, 25.0) * uv1).r + right * texture(WeatherMap, vec2(250.0, 25.0) * uv2).r;
            y = saturate(y - 0.2) / 0.8;
            www = saturate(remap(www, y, 1.0, 0.0, 1.0));

            SkyColor += PhaseF * Atmosphere.T * Atmosphere.L * mask * www;

            mask = left * texture(WeatherMap, 5.0 * -uv1).b + right * texture(WeatherMap, 5.0 * -uv2).b;
            mask = saturate(mask - 0.6) / 0.4;
            y = left * texture(WeatherMap, vec2(15.0, 65.0) * uv1).r + right * texture(WeatherMap, vec2(15.0, 65.0) * uv2).r;
            y = smootherstep(0.0, 1.0, saturate(y - 0.2) / 0.8);
            SkyColor += PhaseF * Atmosphere.T * Atmosphere.L * mask * y;
        }
#endif
     
        SkyColor *= MaxLightIntensity;
    }

    if (outScattering.a != 1.0)
    {
        SAtmosphere Atmosphere;
        AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, Ray.T, Ray.A, Eye, Ray.LastHit, SunDir, Atmosphere);

        vec3 Radiance      = Atmosphere.L;
        vec3 Scattering    = Atmosphere.S  * MaxLightIntensity;

        CloudsColor = Scattering + Radiance * outScattering.rgb;
    }
    
    if (Hits.Ground == 0.0)
        outScattering.rgb = mix(CloudsColor, SkyColor, outScattering.a);
    else
        outScattering.rgb = CloudsColor;
}

// volumetric-cloudscapes-of-horizon-zero-dawn
float SampleCone(RayMarch Ray, int mip)
{
    float cone_density = 0.0;
    Ray.Stepsize /= 6.f;
    for (int i = 0; i <= 6; i++)
    {
        UpdateRay(Ray);
        Ray.Position = Ray.Position + light_kernel[i] * float(i) * Ray.Stepsize;

        if (cone_density < 0.2)
        {
            float density = 0.0;
            if ((density = SampleCloudShape(Ray, mip + 1)) > 0.0)
            {
                cone_density += SampleCloudDetail(Ray, density, mip + 1) + 1e-6;
            }
        }
        else
            cone_density += Clouds.Density * SampleCloudShape(Ray, mip + 1) + 1e-6;
    }

    return cone_density;
}

float MarchToLight(vec3 pos, vec3 rd, int steps, int mip)
{
    const float shadowclr = 0.05;
    if (outScattering.a < shadowclr)
        return shadowclr;

    float len = SphereMinDistance(pos, rd, vec3(0.0), topBound);
    if (len <= 1.0)
        return 1.0;
    
    RayMarch Ray;
    Ray.Stepsize = min(len, topBound - bottomBound) / float(steps);
    Ray.Direction = ubo.SunDirection.xyz;
    Ray.Position = pos;

    float transmittance = 1.0;
    for (int i = 0; i < steps; i++)
    {
        UpdateRay(Ray);

        float cone_density = SampleCone(Ray, mip);

        if (cone_density > 0.0)
        {
            transmittance *= BeerLambert(cone_density, Ray.Stepsize);
        }
    }

    return mix(shadowclr, 1.0, transmittance);
}

// Unoptimized and expensive
void MarchToCloud(inout RayMarch Ray)
{
    Ray.Position = Ray.End;

    // float PhaseF = DualLobeFunction(dot(ubo.SunDirection.xyz, Ray.Direction), -0.2, 0.2, 0.25);
    float PhaseF = HGDPhaseCloud(dot(ubo.SunDirection.xyz, Ray.Direction));

    float EdotV = dot(normalize(ubo.CameraPosition.xyz), Ray.Direction);
    float w = saturate(1000.0 * pow(abs(EdotV), 5.0));

    float mean_depth = 0.0;
    float sample_density = 0.0;
    float mean_transmittance = 0.0;

    // take larger steps and
    // skip empty space until cloud is found
    int steps = int(ceil(mix(96, 72, w))), i = steps - 1;
    Ray.Stepsize = distance(Ray.Start, Ray.End) / float(steps);
    for (i; i > 0 && sample_density == 0.0;i--)
    {
        UpdateRay(Ray);

        if ((sample_density = SampleCloudShape(Ray, 3)) > 0)
            sample_density = SampleCloudDetail(Ray, sample_density, 2);
    }

    if (sample_density == 0.0)
    {
        return;
    }

    // go more precise for clouds
    Ray.End = Ray.Position - Ray.Stepsize * Ray.Direction;
    Ray.FirstHit = Ray.Position;
    Ray.Position = Ray.End;

    steps = i;
    float incr = 1.0 / float(steps);
    float Stepsize = distance(Ray.Start, Ray.End) / float(steps);
    Ray.Stepsize = Stepsize;
    const float I = PI * MaxLightIntensity * PhaseF;
    float ToCamera = distance(ubo.CameraPosition.xyz, Ray.FirstHit);
    float ToAEP = distance(ubo.CameraPosition.xyz, Ray.Start);
    for (i = steps - 1; i > 0; i--)
    {
        UpdateRay(Ray, Stepsize, float(steps - i) * incr);
        
        int mip = int(floor(mix(5.0, 0.0, outScattering.a)));
        if ((sample_density = SampleCloudShape(Ray, mip)) > 0 && outScattering.a > 1e-2)
            sample_density = SampleCloudDetail(Ray, sample_density, mip);

        if (sample_density > 0.0) 
        {
            float extinction = sample_density + 1e-6;
            float transmittance = BeerLambert(extinction, Ray.Stepsize);
            float luminance = MarchToLight(Ray.Position, ubo.SunDirection.xyz, 12, mip) * I;

            vec3 E = extinction * vec3(luminance);
            E = vec3(E - transmittance * E) / extinction;
            outScattering.rgb += E * outScattering.a;
            outScattering.a *= transmittance;
            Ray.LastHit = Ray.Position;

            float Dist = ((float(steps - i + 1) * Ray.Stepsize + ToCamera) / ToAEP);
            mean_depth += mix(Ray.Height, 1.0, Dist) * outScattering.a;
            mean_transmittance += outScattering.a * mix(1.0 - Ray.Height, 1.0, Dist);

            if (outScattering.a <= 1e-5)
                break;
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

    vec4 ScreenNDC = vec4(2.0 * ScreenUV - 1.0, 1.0, 1.0);
    vec4 ScreenView = ubo.ProjectionMatrixInverse * ScreenNDC;
    vec4 ScreenWorld = vec4(ubo.ViewMatrixInverse * vec4(ScreenView.xyz, 0.0));
    vec3 RayOrigin = vec3(ubo.CameraPositionFP64.xyz);
    vec3 SphereCenter = vec3(0.0, 0.0, 0.0);
    outScattering = vec4(0.0, 0.0, 0.0, 1.0);

    RayMarch Ray;
    Ray.Direction = normalize(ScreenWorld.xyz);

    topBound = Rct;
    bottomBound = Rcb + Rcdelta * 0.5 * (1.0 - max(Clouds.Coverage, 0.25));

    Hits.Ground = SphereMinDistance(RayOrigin, Ray.Direction, SphereCenter, Rg);
    Hits.TopCloud = SphereMinDistance(RayOrigin, Ray.Direction, SphereCenter, topBound);
    Hits.BottomCloud = SphereMinDistance(RayOrigin, Ray.Direction, SphereCenter, bottomBound);

    Hits.Camera = length(RayOrigin);
    float muHoriz = -sqrt(1.0 - pow(Rg / Hits.Camera, 2.0));
    float horizon = dot(RayOrigin, Ray.Direction) / Hits.Camera - muHoriz;

    if (Clouds.Coverage > 0 && (horizon > 0 || Hits.Camera > bottomBound))
    {
        if (Hits.Camera < bottomBound)
        {
            Ray.End = RayOrigin + Ray.Direction * Hits.BottomCloud;
            Ray.Start = RayOrigin + Ray.Direction * Hits.TopCloud;
        }
        else if (Hits.Camera > topBound)
        {
            if (Hits.Ground == 0)
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
            if (Hits.Ground == 0)
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
    // else if (Hits.Ground != 0.0)
    // {
    //     discard;
    // }

    Blend(Ray);
}