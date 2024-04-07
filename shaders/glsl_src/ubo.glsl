layout(binding = 0) uniform UnfiormBuffer
{
    mat4 ViewProjectionMatrix;
    mat4 ProjectionMatrix;
    mat4 ViewMatrix;
    vec3 SunDirection;
    float Time;
} ubo;

layout(binding = 1) uniform ViewBuffer
{
    vec4 CameraPosition;
    vec2 Resolution;
} View;