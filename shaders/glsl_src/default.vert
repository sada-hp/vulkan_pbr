#version 460

layout(binding=0) uniform UnfiormBuffer
{
    mat4 ViewProj;
} ubo;

vec4 vertices[3] = vec4[] (
    vec4(0, -1.5, 0, 1),
    vec4(-1.5, 1.5, 0, 1),
    vec4(1.5, 1.5, 0, 1)
);

vec3 colors[3] = vec3[] (
    vec3(1, 0, 0),
    vec3(0, 1, 0),
    vec3(0, 0, 1)
);

layout(location=0) out vec3 fragColor;

void main()
{
    gl_Position = ubo.ViewProj * vertices[gl_VertexIndex];
    fragColor = colors[gl_VertexIndex];
}