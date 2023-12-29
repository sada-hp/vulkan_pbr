#version 460

layout(binding=0) uniform UnfiormBuffer
{
    mat4 ViewProj;
    vec3 CameraPos;
    vec3 LightDir;
} ubo;

layout(location = 0) in vec3 vertPosition;

layout(location=0) out vec3 fragColor;

void main()
{

}