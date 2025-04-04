#version 460
#include "ubo.glsl"
#include "lighting.glsl"

layout(binding = 1) uniform sampler2D TransmittanceLUT;
layout(binding = 2) uniform sampler2D IrradianceLUT;
layout(binding = 3) uniform sampler3D InscatteringLUT;

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec4 vertPosition;

void main()
{
    outColor = vec4(MaxLightIntensity * SkyScattering(TransmittanceLUT, InscatteringLUT, ubo.CameraPosition.xyz, normalize(vertPosition.xyz), ubo.SunDirection.xyz), 1.0);
}