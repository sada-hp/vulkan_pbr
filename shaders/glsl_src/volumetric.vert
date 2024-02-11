#version 460
#include "ubo.glsl"

void main()
{
    vec2 UV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(UV * 2.0f - 1.0f, 0.0f, 1.0f);
}