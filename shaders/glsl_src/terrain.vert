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
 
    TBN = mat3(vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 0.0));

    WorldPosition = vec4(worldPosition.xyz, 1.0);
    gl_Position = vec4(ubo.ViewProjectionMatrix * WorldPosition);
    WorldPosition.xyz -= ubo.CameraPosition.xyz;

    Height = Seed == 0 ? 1.0 : worldPosition.w;
}