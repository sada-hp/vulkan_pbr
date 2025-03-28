#version 460
#include "ubo.glsl"
#include "lighting.glsl"

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
    vec3 U = normalize(ubo.CameraPosition.xyz);
    vec3 R = normalize(cross(U, N));
    U      = normalize(cross(N, R));

    float sampleDelta = 0.125;
    float nrSamples = 0.0; 
    for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
            
            // tangent space to world
            vec3 sampleVec = tangentSample.x * R + tangentSample.y * U + tangentSample.z * N; 

            outColor.rgb += texture(EnvironmentLUT, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    outColor.rgb = PI * outColor.rgb * (1.0 / float(nrSamples));
}