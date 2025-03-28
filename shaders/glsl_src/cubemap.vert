#version 460
#include "cubemap_matrix.glsl"

layout(location = 0) out flat int viewIndex;
layout(location = 1) out vec4 vertPosition;

void main()
{
    viewIndex = gl_VertexIndex / 6;
    vertPosition = Vertex[Index[gl_VertexIndex]];

    gl_Position = Projection * View[viewIndex] * vertPosition;
}