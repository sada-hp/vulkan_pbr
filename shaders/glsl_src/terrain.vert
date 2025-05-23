#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "noise.glsl"

layout (constant_id = 2) const float Scale = 1.f;
layout (constant_id = 3) const float MinHeight = 1.f;
layout (constant_id = 4) const float MaxHeight = 1.f;
layout (constant_id = 5) const uint Seed = 0;
layout (constant_id = 6) const float deltaS = 0;

layout(push_constant) uniform constants
{
    layout(offset = 0) dvec3 Offset;
    layout(offset = 32) mat3x4 Orientation;
} PushConstants;

layout(location = 0) in vec4 vertPosition;
layout(location = 1) in vec4 vertUV;

layout(location = 0) out vec4 WorldPosition;
layout(location = 1) out vec4 NormalData;
layout(location = 2) out float Height;

layout(set = 1, binding = 0) uniform sampler2DArray NoiseMap;

void GetNormalData(float vertexScale, inout vec4 dH)
{
    float Level = vertPosition.w;

    if (vertUV.x > 0.9 || vertUV.y > 0.9)
    {
        float f = smootherstep(0.0, 1.0, (max(vertUV.x, vertUV.y) - 0.9) / 0.1);
        dH = mix(textureGather(NoiseMap, vec3(vertUV.xy, Level), 0), textureGather(NoiseMap, vec3(vertUV.xy * 0.5 + 0.25, Level + 1), 0), f);
        vertexScale = mix(vertexScale, 2.0 * vertexScale, f);
    }
    else if (vertUV.x < 0.1 || vertUV.y < 0.1)
    {
        float f = smootherstep(0.0, 1.0, 1.0 - min(vertUV.x, vertUV.y) / 0.1);
        dH = mix(textureGather(NoiseMap, vec3(vertUV.xy, Level), 0), textureGather(NoiseMap, vec3(vertUV.xy * 0.5 + 0.25, Level + 1), 0), f);
        vertexScale = mix(vertexScale, 2.0 * vertexScale, f);
    }
    else
    {
        dH = textureGather(NoiseMap, vec3(vertUV.xy, Level), 0);
    }

    float sampleScale = vertexScale * exp2(Level);
    NormalData = vec4(dH.w - dH.z, dH.w - dH.x, vertUV.w, sampleScale);
    Height = Seed == 0 ? 0.5 : (dH.w - MinHeight) / (MaxHeight - MinHeight);
}

void main()
{
    vec4 dH;
    float vertexScale = Scale * (ubo.CameraRadius - Rg > MaxHeight ? exp2(mix(5.0, 0.0, saturate(float(MaxHeight - MinHeight) / float(ubo.CameraRadius - Rg)))) : 1.0);
    GetNormalData(vertexScale, dH);
    
    WorldPosition = vec4(vertPosition.xyz, 1.0);
    gl_Position = vec4(ubo.ViewProjectionMatrix * WorldPosition);
}