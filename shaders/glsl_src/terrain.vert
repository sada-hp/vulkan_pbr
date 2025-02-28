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

layout(set = 1, binding = 4) uniform sampler2DArray NoiseMap;

ivec3 clamp_coords(int x, int y, int level)
{
    ivec3 texel = ivec3(x, y, level);
    ivec2 size = textureSize(NoiseMap, 0).xy;

    // if texel is outside the bounds of current level    
    if (x >= size.x || y >= size.y || x < 0 || y < 0)
    {
        texel.z += 1;
        texel.x = 33 + (x < 0 ? x : int(ceil(float(x) / 2)));
        texel.y = 33 + (y < 0 ? y : int(ceil(float(y) / 2)));
    }
    else if (texel.z > 0 && texel.x >= 33 && texel.x <= 99 && texel.y >= 33 && texel.y <= 99)
    {
        texel.z -= 1;
        texel.x = 2 * (texel.x - 33);
        texel.y = 2 * (texel.y - 33);
    }

    return texel;
}

void main()
{
    Level = int(vertPosition.y);

    ivec2 noiseSize = ivec2(textureSize(NoiseMap, 0));
    UV = vertUV.xy;
    ivec3 texelId = ivec3(round(UV * (noiseSize - 1)), Level);

    Height = RoundToIncrement(floor(texelFetch(NoiseMap, texelId, 0).r), 2.5f);

#if 1
        dmat3 Orientation = GetTerrainOrientation();
        dvec3 Center = RoundToIncrement(ubo.CameraPositionFP64.xyz, Scale * exp2(Level));
#else
        dmat3 Orientation;
        Orientation[0] = vec3(1.0, 0.0, 0.0);
        Orientation[1] = normalize(dvec3(0.0, Rg, 0.0));
        Orientation[2] = normalize(cross(Orientation[0], Orientation[1]));
        Orientation[0] = normalize(cross(Orientation[1], Orientation[2]));
        dvec3 Center = dvec3(0.0, Rg, 0.0);
#endif

    float u = texelFetch(NoiseMap, clamp_coords(texelId.x, texelId.y - 1, Level), 0).x;
    float d = texelFetch(NoiseMap, clamp_coords(texelId.x, texelId.y + 1, Level), 0).x;
    float r = texelFetch(NoiseMap, clamp_coords(texelId.x + 1, texelId.y, Level), 0).x;
    float l = texelFetch(NoiseMap, clamp_coords(texelId.x - 1, texelId.y, Level), 0).x;

    Normal.z = u - d;
    Normal.x = l - r;
    Normal.y = Height;
    Normal = vec3(normalize(Normal));

    vec3 ObjectPosition = vec3(Orientation * vec3(vertPosition.x, 0.0, vertPosition.z) + Center);
    WorldPosition = vec4(vec3(normalize(ObjectPosition)) * (Rg + Height), 1.0);

    gl_Position = vec4(ubo.ViewProjectionMatrix * WorldPosition);
    WorldPosition.xyz -= ubo.CameraPosition.xyz;
}