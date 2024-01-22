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

layout(location = 0) out vec3 WorldPosition;

void main()
{
    vec3 pos = vertices[indices[gl_VertexIndex]];
    WorldPosition = 50.0 * pos;
    gl_Position = ubo.ViewProj * vec4(WorldPosition, 1.0);
}