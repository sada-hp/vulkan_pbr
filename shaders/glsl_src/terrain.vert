#version 460
#include "ubo.glsl"
#include "lighting.glsl"

layout(push_constant) uniform constants
{
    layout(offset = 0) dvec3 Offset;
    layout(offset = 32) mat3x4 Orientation;
} PushConstants;

layout(location = 0) in vec3 vertPosition;

layout(location = 0) out vec4 WorldPosition;
layout(location = 1) out vec3 Normal;

void main()
{
    dmat3 Orientation;
    Orientation[0] = ubo.CameraRight.xyz;
    Orientation[1] = normalize(ubo.CameraPositionFP64.xyz);
    Orientation[2] = cross(Orientation[0], Orientation[1]);

    vec3 ObjectPosition = vec3(vertPosition.x, Rg, vertPosition.z);

    Normal = vec3(normalize(Orientation * ObjectPosition));
    WorldPosition = vec4(Rg * Normal, 1.0);
    gl_Position = vec4(ubo.ViewProjectionMatrix * WorldPosition);
}