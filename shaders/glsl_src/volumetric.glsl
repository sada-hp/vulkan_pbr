#define USE_CLOUD_MIE

#include "ubo.glsl"
#include "lighting.glsl"
#include "noise.glsl"

vec4 outScattering;
float outDepth;

layout(push_constant) uniform constants
{
    layout(offset = 0) int Order;
} In;

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
    float Radius;
};

struct
{
    float Camera;
    float TopCloud;
    float BottomCloud;
    float Ground;
} Hits;

layout(set = 1, binding = 0) uniform CloudLayer
{
    float Coverage;
    float CoverageSq;
    float HeightFactor;
    float BottomSmoothnessFactor;
    float LightIntensity;
    float Ambient;
    float Density;
    float BottomBound;
    float TopBound;
    float BoundDelta;
} Params;

layout(set = 1, binding = 1) uniform sampler3D CloudLowFrequency;
layout(set = 1, binding = 2) uniform sampler3D CloudHighFrequency;
layout(set = 1, binding = 3) uniform sampler2D WeatherMap;
layout(set = 1, binding = 4) uniform sampler2D TransmittanceLUT;
layout(set = 1, binding = 5) uniform sampler2D IrradianceLUT;
layout(set = 1, binding = 6) uniform sampler3D InscatteringLUT;
layout(set = 1, binding = 7) uniform sampler2DArray WeatherMapCube;

layout(set = 2, binding = 0, rgba32f) uniform image2D outImage;
layout(set = 2, binding = 1, rg32f) uniform image2D depthImage;
layout(set = 2, binding = 2) uniform sampler2D oldImage;
layout(set = 2, binding = 3) uniform UnfiormBuffer2
{
    dmat4 ReprojectionMatrix;
    dmat4 ViewProjectionMatrix;
    dmat4 ViewMatrix;
    dmat4 ViewMatrixInverse;
    dmat4 ViewProjectionMatrixInverse;
} uboOld;

void UpdateRay(inout RayMarch Ray)
{
    Ray.Position += Ray.Direction * Ray.Stepsize;
    Ray.Radius = length(Ray.Position);
    Ray.Height = saturate((Ray.Radius - Params.BottomBound) / Params.BoundDelta);
}

void UpdateRay(inout RayMarch Ray, float Stepsize)
{
    Ray.Position += Ray.Direction * Stepsize;
    Ray.Radius = length(Ray.Position);
    Ray.Height = saturate((Ray.Radius - Params.BottomBound) / Params.BoundDelta);
}

void UpdateRaySmooth(inout RayMarch Ray, float Stepsize, float f)
{
    Ray.Stepsize = smoothstep(0.0, 2.0, f) * Stepsize * 5.3262290811553903259871638305242;
    Ray.Position += Ray.Direction * Ray.Stepsize;
    Ray.Radius = length(Ray.Position);
    Ray.Height = saturate((Ray.Radius - Params.BottomBound) / Params.BoundDelta);
}

void UpdateRayInverseSmooth(inout RayMarch Ray, float Stepsize, float f)
{
    Ray.Stepsize = inverse_smoothstep(0.0, 1.0, f) * Stepsize;
    Ray.Position += Ray.Direction * Ray.Stepsize;
    Ray.Height = saturate((length(Ray.Position) - Params.BottomBound) / Params.BoundDelta);
}

vec3 GetUV(vec3 p, float scale, float wind)
{
    vec3 uv = p / Params.TopBound;
    return scale * uv + wind * ubo.Time;
}

// Cheaper version of Sample-Density
float SampleCloudShape(in RayMarch Ray, int lod)
{
    vec3 uv = GetUV(Ray.Position, 50.0, ubo.Wind * 0.01);
    float height = 1.0 - Ray.Height;

    float base = textureLod(CloudLowFrequency, uv, lod).r;

    float h1 = pow(height, 0.65);
    float h2 = saturate(remap(1.0 - height, 0.0, Params.BottomSmoothnessFactor, 0.0, 1.0));

    vec3 sx = Ray.Position / Params.TopBound;
    vec2 anim = vec2(ubo.Wind * 0.01 * ubo.Time) + vec2(sin(1e-6 * ubo.Time), cos(1e-6 * ubo.Time));

    vec3 temp = SampleArrayAsCube(WeatherMapCube, sx, 1.0, anim, lod).rgb;
    float weather2 = saturate(temp.r + 0.2);

    temp = SampleArrayAsCube(WeatherMapCube, sx, 15.0, anim + vec2(sin(TWO_PI * temp.r), cos(TWO_PI * temp.b)) * 0.01, lod).rgb;
    float weather1 = 1.0 - saturate(temp.x + 0.2) * saturate(0.5 + temp.y);

    base *= weather1;
    float shape = 1.0 - Params.Coverage * h1 * h2 * weather2;

    return height * saturate(remap(base, shape, 1.0, Params.HeightFactor, 1.0));
}

float SampleCloudDetail(in RayMarch Ray, float base, int lod)
{
    float height = 1.0 - Ray.Height;
    vec3 uv =  GetUV(Ray.Position, 200.0, -ubo.Wind * 0.01);

    vec4 high_frequency_noise = textureLod(CloudHighFrequency, uv, lod);
    float high_frequency_fbm = (high_frequency_noise.r * 1.0 + high_frequency_noise.g * 0.5 + high_frequency_noise.b * 0.25) / 1.75;
    float high_frequency_modifier = mix(high_frequency_fbm, 1.0 - high_frequency_fbm, saturate(height * 25.0));

    return saturate(mix(2.0 * Params.Density, Params.Density / 2.0, outScattering.a) * saturate(remap(base, high_frequency_modifier * mix(0.05, 0.15, outScattering.a), 1.0, 0.0, 1.0)));
}

// volumetric-cloudscapes-of-horizon-zero-dawn
float SampleCone(RayMarch Ray, int mip)
{
    float cone_density = 0.0;
    Ray.Stepsize /= 6.f;
    for (int i = 0; i <= 6; i++)
    {
        Ray.Position = Ray.Position + light_kernel[i] * float(i) * Ray.Stepsize;
        cone_density += Params.Density * SampleCloudShape(Ray, mip);
    }

    return cone_density;
}

float MultiScatter(float phi, float e, float ds)
{
    float luminance = 0.0;
    float a = 1.0, b = 1.0, c = 1.0;
    for (int i = 0; i < 15; i++)
    {
        luminance += b * HGDPhaseCloud(phi, c) * BeerLambert(e * a, ds);
        a *= 0.7;
        b *= 0.75;
        c *= 0.9;
    }

    return saturate(luminance + Params.Ambient);
}

float MarchToLight(vec3 pos, vec3 rd, float phi, int steps, int mip)
{
    RayMarch Ray;
    const float len = SphereMinDistance(pos, rd, vec3(0.0), Params.TopBound);
    steps = int(ceil(float(steps) * mix(0.25, 1.0, smootherstep(0.0, 1.0, saturate(len / Params.BoundDelta)))));
    Ray.Stepsize = min(len, Params.BoundDelta * 1.5) / float(steps);
    Ray.Direction = rd;
    Ray.Position = pos;

    float cone_density = 0.0;
    for (int i = 0; i < steps; i++)
    {
        UpdateRay(Ray);
        cone_density += outScattering.a > 0.01 ? SampleCone(Ray, mip) : SampleCloudShape(Ray, mip);
    }

    return MultiScatter(phi, cone_density, Ray.Stepsize);
}

void MarchToCloud(inout RayMarch Ray, float ray_length)
{
    vec3 Sun = ubo.SunDirection.xyz;
    vec3 Eye = ubo.CameraPosition.xyz;
    Ray.Position = Ray.End;

    float phi = dot(Sun, Ray.Direction);
    float EdotV = dot(ubo.WorldUp.xyz, Ray.Direction);
    float EdotL = dot(ubo.WorldUp.xyz, Sun);
    float step_mod = smootherstep(0.0, 1.0, saturate(ray_length / (Params.BoundDelta * 10.0)));
    float sample_density = 0.0;

    // skip empty space until cloud is found
    int steps = int(mix(32.0, 64.0, step_mod)), i = 0;
    Ray.Stepsize = ray_length / float(steps);

    bool found = false;
    for (i = steps; i > 0; i--)
    {
        UpdateRay(Ray);

        if (!found && ((sample_density = SampleCloudShape(Ray, 1)) > 0))
        {
            Ray.End = Ray.Position;

            i = steps - 1;
            Ray.Stepsize = Ray.Stepsize / float(steps);
            
            Ray.Direction = -Ray.Direction;
            found = true;
        }
        else if (found && ((sample_density = SampleCloudShape(Ray, 0)) > 0))
        {
            if ((sample_density = SampleCloudDetail(Ray, sample_density, 0)) > 0)
            {
                Ray.End = Ray.Position;
            }
        }
        else if (found)
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
    Ray.Direction = -Ray.Direction;
    steps = int(mix(16.0, 96.0, step_mod));
    float Stepsize = distance(Ray.Start, Ray.End) / float(steps);
    Ray.Stepsize = Stepsize;
    float ToAEP = distance(Ray.FirstHit, Ray.Start);
    float incr = 1.0 / float(steps);

    SAtmosphere AtmosphereStart, AtmosphereEnd;
    AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, Eye, Ray.FirstHit, Sun, AtmosphereStart);
    AerialPerspective(TransmittanceLUT, IrradianceLUT, InscatteringLUT, Eye, Ray.Start, Sun, AtmosphereEnd);

    float Dist = 0.0;
    float PrevDist = 0.0, DistAEP = 0.0;
    float LightIntensity = MaxLightIntensity;
    for (i = steps; i >= 0; i--)
    {
        UpdateRaySmooth(Ray, Stepsize, float(steps - i) * incr);

        if (Ray.Radius < Params.BottomBound)
            continue;

        if (Ray.Radius > Params.TopBound)
            break;
        
        int mip = int(floor(mix(5.0, 0.0, saturate(20.0 * outScattering.a))));
        if ((sample_density = SampleCloudShape(Ray, mip)) > 0 && outScattering.a > 0.1)
            sample_density = SampleCloudDetail(Ray, sample_density, mip);

        Dist += Ray.Stepsize;
        if (sample_density > 0.0)
        {
            float extinction = sample_density + 1e-7;
            float transmittance = BeerLambert(extinction, Ray.Stepsize);
            vec3 E = MarchToLight(Ray.Position, Sun, phi, 6, mip) * GetTransmittanceWithShadow(TransmittanceLUT, Ray.Radius, dot(Ray.Position, Sun) / Ray.Radius, Hits.Camera, EdotL);
            E += mix(AtmosphereStart.S, AtmosphereEnd.S, DistAEP) - mix(AtmosphereStart.S, AtmosphereEnd.S, PrevDist);
            E *= extinction * LightIntensity;
            E = vec3(E - transmittance * E) / extinction;
            Ray.LastHit = Ray.Position;
            outScattering.rgb += E * outScattering.a;
            outScattering.a *= transmittance;
            PrevDist = DistAEP;

            if (outScattering.a < 1e-5)
                break;
        }
        
        DistAEP = Dist / ToAEP;
    }

    outScattering.rgb += Params.LightIntensity * MaxLightIntensity * AtmosphereStart.S * (1.0 - outScattering.a);
}