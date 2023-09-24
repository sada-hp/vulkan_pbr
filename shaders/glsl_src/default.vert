#version 460

layout(binding=0) uniform UnfiormBuffer
{
    mat4 ViewProj;
} ubo;

layout(location = 0) in vec3 vertPosition;

layout(location=0) out vec3 fragColor;

void main()
{
    gl_Position = ubo.ViewProj * vec4(vertPosition, 1.0);
    fragColor = vec3(1, 0, 0);
}