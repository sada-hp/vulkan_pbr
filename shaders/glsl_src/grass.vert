#version 460
#include "ubo.glsl"
#include "noise.glsl"
#include "common.glsl"

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
    vec4( -1.0, 0.0, 0.0, 1.0),
    vec4(  1.0, 0.0, 0.0, 1.0),
    vec4(  1.0, 1.0, 0.0, 1.0),
    vec4( -1.0, 1.0, 0.0, 1.0),
    vec4(  1.0, 3.0, 0.0, 1.0),
    vec4( -1.0, 3.0, 0.0, 1.0),
    vec4( 0.75, 6.0, 0.0, 1.0),
    vec4(-0.75, 6.0, 0.0, 1.0),
    vec4( 0.45, 8.0, 0.0, 1.0),
    vec4(-0.45, 8.0, 0.0, 1.0),
    vec4( 0.00, 10.0, 0.0, 1.0)
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
    if (hFrac > 0.01)
    {
        vec3 ObjectCenter = vert.World.xyz;
        vec3 ObjectCenterNorm = normalize(ObjectCenter);

        float anim = gl_InstanceIndex + ubo.Time * 1e-1;
        vec3 hash = noise3(ObjectCenter + ObjectCenterNorm * gl_InstanceIndex);
        
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

        AO = saturate(LocalPosition.y / 6.0);
        LocalPosition *= (saturate(abs(hash) + 0.35) * vec2(6.0, 10.0).xyx);

        float p1;
        vec2 d1, d2;
        perlind(ObjectCenter.xz, 10, p1, d1, d2);

        float lean = max(AO, 0.25) * perlin(vec2(p1, dot(d1, d2)) + anim, 10);
        mat3 RotX = inverse(mat3(vec3(1.0, 0.0, 0.0), 
                    vec3(0.0, cos(lean), -sin(lean)), 
                    vec3(0.0, sin(lean), cos(lean))
        ));

        float bank = noise(d1);
        mat3 RotZ = inverse(mat3(vec3(cos(bank), sin(bank), 0.0), 
                    vec3(-sin(bank), cos(bank), 0.0), 
                    vec3(0.0, 0.0, 1.0)
        ));

        float face = noise(d2);
        mat3 RotY = inverse(mat3(vec3(cos(face), 0.0, -sin(face)), 
                    vec3(0.0, 1.0, 0.0), 
                    vec3(sin(face), 0.0, cos(face))
        ));

        mat3 Rot = RotZ * RotX * RotY;
        WorldPosition = vec4((Rot * (LocalPosition + Scale * exp2(vert.Position.y) * vec3(hash.x, 0.0, hash.z))) + ObjectCenter, 1.0);

#if 1
        dvec3 Camera = ubo.WorldUp.xyz;
        float sampleScale = Scale * exp2(vert.Position.y);
        dmat3 Orientation = GetTerrainOrientation(Camera);
        vec4 dH = textureGather(NoiseMap, vec3(vert.UV.xy, vert.Position.y), 0);
        vec3 Normal = normalize(vec3(dH.w - dH.z, sampleScale, dH.w - dH.x));

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