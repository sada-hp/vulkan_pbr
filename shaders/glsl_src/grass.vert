#version 460
#include "ubo.glsl"
#include "noise.glsl"
#include "common.glsl"

struct Vertex
{
    vec4 Position;
    vec4 UV;
};

vec4 Vertices[7] = {
    vec4(-0.5,0,0,1),
    vec4(0.5,0,0,1),
    vec4(0.5,4,0,1),
    vec4(-0.5,4,0,1),
    vec4(-0.25,6,0,1),
    vec4(0.25,6,0,1),
    vec4(0,7,0,1)
};

int indices[15] = {
    0,1,2,
    3,2,0,
    5,3,2,
    4,3,5,
    6,4,5
};

layout (constant_id = 0) const float Rg = 6360.0 * 1e3;
layout (constant_id = 1) const float Rt = 6420.0 * 1e3;
layout (constant_id = 2) const float Scale = 1.f;
layout (constant_id = 3) const float MinHeight = 1.f;
layout (constant_id = 4) const float MaxHeight = 1.f;
layout (constant_id = 5) const float deltaS = 0;

layout(set = 1, binding = 0) uniform sampler2DArray NoiseMap;
layout (std140, set = 1, binding = 1) readonly buffer TerrainVertex{
	Vertex at[];
} vertices;

layout(location = 0) out flat vec4 CenterPosition;
layout(location = 1) out vec4 WorldPosition;
layout(location = 2) out float AO;
layout(location = 3) out vec3 Normal;

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
    ivec2 noiseSize = ivec2(textureSize(NoiseMap, 0));
    Vertex vert = vertices.at[gl_BaseInstance + gl_InstanceIndex % (noiseSize.x * noiseSize.y)];

    ivec3 texelId = ivec3(round(vert.UV.xy * (noiseSize - 1)), vert.Position.y);

    float Height = texelFetch(NoiseMap, texelId, 0).r;
    WorldPosition = vec4(0.0, 0.0, 0.0, 1.0);

    float hFrac = (Height - MinHeight) / (MaxHeight - MinHeight);
    if (hFrac > 0.01)
    {
        dvec3 Camera = ubo.WorldUp.xyz;
        dmat3 Orientation = GetTerrainOrientation(Camera);
        dvec3 Center = round(RoundToIncrement(Camera * Rg, Scale * exp2(max(vert.Position.y, deltaS))));
        vec3 ObjectPosition = vec3(Orientation * vec3(vert.Position.x, Height, vert.Position.z) + Center);
        vec3 ObjectCenterNorm = normalize(ObjectPosition);
        vec3 ObjectCenter = ObjectCenterNorm * (Rg + Height);

        AO = Vertices[indices[gl_VertexIndex]].y / 6.0;

        float anim = gl_InstanceIndex + ubo.Time * 1e-1;

        vec3 hash = noise3(ObjectCenter + ObjectCenterNorm * gl_InstanceIndex);
        vec3 hash2 = vec3(perlin(0.2 * ObjectCenter * 0.5 + anim, 5), perlin(0.2 * ObjectCenter + anim, 5), perlin(ObjectCenter * 0.05 + anim, 5));

        mat3 Rot = mat3(1.0);
        Rot[0] = normalize(hash);
        Rot[1] = normalize(0.001 + mix(vec3(0.0, 1.0, 0.0), hash2 * vec3(1.0, 2.0, 1.0), smootherstep(0.0, 1.5, AO)));
        Rot[2] = normalize(cross(Rot[0], Rot[1]));
        Rot[0] = normalize(cross(Rot[1], Rot[2]));

        WorldPosition = vec4((Rot * ((saturate(abs(hash) + 0.35) * vec2(gl_BaseInstance == 0 ? 5.0 : 10.0, 10.0).xyx) * (Vertices[indices[gl_VertexIndex]].xyz + Scale * vec3(hash.x, 0.0, hash.z)))) + ObjectCenter, 1.0);

#if 1
        ivec3 texelIdD = clamp_coords(texelId + ivec3(0, 1, 0));
        float d = texelFetch(NoiseMap, texelIdD, 0).r;

        ivec3 texelIdR = clamp_coords(texelId + ivec3(1, 0, 0));
        float r = texelFetch(NoiseMap, texelIdR, 0).r;

        vec3 B = vec3(Orientation * vec3(vert.Position.x + Scale * exp2(texelIdR.z), r, vert.Position.z) + Center);
        vec3 C = vec3(Orientation * vec3(vert.Position.x, d, vert.Position.z + Scale * exp2(texelIdD.z)) + Center);

        vec3 AC = C - ObjectPosition;
        vec3 AB = B - ObjectPosition;

        vec3 Normal = normalize(cross(AC, AB));

        if (saturate(10.0 * (1.0 - saturate(abs(dot(Normal, normalize(WorldPosition.xyz)))))) > 0.5)
        {
            WorldPosition = vec4(0.0, 0.0, 0.0, 1.0);
        }
#endif

        gl_Position = vec4(ubo.ViewProjectionMatrix * WorldPosition);
        CenterPosition.xyz = (Scale * 0.5 * vec3(hash.x, 0.0, hash.z) + ObjectCenter) - ubo.CameraPosition.xyz;
        WorldPosition.xyz -= ubo.CameraPosition.xyz;
        Normal = ObjectCenterNorm;
    }
    else
    {
        gl_Position = vec4(-1000.0);
    }
}