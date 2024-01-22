#version 460
#include "ubo.glsl"

layout(location = 0) in vec3 WorldPosition;

layout(location=0) out vec4 outColor;

layout(binding = 1) uniform sampler3D CloudLowFrequency;

float remap(float orig, float old_min, float old_max, float new_min, float new_max)
{
    return new_min + ((orig - old_min) / (old_max - old_min)) * (new_max - new_min);
}

float SampleDensity(vec3 x0)
{
    vec4 low_frequency_noise = texture(CloudLowFrequency, x0);
    float low_frequency_fbm = low_frequency_noise.g * 0.625 + low_frequency_noise.b * 0.25 + low_frequency_noise.a * 0.125;
    float density = remap(low_frequency_noise.r, -(1.0 - low_frequency_fbm), 1.0, 0.0, 1.0);

    return density;
}

vec4 RayMarch(vec3 p, vec3 d)
{
    const int steps = 1000;
    const float step = 100.0 / float(steps);
    float density = 0.0;

    for (int i = 0; i < steps; i++) {
        p += step * d;

        //if (p.y < -50.0 || p.y > 50.0 || p.x < -50.0 || p.x > 50.0 || p.z < -50.0 || p.z > 50.0) {
        //    break;
        //}

        float n = SampleDensity(p / 100.0);
        //float n = texture(CloudLowFrequency, p / 100.0).g;
        if (n >= 0.0) {
            density += n;
        }
    }

    return clamp(vec4(density / float(steps)), 0.0, 1.0);
}

void main()
{
    vec3 ViewDirection = normalize(ubo.CameraPosition - WorldPosition);
    outColor = RayMarch(WorldPosition, ViewDirection);
}