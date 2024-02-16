#version 460

layout(push_constant) uniform constants
{
    layout(offset = 64) vec3 ColorMask;
} PushConstants;

layout(location=0) in vec3 color;

layout(location=0) out vec4 outColor;

void main()
{
    outColor = vec4(PushConstants.ColorMask * color, 1);
}