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
layout(location = 4) out flat int Level;

layout(set = 1, binding = 4) uniform sampler2D NoiseMap;

void main()
{
    ivec2 noiseSize = ivec2(textureSize(NoiseMap, 0));
    ivec2 texelId = ivec2(gl_VertexIndex % noiseSize.x, gl_VertexIndex / noiseSize.x);

    float Height = texelFetch(NoiseMap, texelId, 0).r;

    dmat3 Orientation;
    Orientation[0] = vec3(1.0, 0.0, 0.0);
    Orientation[1] = normalize(ubo.CameraPositionFP64.xyz);
    Orientation[2] = normalize(cross(Orientation[0], Orientation[1]));
    Orientation[0] = normalize(cross(Orientation[1], Orientation[2]));

    // dvec3 Center = ubo.CameraPositionFP64.xyz;
    dvec3 Center = RoundToIncrement(ubo.CameraPositionFP64.xyz, Scale * exp2(vertPosition.y));
    vec3 ObjectPosition = vec3(Orientation * vec3(vertPosition.x, 0.0, vertPosition.z) + Center);
    Normal = vec3(normalize(ObjectPosition));

    UV = vec2(0.5) + vertPosition.xz / (Scale * exp2(Rings));
    Normal = vec3(normalize(ObjectPosition));
    Level = int(vertPosition.y);
    WorldPosition = vec4(Normal * (MinHeight + Rg + Height * (MaxHeight - MinHeight)), 1.0);

    gl_Position = vec4(ubo.ViewProjectionMatrix * WorldPosition);
    WorldPosition -= ubo.CameraPosition;
}