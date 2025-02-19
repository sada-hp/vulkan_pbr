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

layout(location = 0) in vec4 vertPosition;
layout(location = 1) in vec4 vertUV;

layout(location = 0) out vec4 WorldPosition;
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec2 UV;
layout(location = 3) out float Height;
layout(location = 4) out flat int Level;

layout(set = 1, binding = 4) uniform sampler2DArray NoiseMap;

void main()
{
    Level = int(vertPosition.y);

    ivec2 noiseSize = ivec2(textureSize(NoiseMap, 0));
    UV = vertUV.xy;
    ivec3 texelId = ivec3(round(UV * (noiseSize - 1)), Level);

    vec4 Elevation = texelFetch(NoiseMap, texelId, 0);

    Normal = Elevation.xyz;
    Height = Elevation.a;

    WorldPosition = vec4(Elevation.xyz * (MinHeight + Rg + Elevation.a * (MaxHeight - MinHeight)), 1.0);

    gl_Position = vec4(ubo.ViewProjectionMatrix * WorldPosition);
    WorldPosition -= ubo.CameraPosition;
}