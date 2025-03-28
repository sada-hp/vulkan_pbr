#version 460
#include "ubo.glsl"
#include "brdf.glsl"
#include "lighting.glsl"

layout(push_constant) uniform constants
{
    float Roughness;
} In;

layout(binding = 1) uniform sampler2D TransmittanceLUT;
layout(binding = 2) uniform sampler2D IrradianceLUT;
layout(binding = 3) uniform sampler3D InscatteringLUT;
layout(binding = 4) uniform samplerCube EnvironmentLUT;

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec4 vertPosition;

void main()
{
    outColor = vec4(0.0, 0.0, 0.0, 1.0);

    vec3 N = normalize(vertPosition.xyz);    
    vec3 R = N;
    vec3 V = R;

    const uint SAMPLE_COUNT = 512;
    float totalWeight = 0.0;   
    for(uint i = 0u; i < SAMPLE_COUNT; i++)
    {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(Xi, N, In.Roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if(NdotL > 0.0)
        {
            outColor.rgb += texture(EnvironmentLUT, L).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    outColor.rgb = outColor.rgb / totalWeight;
}