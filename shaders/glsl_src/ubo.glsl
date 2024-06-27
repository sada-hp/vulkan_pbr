layout(set = 0, binding = 0) uniform UnfiormBuffer
{
    mat4 ViewProjectionMatrix;
    mat4 ProjectionMatrix;
    mat4 ViewMatrix;
    vec4 CameraPosition;
    vec3 SunDirection;
    float Time;
    vec2 Resolution;
} ubo;