layout(set = 0, binding = 0) uniform UnfiormBuffer
{
    dmat4 ViewProjectionMatrix;
    dmat4 ViewMatrix;
    dmat4 ViewMatrixInverse;
    dvec4 CameraPositionFP64;
    mat4 ProjectionMatrix;
    mat4 ProjectionMatrixInverse;
    vec4 CameraPosition;
    vec4 SunDirection;
    vec4 CameraUp;
    vec4 CameraRight;
    vec4 CameraForward;
    vec2 Resolution;
    double CameraRadius;
    float Time;
} ubo;

dmat4 GetWorldMatrix(mat3x4 Orientation, dvec3 Offset)
{
    return dmat4(Orientation[0][0], Orientation[0][1], Orientation[0][2], 0.0,
                 Orientation[1][0], Orientation[1][1], Orientation[1][2], 0.0,
                 Orientation[2][0], Orientation[2][1], Orientation[2][2], 0.0,
                 Offset[0], Offset[1], Offset[2], 1.0);
}

dmat3 GetTerrainOrientation()
{
    dmat3 Orientation;
    Orientation[0] = vec3(1.0, 0.0, 0.0);
    Orientation[1] = normalize(ubo.CameraPositionFP64.xyz);
    Orientation[2] = normalize(cross(Orientation[0], Orientation[1]));
    Orientation[0] = normalize(cross(Orientation[1], Orientation[2]));

    return Orientation;
}