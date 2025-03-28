#version 460
#include "ubo.glsl"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in flat int viewIndex[];
layout(location = 1) in vec4 vertPosition[];

layout(location = 0) out vec4 outPosition;

void main()
{
    for(int vertex = 0; vertex < 3; vertex++) 
    {
        gl_Layer = viewIndex[vertex];
        outPosition = vertPosition[vertex];
        gl_Position = gl_in[vertex].gl_Position;
        EmitVertex();
    }

	EndPrimitive();
}