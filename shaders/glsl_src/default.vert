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
layout(location = 2) out vec3 Normal;
layout(location = 3) out vec3 Tangent;

layout(binding = 2) uniform sampler2D TransmittanceLUT;
layout(binding = 3) uniform sampler2D IrradianceLUT;
layout(binding = 4) uniform sampler3D InscatteringLUT;

void main()
{
    mat3 mNormal = mat3(transpose(inverse(PushConstants.WorldMatrix)));
    Tangent = normalize((mNormal * vertTangent).xyz);
    Normal = normalize((mNormal * vertNormal).xyz);

    WorldPosition = PushConstants.WorldMatrix * vec4(vertPosition, 1.0);
    FragUV = vertUV;
    //fragNormal = normalize(mat3(PushConstants.WorldMatrix) * vertNormal.xyz);
    gl_Position = ubo.ViewProjectionMatrix * WorldPosition;
}