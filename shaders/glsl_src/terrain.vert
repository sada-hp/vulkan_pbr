#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "noise.glsl"

layout (constant_id = 2) const float Scale = 1.f;
layout (constant_id = 3) const float MinHeight = 1.f;
layout (constant_id = 4) const float MaxHeight = 1.f;
layout (constant_id = 5) const uint Seed = 0;

layout(push_constant) uniform constants
{
    layout(offset = 0) dvec3 Offset;
    layout(offset = 32) mat3x4 Orientation;
} PushConstants;

layout(location = 0) in vec4 vertPosition;
layout(location = 1) in vec4 vertUV;

layout(location = 0) out vec4 WorldPosition;
layout(location = 1) out vec2 VertPosition;
layout(location = 2) out flat float Diameter;
layout(location = 3) out float Height;
layout(location = 4) out flat int Level;
layout(location = 5) out mat3 TBN;

layout(set = 2, binding = 0) uniform sampler2DArray NoiseMap;

ivec3 clamp_coords(ivec3 texel)
{
    ivec2 size = textureSize(NoiseMap, 0).xy;
    ivec2 onefourth = size / 4;
    ivec2 threefourth = 3 * onefourth;

    // if texel is outside the bounds of current level    
    if (texel.x >= size.x || texel.y >= size.y || texel.x < 0 || texel.y < 0)
    {
        texel.z += 1;
        texel.x = onefourth.x + (texel.x < 0 ? texel.x : int(ceil(float(texel.x) / 2)));
        texel.y = onefourth.y + (texel.y < 0 ? texel.y : int(ceil(float(texel.y) / 2)));
    }

    return texel;
}

void main()
{
    NOISE_SEED = Seed;
    Level = int(vertPosition.y);

    ivec2 noiseSize = ivec2(textureSize(NoiseMap, 0));
    VertPosition = vertPosition.xz;
    Diameter = vertUV.w;
    ivec3 texelId = ivec3(round(vertUV.xy * (noiseSize - 1)), Level);

    Height = texelFetch(NoiseMap, texelId, 0).r;

    dvec3 Camera = normalize(round(ubo.CameraPositionFP64.xyz));
    dmat3 Orientation = GetTerrainOrientation(Camera);
    dvec3 Center = round(RoundToIncrement(Camera * Rg, Scale * exp2(Level)));

    ivec3 texelIdD = clamp_coords(texelId + ivec3(0, 1, 0));
    float d = texelFetch(NoiseMap, texelIdD, 0).r;

    ivec3 texelIdR = clamp_coords(texelId + ivec3(1, 0, 0));
    float r = texelFetch(NoiseMap, texelIdR, 0).r;

    vec3 A = vec3(Orientation * vec3(vertPosition.x, Height, vertPosition.z) + Center);
    vec3 B = vec3(Orientation * vec3(vertPosition.x + Scale * exp2(texelIdR.z), r, vertPosition.z) + Center);
    vec3 C = vec3(Orientation * vec3(vertPosition.x, d, vertPosition.z + Scale * exp2(texelIdD.z)) + Center);

    vec3 AC = C - A;
    vec3 AB = B - A;

    vec3 Normal = normalize(cross(AC, AB));

    vec2 Auv = vec2(vertPosition.x, vertPosition.z) / vertUV.w;
    vec2 Buv = vec2(vertPosition.x + Scale * exp2(Level), vertPosition.z) / vertUV.w;
    vec2 Cuv = vec2(vertPosition.x, vertPosition.z + Scale * exp2(Level)) / vertUV.w;
    Auv.y = 1.0 - Auv.y;
    Buv.y = 1.0 - Buv.y;
    Cuv.y = 1.0 - Cuv.y;
    vec2 delta1 = Buv - Auv;
    vec2 delta2 = Cuv - Auv;

    float f = 1.0 / (delta1.x * delta2.y - delta1.y * delta2.x);

    vec3 Tangent;
    Tangent.x = f * (delta2.y * AB.x - delta1.y * AC.x);
    Tangent.y = f * (delta2.y * AB.y - delta1.y * AC.y);
    Tangent.z = f * (delta2.y * AB.z - delta1.y * AC.z);

    Tangent = normalize(Tangent);
    vec3 Bitangent = normalize(cross(Normal, Tangent));
    TBN = mat3(Tangent, Bitangent, Normal);

    WorldPosition = vec4(vec3(normalize(A)) * (Rg + Height), 1.0);
    gl_Position = vec4(ubo.ViewProjectionMatrix * WorldPosition);
    WorldPosition.xyz -= ubo.CameraPosition.xyz;

    Height = Seed == 0 ? 1.0 : (Height - MinHeight) / (MaxHeight - MinHeight);
}