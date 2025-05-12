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
layout(location = 1) in vec4 worldPosition;
layout(location = 2) in vec4 vertUV;

layout(location = 0) out vec4 WorldPosition;
layout(location = 1) out vec2 VertPosition;
layout(location = 2) out flat float Diameter;
layout(location = 3) out flat float Diameter2;
layout(location = 4) out float Height;
layout(location = 5) out flat int Level;
layout(location = 6) out mat3 TBN;

layout(set = 2, binding = 0) uniform sampler2DArray NoiseMap;

void GetTBN(float vertexScale)
{
    dvec3 Camera = ubo.WorldUp.xyz;
    float sampleScale = vertexScale * exp2(vertPosition.y);
    dmat3 Orientation = GetTerrainOrientation();
    vec4 dH = textureGather(NoiseMap, vec3(vertUV.xy, vertPosition.y), 0);

    vec3 Normal = normalize(vec3(dH.w - dH.z, sampleScale, dH.w - dH.x));

    vec2 dUV = vec2(sampleScale / vertUV.w, 0.0);
    float f = 1.0 / (dUV.x * dUV.x);

    vec3 Tangent;
    Tangent.x = f * dUV.x * sampleScale;
    Tangent.y = f * dUV.x * (dH.z - dH.w);
    Tangent.z = 0.0;

    Tangent = normalize(Tangent);
    vec3 Bitangent = normalize(cross(Normal, Tangent));
    TBN = mat3(Orientation * mat3(Tangent, Bitangent, Normal));
}

void main()
{
    NOISE_SEED = Seed;
    Level = int(vertPosition.y);

    float vertexScale = Level == 0 ? 2.0 * Scale : Scale;
    vertexScale = vertexScale * (ubo.CameraRadius - Rg > MaxHeight ? exp2(mix(5.0, 0.0, saturate(float(MaxHeight - MinHeight) / float(ubo.CameraRadius - Rg)))) : 1.0);
    
    GetTBN(vertexScale);
    VertPosition = vertexScale * vertPosition.xz;
    Diameter = vertexScale * vertUV.w;
    Diameter2 = vertexScale * vertUV.z;

    WorldPosition = vec4(worldPosition.xyz * (Rg + MinHeight + worldPosition.w * (MaxHeight - MinHeight)), 1.0);

    gl_Position = vec4(ubo.ViewProjectionMatrix * WorldPosition);
    Height = Seed == 0 ? 1.0 : worldPosition.w;
}