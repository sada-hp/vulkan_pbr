layout(set = 0, binding = 0) uniform UnfiormBuffer
{
    dmat4 ViewProjectionMatrix;
    dmat4 ViewMatrix;
    dmat4 ViewMatrixInverse;
    mat4 ProjectionMatrix;
    mat4 ProjectionMatrixInverse;
    dvec4 CameraPositionFP64;
    vec4 CameraPosition;
    vec3 SunDirection;
    float Time;
    vec2 Resolution;
} ubo;