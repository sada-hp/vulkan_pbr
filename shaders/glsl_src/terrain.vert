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
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec2 UV;
layout(location = 3) out float Height;
layout(location = 4) out flat int Level;

layout(set = 2, binding = 0) uniform sampler2DArray NoiseMap;

ivec3 clamp_coords(int x, int y, int level)
{
    ivec3 texel = ivec3(x, y, level);
    ivec2 size = textureSize(NoiseMap, 0).xy;
    ivec2 onefourth = size / 4;
    ivec2 threefourth = 3 * onefourth;

    // if texel is outside the bounds of current level    
    if (x >= size.x || y >= size.y || x < 0 || y < 0)
    {
        texel.z += 1;
        texel.x = onefourth.x + (x < 0 ? x : int(ceil(float(x) / 2)));
        texel.y = onefourth.y + (y < 0 ? y : int(ceil(float(y) / 2)));
    }
    else if (texel.z > 0 && texel.x >= onefourth.x && texel.x <= threefourth.x && texel.y >= onefourth.y && texel.y <= threefourth.y)
    {
        texel.z -= 1;
        texel.xy = 2 * (texel.xy - onefourth);
    }

    return texel;
}

void main()
{
    NOISE_SEED = Seed;
    Level = int(vertPosition.y);

    ivec2 noiseSize = ivec2(textureSize(NoiseMap, 0));
    UV = vertUV.xy;
    ivec3 texelId = ivec3(round(UV * (noiseSize - 1)), Level);

    Height = texelFetch(NoiseMap, texelId, 0).r;

    dvec3 Camera = normalize(round(ubo.CameraPositionFP64.xyz));
    dmat3 Orientation = GetTerrainOrientation(Camera);
    dvec3 Center = round(RoundToIncrement(Camera * Rg, Scale * exp2(Level)));

    ivec3 texelIdU = clamp_coords(texelId.x, texelId.y - 1, Level);
    float u = texelFetch(NoiseMap, texelIdU, 0).r;

    ivec3 texelIdD = clamp_coords(texelId.x, texelId.y + 1, Level);
    float d = texelFetch(NoiseMap, clamp_coords(texelId.x, texelId.y + 1, Level), 0).r;

    ivec3 texelIdR = clamp_coords(texelId.x + 1, texelId.y, Level);
    float r = texelFetch(NoiseMap, clamp_coords(texelId.x + 1, texelId.y, Level), 0).r;

    ivec3 texelIdL = clamp_coords(texelId.x - 1, texelId.y, Level);
    float l = texelFetch(NoiseMap, clamp_coords(texelId.x - 1, texelId.y, Level), 0).r;

    Normal.x = (l - r) / exp2(max(texelIdR.z, texelIdL.z));
    Normal.z = (u - d) / exp2(max(texelIdD.z, texelIdU.z));
    Normal.y = 2.0 * Scale;
    Normal = vec3(Orientation * normalize(Normal));

    vec3 ObjectPosition = vec3(Orientation * vec3(vertPosition.x, 0.0, vertPosition.z) + Center);

    WorldPosition = vec4(vec3(normalize(ObjectPosition)) * (Rg + Height), 1.0);
    gl_Position = vec4(ubo.ViewProjectionMatrix * WorldPosition);
    WorldPosition.xyz -= ubo.CameraPosition.xyz;

    Height = Seed == 0 ? 1.0 : (Height - MinHeight) / (MaxHeight - MinHeight);
}