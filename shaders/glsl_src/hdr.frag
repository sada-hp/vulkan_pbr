#version 460

layout(input_attachment_index = 0,binding = 0) uniform subpassInput HDRColor;

layout(location = 0) in vec2 UV;

layout(location = 0) out vec4 Color;

void main()
{
    vec3 c = subpassLoad(HDRColor).rgb;
    c = c / (c + vec3(1.0));
    Color = vec4(pow(c, vec3(1.0 / 2.2)), 1.0);
}