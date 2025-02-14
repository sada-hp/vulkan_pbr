#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "noise.glsl"

layout (constant_id = 2) const float Scale = 1.f;
layout (constant_id = 3) const float MinHeight = 1.f;
layout (constant_id = 4) const float MaxHeight = 1.f;
layout (constant_id = 5) const uint Rings = 0;
layout (constant_id = 6) const uint Seed = 0;

layout(push_constant) uniform constants
{
    layout(offset = 0) dvec3 Offset;
    layout(offset = 32) mat3x4 Orientation;
} PushConstants;

layout(location = 0) in vec3 vertPosition;

layout(location = 0) out vec4 WorldPosition;
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec2 UV;
layout(location = 3) out float Height;

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

    dvec3 Center = RoundToIncrement(ubo.CameraPositionFP64.xyz, Scale * exp2(vertPosition.y));
    vec3 ObjectPosition = vec3(Orientation * vec3(vertPosition.x, 0.0, vertPosition.z) + Center);

    UV = 0.1f * vertPosition.zx / float(exp2(Rings));

    Normal = vec3(normalize(ObjectPosition));

    if (Seed != 0)
    {
        NOISE_SEED = Seed;

        float perlin1 = perlin(Normal.xz, 100.0 * 1.0);
        float perlin2 = perlin(Normal.yx, 100.0 * 1.0);
        float perlin3 = perlin(Normal.yz, 100.0 * 1.0);

        float perlin4 = perlin(Normal.xz, 300.0 * 1.0);
        float perlin5 = perlin(Normal.xz, 300.0 * 1.0);
        float perlin6 = perlin(Normal.xz, 300.0 * 1.0);

        Height = 0.25 * saturate(perlin1 * perlin2 * perlin3) + 0.75 * saturate(perlin4 * perlin5 * perlin6);
        
        WorldPosition = vec4(Normal * (MinHeight + Rg + Height * (MaxHeight - MinHeight)), 1.0);
    }
    else
    {
        WorldPosition = vec4(Normal * Rg, 1.0);
    }

    gl_Position = vec4(ubo.ViewProjectionMatrix * WorldPosition);
    WorldPosition -= ubo.CameraPosition;
}