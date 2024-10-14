#version 460
#include "ubo.glsl"
#include "lighting.glsl"

layout(push_constant) uniform constants
{
    layout(offset = 0) dvec3 Offset;
    layout(offset = 32) mat3x4 Orientation;
} PushConstants;

layout(location = 0) in vec3 vertPosition;
layout(location = 1) in vec3 vertNormal;
layout(location = 2) in vec3 vertTangent;
layout(location = 3) in vec2 vertUV;

layout(location = 0) out vec2 FragUV;
layout(location = 1) out vec3 WorldPosition;
layout(location = 2) out mat3 TBN;

layout(set = 1, binding = 1) uniform sampler2D TransmittanceLUT;
layout(set = 1, binding = 2) uniform sampler2D IrradianceLUT;
layout(set = 1, binding = 3) uniform sampler3D InscatteringLUT;

void main()
{
    dmat4 WorldMatrix = GetWorldMatrix(PushConstants.Orientation, PushConstants.Offset);

    dmat3 mNormal = transpose(dmat3(inverse(WorldMatrix))); // for non-uniform scaled objects, strips the scale information and leaves the rotation vectors
    vec3 Tangent = normalize(vec3(mNormal * vertTangent).xyz);
    vec3 Normal = normalize(vec3(mNormal * vertNormal));
    vec3 Bitangent = normalize(vec3(cross(Normal, Tangent)));

    TBN = mat3(Tangent, Bitangent, Normal);

    dvec4 WorldPositionFP64 = WorldMatrix * dvec4(vertPosition, 1.0); 
    WorldPosition = vec3(WorldPositionFP64.xyz - ubo.CameraPositionFP64.xyz);
    FragUV = vertUV;
    
    gl_Position = vec4(ubo.ViewProjectionMatrix * WorldPositionFP64);
}