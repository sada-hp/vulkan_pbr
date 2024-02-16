#version 460
#include "ubo.glsl"
#include "noise.glsl"

#define EARTH_RADIUS 6370000
#define ATMOSPHERE_START 6420000
#define ATMOSPHERE_END 6700000
#define ATMOSPHERE_DELTA ATMOSPHERE_END - ATMOSPHERE_END
#define AMBIENT 0.25
#define ABSORPTION 0.0005

const vec3 SunDirection = normalize(vec3(1.0));
vec3 sphereStart;
vec3 sphereEnd;
float sphereDistance;

layout(location=0) out vec4 outColor;

layout(binding = 2) uniform sampler3D CloudLowFrequency;
layout(binding = 3) uniform sampler3D CloudHighFrequency;
layout(binding = 4) uniform sampler2D GradientTexture;

bool raySphereintersection(vec3 ro, vec3 rd, float radius, out vec3 p1)
{
	vec3 sphereCenter = vec3(View.CameraPosition.x, -EARTH_RADIUS, View.CameraPosition.z);

	float radius2 = radius*radius;

	vec3 L = ro - sphereCenter;
	float a = dot(rd, rd);
	float b = 2.0 * dot(rd, L);
	float c = dot(L,L) - radius2;

	float discr = b*b - 4.0 * a * c;
	if(discr < 0.0) return false;
	float t1 = max(0.0, (-b + sqrt(discr))/2);
    //float t2 = max(0.0, (-b - sqrt(discr))/2);
	if(t1 == 0.0){
		return false;
	}

    p1 = ro + rd * t1;

	return true;
}

float GetHeightFraction(vec3 p)
{
    return saturate((p.y - sphereStart.y) / (sphereEnd.y - sphereStart.y));
}

float SampleCloudShape(vec3 x0)
{
    float height = GetHeightFraction(x0);
    vec3 uv =  x0 / ATMOSPHERE_START * 5.0;

    vec4 low_frequency_noise = texture(CloudLowFrequency, uv + vec3(ubo.Time, 0.0, 0.0) / 50.0);
    float low_frequency_fbm = low_frequency_noise.g * 0.625 + low_frequency_noise.b * 0.25 + low_frequency_noise.a * 0.125;
    float base = remap(low_frequency_noise.r, (1.0 - low_frequency_fbm), 1.0, 0.0, 1.0);
    base *= texture(GradientTexture, vec2(0.0, height)).r;

    return base;
}

//GPU Pro 7
float SampleDensity(vec3 x0)
{
    float height = GetHeightFraction(x0);
    vec3 uv =  x0 / ATMOSPHERE_START * 5.0;

    vec4 low_frequency_noise = texture(CloudLowFrequency, uv + vec3(ubo.Time, 0.0, 0.0) / 50.0);
    float low_frequency_fbm = low_frequency_noise.g * 0.625 + low_frequency_noise.b * 0.25 + low_frequency_noise.a * 0.125;
    float base = remap(low_frequency_noise.r, (1.0 - low_frequency_fbm), 1.0, 0.0, 1.0);
    base *= texture(GradientTexture, vec2(0.0, height)).r;

    vec4 high_frequency_noise = texture(CloudHighFrequency, (uv * 20.0) + vec3(ubo.Time, 0.0, 0.0) / 10.0);
    float high_frequency_fbm = high_frequency_noise.r * 0.625 + high_frequency_noise.g * 0.25 + high_frequency_noise.b * 0.125;
    float high_frequency_modifier = mix(high_frequency_fbm, 1.0 - high_frequency_fbm, clamp(height * 10.0, 0.0, 1.0));

    float cloud_final = remap(base, high_frequency_modifier * 0.2, 1.0, 0.0, 1.0);

    return cloud_final;
}

float MarchToLight(vec3 rs, vec3 re, vec3 rd, float stepsize)
{
    const int steps = 100;
    float transmittance = 1.0;

    vec3 pos = rs;
    for (int i = 0; i < steps && transmittance > AMBIENT; i++)
    {
        pos += stepsize * rd;

        if (pos.y <= sphereEnd.y)
        {
            float density = SampleDensity(pos);

            if (density > 0.0) {
                float transmittance_cloud = exp(-stepsize * density * ABSORPTION);
                transmittance *= transmittance_cloud;
            }
        }
        else {
            break;
        }
    }

    return max(transmittance, AMBIENT);
}

vec4 MarchToCloud(vec3 rs, vec3 re, vec3 rd)
{
    const int steps = 200;
    const float len = distance(rs, re);
    const float stepsize = len / float(steps);
    vec3 cloud_scatter = vec3(0.0);
    float transmittance = 1.0;

    vec3 pos = rs;
    float density = SampleCloudShape(pos);

    int i = 1;
    if (density == 0.0) {
        int mult = 5;
        while (density == 0.0 && i < steps) {
            pos += stepsize * mult * rd;
            density = SampleCloudShape(pos);
            i += mult;
        }

        pos -= stepsize * mult * rd;
        i -= mult;
    }

    for (; i < steps && transmittance > 1e-3; i++) {
        pos += stepsize * rd;
        density = SampleDensity(pos);

        if (density > 0.0) {
            float transmittance_old = transmittance;
            float transmittance_cloud = exp(-stepsize * density * ABSORPTION);
            transmittance *= transmittance_cloud;

            float scatter_amount = MarchToLight(pos, re, SunDirection, stepsize);
            //cloud_scatter += transmittance * (1.0 - transmittance_cloud) * scatter_amount;
            cloud_scatter += (transmittance_old - transmittance) * scatter_amount;
        }
    }

    return clamp(vec4(cloud_scatter.rgb, 1.0 - transmittance), 0.0, 1.0);
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
        sphereDistance = distance(sphereStart, sphereEnd);
        outColor = MarchToCloud(sphereStart, sphereEnd, RayDirection);
    }
    else {
        outColor = vec4(0.0);
    }

    gl_FragDepth = 1.0;
}