#version 460
#include "ubo.glsl"
#include "lighting.glsl"

layout(push_constant) uniform constants
{
    mat4 WorldMatrix;
} PushConstants;

layout(location = 0) in vec3 vertPosition;
layout(location = 1) in vec3 vertNormal;
layout(location = 2) in vec3 vertTangent;
layout(location = 3) in vec2 vertUV;

layout(location = 0) out vec2 FragUV;
layout(location = 1) out vec4 WorldPosition;
layout(location = 2) out mat3 TBN;

layout(binding = 2) uniform sampler2D TransmittanceLUT;
layout(binding = 3) uniform sampler2D IrradianceLUT;
layout(binding = 4) uniform sampler3D InscatteringLUT;

void main()
{
    mat3 mNormal = transpose(mat3(inverse(PushConstants.WorldMatrix))); // for non-uniform scaled objects, strips the scale information and leaves the rotation vectors
    vec3 Tangent = normalize((mNormal * vertTangent).xyz);
    vec3 Normal = normalize((mNormal * vertNormal).xyz);
    vec3 Bitangent = normalize(cross(Normal, Tangent));

    TBN = mat3(Tangent, Bitangent, Normal);

    WorldPosition = PushConstants.WorldMatrix * vec4(vertPosition, 1.0);
    FragUV = vertUV;
    
    gl_Position = ubo.ViewProjectionMatrix * WorldPosition;
}