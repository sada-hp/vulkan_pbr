#ifndef _UBO_SHADER
#define _UBO_SHADER

layout(set = 0, binding = 0) uniform UnfiormBuffer
{
    dmat4 ViewProjectionMatrix;
    dmat4 ViewMatrix;
    dmat4 ViewMatrixInverse;
    dmat4 ViewProjectionMatrixInverse;
    dvec4 CameraPositionFP64;
    mat4 PlanetMatrix;
    mat4 ProjectionMatrix;
    mat4 ProjectionMatrixInverse;
    vec4 CameraPosition;
    vec4 SunDirection;
    vec4 WorldUp;
    vec4 CameraUp;
    vec4 CameraRight;
    vec4 CameraForward;
    vec2 Resolution;
    double CameraRadius;
    float Wind;
    float Time;
} ubo;

#endif