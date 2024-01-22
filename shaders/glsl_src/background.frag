#version 460

layout(location = 0) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform samplerCube skybox;

void main() 
{
	outColor = vec4(texture( skybox, -inWorldPos ).xyz, 1.0);
}
