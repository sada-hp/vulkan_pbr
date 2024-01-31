#version 460
#include "ubo.glsl"

const vec3 vertices[8] = vec3[] (
    vec3(1.0, 1.0, 1.0),
    vec3(1.0, 1.0, -1.0),
    vec3(-1.0, 1.0, 1.0),
    vec3(-1.0, 1.0, -1.0),
    vec3(1.0, -1.0, 1.0),
    vec3(1.0, -1.0, -1.0),
    vec3(-1.0, -1.0, 1.0),
    vec3(-1.0, -1.0, -1.0)
);

const uint indices[36] = uint[] (
    0, 1, 2, 2, 1, 3,
	6, 5, 4, 7, 5, 6,
	0, 2, 6, 6, 4, 0,
	1, 0, 5, 4, 5, 0,
	5, 7, 1, 3, 1, 7,
	7, 6, 3, 2, 3, 6
);

layout(location = 0) out vec3 Position;

void main()
{
    vec3 pos = vertices[indices[gl_VertexIndex]];
    vec4 WorldPosition = vec4(50.0 * pos, 1.0);
    vec4 ViewPosition = ubo.ViewMatrix * WorldPosition;

    Position = WorldPosition.xyz;
    gl_Position = ubo.ProjectionMatrix * ViewPosition;
}