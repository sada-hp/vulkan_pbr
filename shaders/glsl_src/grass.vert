#version 460
#include "ubo.glsl"
#include "noise.glsl"
#include "common.glsl"
#include "constants.glsl"

layout(push_constant) uniform constants
{
    layout(offset = 0) float Lod;
} In;

struct Vertex
{
    vec4 Position;
    vec4 World;
    vec4 UV;
};

vec4 Vertices[11] = {
    vec4( -1.0,  0.0, 0.0, 1.0),
    vec4(  1.0,  0.0, 0.0, 1.0),
    vec4(  1.0,  1.0, 0.0, 1.0),
    vec4( -1.0,  1.0, 0.0, 1.0),
    vec4(  1.0,  3.0, 0.0, 1.0),
    vec4( -1.0,  3.0, 0.0, 1.0),
    vec4( 0.75,  6.0, 0.0, 1.0),
    vec4(-0.75,  6.0, 0.0, 1.0),
    vec4( 0.45,  8.0, 0.0, 1.0),
    vec4(-0.45,  8.0, 0.0, 1.0),
    vec4(  0.0, 10.0, 0.0, 1.0)
};

int indices_l0[27] = {
    0,1,2,
    0,2,3,
    3,2,4,
    3,4,5,
    5,4,6,
    5,6,7,
    7,6,8,
    7,8,9,
    9,8,10
};

int indices_l1[15] = {
    0,1,4,
    0,4,5,
    5,4,8,
    5,8,9,
    9,8,10
};

int indices_l2[3] = {
    0,1,10
};

layout (constant_id = 2) const float Scale = 1.f;
layout (constant_id = 3) const float MinHeight = 1.f;
layout (constant_id = 4) const float MaxHeight = 1.f;
layout (constant_id = 5) const float deltaS = 0;

layout(set = 1, binding = 0) uniform sampler2DArray NoiseMap;
layout (std140, set = 1, binding = 1) readonly buffer TerrainVertex{
	Vertex at[];
} vertices;

layout(location = 0) out vec4 CenterPosition;
layout(location = 1) out vec4 WorldPosition;
layout(location = 2) out float AO;
layout(location = 3) out vec3 Normal;

void main()
{
    ivec2 noiseSize = ivec2(textureSize(NoiseMap, 0));
    Vertex vert = vertices.at[gl_BaseInstance + gl_InstanceIndex % (noiseSize.x * noiseSize.y)];

    WorldPosition = vec4(0.0, 0.0, 0.0, 1.0);

    float hFrac = vert.World.w;
    float sampleScale = Scale * exp2(vert.Position.y);
    vec3 ObjectCenterNorm = vert.World.xyz;

    dmat3 Orientation = GetTerrainOrientation();

    dvec3 Camera = ubo.WorldUp.xyz;
    vec4 dH = textureGather(NoiseMap, vec3(vert.UV.xy, vert.Position.y), 0);
    vec3 Normal = vec3(normalize(Orientation * vec3(dH.w - dH.z, sampleScale, dH.w - dH.x)));
    vec3 ObjectCenter = vert.World.xyz * (Rg + dH.w);

    if (hFrac < 0.01 || saturate(10.0 * (1.0 - saturate(abs(dot(Normal, ObjectCenterNorm))))) > 0.5)
    {
        WorldPosition = vec4(0.0, 0.0, -1.0, 1.0);
    }
    else
    {
        float a1 = abs(dot(ObjectCenterNorm, vec3(0, 1, 0)));
        float a2 = abs(dot(ObjectCenterNorm, vec3(0, 0, 1)));
        float a3 = abs(dot(ObjectCenterNorm, vec3(1, 0, 0)));

        float anim = ubo.Time * 0.005;
        vec2 hash = a1 * noise2((ObjectCenter.xz + 1e3 * Scale * ObjectCenterNorm.xz * gl_InstanceIndex) * 1e-3)
            + a2 * noise2((ObjectCenter.xy + 1e3 * Scale * ObjectCenterNorm.xy * gl_InstanceIndex) * 1e-3)
            + a3 * noise2((ObjectCenter.yz + 1e3 * Scale * ObjectCenterNorm.yz * gl_InstanceIndex) * 1e-3);

        vec3 LocalPosition;
        if (In.Lod == 0)
        {
            LocalPosition = Vertices[indices_l0[gl_VertexIndex]].xyz;
        }
        else if (In.Lod == 1)
        {
            LocalPosition = Vertices[indices_l1[gl_VertexIndex]].xyz;
        }
        else
        {
            LocalPosition = Vertices[indices_l2[gl_VertexIndex]].xyz;
        }

        AO = saturate(LocalPosition.y / 10.0);
        LocalPosition *= vec2(6.0, 10.0).xyx;
        LocalPosition += sampleScale * vec3(hash.x, 0.0, hash.y);

        float lean = HALF_PI * smootherstep(0.0, 1.0, AO) * (a1 * perlin(5.0 * ObjectCenterNorm.xz + anim, 100.0) + a2 * perlin(5.0 * ObjectCenterNorm.xy + anim, 100.0) + a3 * perlin(5.0 * ObjectCenterNorm.yz + anim, 100.0));
        mat3 RotX = mat3(vec3(1.0, 0.0, 0.0), 
            vec3(0.0, cos(lean), -sin(lean)), 
            vec3(0.0, sin(lean), cos(lean))
        );

        float bank = 0.5 * (a1 * noise(ObjectCenter.xz) + a2 * noise(ObjectCenter.xy) + a3 * noise(ObjectCenter.yz));
        mat3 RotZ = mat3(vec3(cos(bank), sin(bank), 0.0), vec3(-sin(bank), cos(bank), 0.0), vec3(0.0, 0.0, 1.0));

        float face = TWO_PI * bank;
        mat3 RotY = mat3(vec3(cos(face), 0.0, -sin(face)), 
            vec3(0.0, 1.0, 0.0), 
            vec3(sin(face), 0.0, cos(face))
        );

        mat3 Rot = mat3(Orientation * RotX * RotZ * RotY);
        WorldPosition = vec4(Rot * LocalPosition + ObjectCenter, 1.0);
    }

    gl_Position = vec4(ubo.ViewProjectionMatrix * WorldPosition);
    CenterPosition.xyz = ObjectCenter - ubo.CameraPosition.xyz;
    WorldPosition.xyz -= ubo.CameraPosition.xyz;
    Normal = ObjectCenterNorm;
}