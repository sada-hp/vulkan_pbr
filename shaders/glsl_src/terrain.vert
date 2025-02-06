#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "noise.glsl"

layout (constant_id = 2) const float Scale = 1.f;
layout (constant_id = 3) const float MinHeight = 1.f;
layout (constant_id = 4) const float MaxHeight = 1.f;
layout (constant_id = 5) const uint Seed = 0;

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

    dvec3 Center = RoundToIncrement(ubo.CameraPositionFP64.xyz, Scale * exp2(vertPosition.y) * 0.5f);

    vec3 ObjectPosition = vec3(Orientation * vec3(vertPosition.x, 0.0, vertPosition.z) + Center);

    Normal = vec3(normalize(ObjectPosition));

    if (Seed != 0)
    {
        NOISE_SEED = Seed;

        float perlin1 = perlin(Normal.xz, 400.0 * 1.0);
        float perlin2 = perlin(Normal.xy, 400.0 * 1.0);
        float perlin3 = perlin(Normal.yz, 400.0 * 1.0);

        Height = saturate((perlin1 + perlin2 + perlin3) / 3.0);
        
        WorldPosition = vec4(Normal * (MinHeight + Rg + Height * (MaxHeight - MinHeight)), 1.0);
    }
    else
    {
        WorldPosition = vec4(Normal * Rg, 1.0);
    }

    gl_Position = vec4(ubo.ViewProjectionMatrix * WorldPosition);
}