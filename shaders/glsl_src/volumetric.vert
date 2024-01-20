#version 460

layout(binding=0) uniform UnfiormBuffer
{
    mat4 ViewProj;
} ubo;

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
    2, 1, 0, 3, 1, 2,
	4, 5, 6, 6, 5, 7,
	6, 2, 0, 0, 4, 6,
	5, 0, 1, 0, 5, 4,
	1, 7, 5, 7, 1, 3,
	3, 6, 7, 6, 3, 2
);

void main()
{
    vec3 pos = vertices[indices[gl_VertexIndex]];
    gl_Position = ubo.ViewProj * vec4(pos * 20.0, 1.0);
}