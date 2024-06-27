#version 460

layout(input_attachment_index = 0,binding = 0) uniform subpassInput HDRColor;

layout(location = 0) in vec2 UV;

layout(location = 0) out vec4 Color;

// snatched from precomputed-atmospheric-scattering code
vec3 Tonemap(vec3 L) 
{
    const float exposure = 0.65;
    const float gamma = 2.2;

    L = L * exposure;
    L.r = L.r < 1.413 ? pow(L.r * 0.38317, 1.0 / gamma) : 1.0 - exp(-L.r);
    L.g = L.g < 1.413 ? pow(L.g * 0.38317, 1.0 / gamma) : 1.0 - exp(-L.g);
    L.b = L.b < 1.413 ? pow(L.b * 0.38317, 1.0 / gamma) : 1.0 - exp(-L.b);
    return L;
}

vec3 Tonemap2(vec3 L) 
{
    const float gamma = 2.2;
    L = L / (L + vec3(1.0));
    return pow(L, vec3(1.0 / gamma));
}

void main()
{
    vec3 c = subpassLoad(HDRColor).rgb;
    Color = vec4(Tonemap(c), 1.0);
}