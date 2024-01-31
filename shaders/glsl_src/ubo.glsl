layout(binding = 0) uniform UnfiormBuffer
{
    mat4 ViewProj;
    mat4 ProjectionMatrix;
    mat4 ViewMatrix;
} ubo;

layout(binding = 1) uniform ViewBuffer
{
    vec4 CameraPosition;
    vec2 Resolution;
} View;