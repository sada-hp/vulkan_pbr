#version 460
#include "ubo.glsl"
#include "brdf.glsl"
#include "hextiling.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outDeferred;

layout(location = 0) in flat vec4 CenterPosition;
layout(location = 1) in vec4 WorldPosition;
layout(location = 2) in float AO;
layout(location = 3) in vec3 Normal;

layout(set = 2, binding = 0) uniform sampler2DArray AlbedoMap;
layout(set = 2, binding = 1) uniform sampler2DArray NormalHeightMap;
layout(set = 2, binding = 2) uniform sampler2DArray ARMMap;

void main()
{
    vec3 dX = dFdxFine(WorldPosition.xyz + ubo.CameraPosition.xyz);
    vec3 dY = dFdyFine(WorldPosition.xyz + ubo.CameraPosition.xyz);
    vec3 N = normalize(cross(dY.xyz, dX.xyz));

    HexParams Tiling;
    GetHexParams((ubo.CameraPosition + CenterPosition).xz * 5e-4, Tiling);

    vec4 c1 = textureGrad(AlbedoMap, vec3(Tiling.st1, 0), Tiling.dSTdx, Tiling.dSTdy);
    vec4 c2 = textureGrad(AlbedoMap, vec3(Tiling.st2, 0), Tiling.dSTdx, Tiling.dSTdy);
    vec4 c3 = textureGrad(AlbedoMap, vec3(Tiling.st3, 0), Tiling.dSTdx, Tiling.dSTdy);

    vec3 Lw = vec3(0.299, 0.587, 0.114);
    vec3 Dw = vec3(dot(c1.xyz, Lw), dot(c2.xyz, Lw), dot(c3.xyz, Lw));

    Dw = mix(vec3(1.0), Dw, vec3(0.6));
    vec3 W = Dw * pow(vec3(Tiling.w1, Tiling.w2, Tiling.w3), vec3(7));

    W /= (W.x + W.y + W.z);

    vec3 avgAlbedo = W.x * c1.xyz + W.y * c2.xyz + W.z * c3.xyz;

    outColor = vec4(mix(avgAlbedo, vec3(0.0, 0.25, 0.0), 0.15), 1.0);
    outNormal = vec4(N, 1.0);
    outDeferred = vec4(AO, 0.85, 0.0, 0.1);
}