#version 460
#include "ubo.glsl"
#include "lighting.glsl"

layout(push_constant) uniform constants
{
    mat4 WorldMatrix;
} PushConstants;

layout(location = 0) in vec3 vertPosition;

layout(location = 0) out vec4 WorldPosition;
layout(location = 1) out vec3 Normal;

void main()
{
    Normal = vec3(normalize(dvec3(ubo.CameraPositionFP64.xyz + vertPosition + vec3(0.0, Rg, 0.0))));
    WorldPosition = vec4((Rg - 10.0) * Normal - vec3(0.0, Rg, 0.0), 1.0); // this is used in FS, radius is offset a little, to avoid radiance sampling artifacts
    gl_Position = vec4(ubo.ViewProjectionMatrix * vec4(Rg * Normal - vec3(0.0, Rg, 0.0), 1.0)); // this is actual position (at 0 height)
}