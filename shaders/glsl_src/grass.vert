#version 460
#include "ubo.glsl"
#include "noise.glsl"
#include "common.glsl"
#include "constants.glsl"

struct Vertex
{
    vec4 Position;
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

int indices[45] = {
    // lod 0
    0,1,2, 0,2,3,
    3,2,4, 3,4,5,
    5,4,6, 5,6,7,
    7,6,8, 7,8,9,
    9,8,10,
    // lod 1
    0,1,4, 0,4,5,
    5,4,8, 5,8,9,
    9,8,10,
    // lod 2
    0,1,10
};

layout (constant_id = 2) const float Scale = 1.f;
layout (constant_id = 3) const float MinHeight = 1.f;
layout (constant_id = 4) const float MaxHeight = 1.f;
layout (constant_id = 5) const float deltaS = 0;
layout (constant_id = 6) const float Seed = 0;

layout(set = 1, binding = 0) uniform sampler2DArray NoiseMap;
layout (std140, set = 1, binding = 1) readonly buffer TerrainVertex{
	Vertex at[];
} vertices;
layout (std140, set = 1, binding = 2) readonly buffer PositionsBuffer
{
	ivec4 at[];
} positions;
layout(set = 1, binding = 3) uniform sampler2D DepthMap;

layout(location = 0) out vec4 ObjectCenter;
layout(location = 1) out vec4 FragNormal;
layout(location = 2) out vec3 SlopeNormal;

void main()
{
    ivec2 size = ivec2(textureSize(NoiseMap, 0));
    Vertex vert = vertices.at[positions.at[gl_InstanceIndex / 4][gl_InstanceIndex % 4]];
    float Level = vert.Position.w; // refine

    vec4 dH = textureGather(NoiseMap, vec3(vert.UV.xy, Level), 0);
    float Height = (dH.w - MinHeight) / (MaxHeight - MinHeight);

    float sampleScale = Scale * exp2(Level);
    vec3 ObjectCenterNorm = vert.Position.xyz / (Rg + dH.w);

    SlopeNormal = vec3(normalize(mat3(ubo.PlanetMatrix) * vec3(dH.w - dH.z, sampleScale, dH.w - dH.x)));
    ObjectCenter = vec4(vert.Position.xyz, Height);

    vec2 WorldUV = vec2(0.0), UVdir = vec2(0.0);
    if (abs(ObjectCenter.y) > abs(ObjectCenter.x) && abs(ObjectCenter.y) > abs(ObjectCenter.z))
    {
        WorldUV = ObjectCenter.xz;
        UVdir = ObjectCenterNorm.xz;
    }
    else if (abs(ObjectCenter.z) > abs(ObjectCenter.x) && abs(ObjectCenter.z) > abs(ObjectCenter.y))
    {
        WorldUV = ObjectCenter.xy;
        UVdir = ObjectCenterNorm.xy;
    }
    else
    {
        WorldUV = ObjectCenter.yz;
        UVdir = ObjectCenterNorm.yz;
    }

    mat3 Rot = mat3(ubo.PlanetMatrix);
    float anim = ubo.Wind * ubo.Time * 0.01;
    vec2 hash = noise2(WorldUV);

    ObjectCenter.xyz = ObjectCenter.xyz + Rot * 0.5 * sampleScale * vec3(hash.x, 0.0, hash.y);
    vec4 CenterUV = vec4(ubo.ViewProjectionMatrix * vec4(ObjectCenter.xyz, 1.0));
    CenterUV.xyz /= CenterUV.w;
    CenterUV.xy = saturate(CenterUV.xy * 0.5 + 0.5);
    float depth = texture(DepthMap, CenterUV.xy).r;

    if (depth > CenterUV.z || Height < 0.0096 || (Height < 0.01 && max(abs(hash.x), abs(hash.y)) > saturate(Height - 0.0096) / (0.01 - 0.0096)) || Height > 0.53 || saturate(10.0 * (1.0 - saturate(abs(dot(SlopeNormal, ObjectCenterNorm))))) > 0.45)
    {
        gl_Position = vec4(-2.0);
    }
    else
    {
        vec3 LocalPosition = Vertices[indices[gl_VertexIndex - gl_BaseVertex + (gl_DrawID == 2 || ubo.CameraRadius > Rg + MaxHeight ? 42 : (gl_DrawID == 1 ? 27 : 0))]].xyz;
        float AO = saturate(0.1 + LocalPosition.y / 10.0);
        
        LocalPosition = LocalPosition * vec2(5.0, 10.0).xyx;

        float wind = HALF_PI * smootherstep(0.0, 1.0, AO) * perlin(5.0 * UVdir + anim, 100.0);
        float lean = 0.25 * smootherstep(0.0, 1.0, AO) * hash.y;
        float bank = perlin(15.0 * -UVdir + anim, 200.0);
        float face = TWO_PI * (1.0 - 2.0 * worley(WorldUV, 25.0));

        vec4 windTrig = vec4(cos(wind), sin(wind), 0.0, 0.0);
        vec4 leanTrig = vec4(cos(lean), sin(lean), 0.0, 0.0);
        vec4 bankTrig = vec4(cos(bank), sin(bank), 0.0, 0.0);
        vec4 faceTrig = vec4(cos(face), sin(face), 0.0, 0.0);

        windTrig.z = -windTrig.y;
        leanTrig.z = -leanTrig.y;
        bankTrig.z = -bankTrig.y;
        faceTrig.z = -faceTrig.y;

        mat3 RotWind = mat3(vec3(1.0, 0.0, 0.0), windTrig.wxz, windTrig.wyx);
        mat3 RotLean = mat3(vec3(1.0, 0.0, 0.0), leanTrig.wxz,  leanTrig.wyx);
        mat3 RotSide = mat3(vec3(bankTrig.xyw),  bankTrig.zxw, vec3(0.0, 0.0, 1.0));
        mat3 RotYaw = mat3(faceTrig.xwz, vec3(0.0, 1.0, 0.0), faceTrig.ywx);
        mat3 Rot = Rot * RotWind * RotSide * RotYaw * RotLean;

        FragNormal = vec4(Rot * normalize(vec3(0.0, 0.0, 1.0)), AO);
        gl_Position = vec4(ubo.ViewProjectionMatrix * vec4(Rot * LocalPosition + ObjectCenter.xyz, 1.0));
    }
}