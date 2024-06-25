#version 460

layout(input_attachment_index = 0,binding = 0) uniform subpassInput HDRColor;

layout(location = 0) in vec2 UV;

layout(location = 0) out vec4 Color;

// snatched from precomputed-atmospheric-scattering code
vec3 Tonemap(vec3 L) 
{
    float exposure = 2.5;
    L = L * exposure;
    L.r = L.r < 1.413 ? pow(L.r * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.r);
    L.g = L.g < 1.413 ? pow(L.g * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.g);
    L.b = L.b < 1.413 ? pow(L.b * 0.38317, 1.0 / 2.2) : 1.0 - exp(-L.b);
    return L;
}

void main()
{
    vec3 c = subpassLoad(HDRColor).rgb;
    Color = vec4(Tonemap(c), 1.0);
}