#version 460

layout(input_attachment_index = 0,binding = 0) uniform subpassInput HDRColor;

layout(location = 0) in vec2 UV;

layout(location = 0) out vec4 LinearColor;

void main()
{
    vec3 Color = subpassLoad(HDRColor).rgb;
    LinearColor = vec4(pow(Color, vec3(1.0 / 2.2)), 1.0);
}