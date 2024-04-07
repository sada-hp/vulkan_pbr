#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "common.glsl"

layout(location = 0) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

layout(binding = 2) uniform samplerCube skybox;

vec3 Scatter(vec3 o, vec3 d)
{
    int steps = 100;
    float stepsize = ATMOSPHERE_DELTA / float(steps);

    vec3 Lm = vec3(0.0);
    vec3 Lr = vec3(0.0);
    vec3 D = vec3(0.0);

    vec3 Scatter = vec3(0.0);
    for (int i = 0; i < steps; i++)
    {
        o += d * stepsize;
    }

    return Scatter;
}

void main() 
{
	vec2 ScreenUV = gl_FragCoord.xy / View.Resolution;
    vec4 ScreenNDC = vec4(2.0 * ScreenUV - 1.0, 1.0, 1.0);
    vec4 ScreenView = inverse(ubo.ProjectionMatrix) * ScreenNDC;
    vec4 ScreenWorld = inverse(ubo.ViewMatrix) * vec4(ScreenView.xy, -1.0, 0.0);
    vec3 RayDirection = normalize(ScreenWorld.xyz);

    //outColor = vec4(Scatter(View.CameraPosition.xyz, RayDirection.xyz), 1.0);
	float sunAmount = max(dot(RayDirection, normalize(ubo.SunDirection.xyz)), 0.0);
	outColor = vec4(texture(skybox, inWorldPos).rgb + 15.0 * vec3(smoothstep(0.9998, 1.0, sunAmount)), 1.0);
}
