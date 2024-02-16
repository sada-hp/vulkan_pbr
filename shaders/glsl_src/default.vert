#version 460
#include "ubo.glsl"

layout(push_constant) uniform constants
{
    mat4 WorldMatrix;
} PushConstants;

layout(location = 0) in vec3 vertPosition;

layout(location=0) out vec3 fragColor;

void main()
{
    gl_Position = ubo.ProjectionMatrix * ubo.ViewMatrix * (PushConstants.WorldMatrix * vec4(vertPosition, 1.0));
    fragColor = vec3(1, 1, 1);
}