#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "noise.glsl"

layout(push_constant) uniform constants
{
    layout(offset = 0) dvec3 Offset;
    layout(offset = 32) mat3x4 Orientation;
} PushConstants;

layout(location = 0) in vec3 vertPosition;

layout(location = 0) out vec4 WorldPosition;
layout(location = 1) out vec3 Normal;
layout(location = 2) out float Height;

float RoundToIncrement(float value, float increment)
{
    return round(value * (1.0 / increment)) * increment;
}

vec2 RoundToIncrement(vec2 value, float increment)
{
    return round(value * (1.0 / increment)) * increment;
}

vec3 RoundToIncrement(vec3 value, float increment)
{
    return round(value * (1.0 / increment)) * increment;
}

dvec3 RoundToIncrement(dvec3 value, float increment)
{
    return round(value * (1.0 / increment)) * increment;
}

void main()
{
    dmat3 Orientation;
    Orientation[0] = vec3(1.0, 0.0, 0.0);
    Orientation[1] = normalize(ubo.CameraPositionFP64.xyz);

    Orientation[2] = normalize(cross(Orientation[0], Orientation[1]));
    Orientation[0] = normalize(cross(Orientation[1], Orientation[2]));

    dvec3 Center = RoundToIncrement(ubo.CameraPositionFP64.xyz, 10.f * exp2(vertPosition.y));

    vec3 ObjectPosition = vec3(Orientation * vec3(vertPosition.x, 0.0, vertPosition.z) + Center);

    Normal = vec3(normalize(ObjectPosition));

    float perlin1 = perlin(Normal.xz, 200.0 * 1.0);
    float perlin2 = perlin(Normal.xz, 300.0 * 4.0);
    float perlin3 = perlin(Normal.xz, 400.0 * 6.0);
    float perlin4 = perlin(Normal.xz, 500.0 * 16.0);
    float perlin5 = perlin(Normal.xz, 100.0);

    Height = pow(saturate(
        (saturate(perlin1 * 1.0 + perlin2 * 0.5 + perlin3 * 0.25 + perlin4 * 0.125) - perlin5 * perlin1)
        // (perlin5 * perlin1 + pow(1.0 - perlin2, 2.0) * 0.5 + perlin3 * 0.25 + perlin4 * 0.125)
    ), 3.0);
    
    WorldPosition = vec4(Normal * (300.0 + Rg + Height * 7500.0), 1.0);
    // WorldPosition = vec4(Normal * Rg, 1.0);

    gl_Position = vec4(ubo.ViewProjectionMatrix * WorldPosition);
}